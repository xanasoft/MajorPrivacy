#pragma once
#include "StVariant.h"

#include <map>

//
// Remembers what has already been sent to one client, so that only what changed
// gets sent again.
//
// The process list is a few hundred entries of two dozen fields, and the handle
// and socket lists hanging off each of them are thousands more, nearly all of it
// constant: a name never changes, a path never changes, a handle's target never
// changes. The GUI asks for all of it four times a second and then throws most
// of it away again - see the "we dont have live handle data" shortcuts in the
// client's CProcess::UpdateHandlesUnsafe and CProgramManager::UpdateLibs.
// Building, packing, piping and parsing all of that every round is what this is
// here to stop.
//
// Modelled on TaskExplorer's CVariantCache, which generalised CoreServer's
// SStateCache/UPDATE() from NeoLoader. It knows about ids, field ids and values
// and nothing about processes, programs or any particular field - anything
// expressible as "a set of entries, each a set of fields" can use it, which is
// every list this protocol carries.
//
// ---- what a delta looks like ----
//
// Exactly what a full reply looks like today, minus the fields that did not
// change, plus a list of the entries that went away:
//
//     API_V_PROCESSES     = LIST of INDEX, each { key field, changed fields }
//     API_V_OLD_PROCESSES = LIST of keys
//
// The key travels as an ordinary field inside the entry, named by KeyField -
// API_V_PUID for processes, API_V_PROG_UID for programs and libraries. It is
// stamped in by FinishRound on every entry that travels and never by the caller,
// because a key that went through the change filter would be dropped as
// unchanged on the second round and leave the entry unattributable.
//
// The alternative - a VAR_TYPE_INDEX keyed by the id, which is how TaskExplorer
// does it - cannot carry a 64 bit key. An index entry is a bare uint32 on the
// wire, see the VAR_TYPE_INDEX case in CVariant::MkPayload, which is why that
// version has to hand out session handles and keep a translation table at both
// ends. Carrying the key inside the entry costs nine bytes more per *changed*
// entry and in exchange there is no handle table, no first-sight bookkeeping and
// no stale handles after a reconnect. It also needs no new reader: the client
// already takes the key out of the entry and merges the rest field by field.
//
// ---- identity ----
//
// The key has to identify a thing for as long as anyone might remember it, which
// is not the same as identifying it right now. A pid, a thread id and a handle
// value are all recycled by the operating system, often within seconds. Key a
// delta by one of those and a new thing that inherits a dead thing's number is
// merged into the dead thing's entry - and because a delta carries only what
// changed, every field the two happen to share keeps the dead one's value
// forever. A full resend heals itself on the next round, this does not, which is
// what makes it worth care here and nowhere else.
//
// For processes that key is SProcessUID, the pid together with the creation
// time, which CProcessList already uses as its own map key. For programs and
// libraries it is the program UID. Both belong to us and neither is recycled.
//
// ---- how a round works ----
//
//     Cache.BeginRound();
//     for (auto pThing : Things)
//     {
//         Cache.Write(pThing->GetUID(), API_X_NAME, StVariant(pMemPool, pThing->GetName()));
//         Cache.Write(pThing->GetUID(), API_X_SIZE, StVariant(pMemPool, pThing->GetSize()));
//     }
//     StVariant Removed(pMemPool);
//     StVariant Changed = Cache.FinishRound(Removed);
//
// Every entry still wanted has to be written every round, the fields that never
// change included: mentioning an entry is what keeps it from being reported as
// removed, and the cache works out on its own what of it actually travels. So
// there is no reason to be sparing about what gets written.
//
// ---- what the reader has to do ----
//
// Apply, not replace. A field that is absent has not changed, and an entry that
// is absent has not changed either. That is the whole point, and it means a
// reader that throws its copy away has to say so - see Clear().
//
// ---- lifetime ----
//
// One of these per subscription: per connection and per list, living in
// CServiceCore::SClient, which is made and destroyed with the pipe. The
// connection is the subscription, so there is nothing to expire, nothing to
// guess and no cache token on the wire - which is what the abandoned
// LibraryCacheToken in ServiceCoreAPI.cpp was trying to solve the hard way.
//
// Not thread safe. SClient is found under CServiceCore::m_ClientsMutex and
// carries its own Mutex for its contents; the cache belongs under that one.
//

template <class TKey>
class CVariantCacheT
{
public:
	//
	// KeyField is the field id the key travels under. It must be one the entries
	// do not otherwise use, see the ASSERT in Write().
	//
	CVariantCacheT(FW::AbstractMemPool* pMemPool, uint32 KeyField)
		: m_pMemPool(pMemPool), m_KeyField(KeyField)
		, m_Scopes(pMemPool), m_Changed(pMemPool) {}

	//
	// Starts a round. Every entry still wanted must be written before
	// FinishRound, or it will be reported as removed.
	//
	// ---- scopes ----
	//
	// A round can cover part of the cache instead of all of it, which is what the
	// per program lists need. One connection asks for the handles of program 500,
	// then of program 900, then of 500 again; all of it belongs in one cache,
	// because the entries are keyed by an id that is unique across the machine
	// and there is nothing to be gained by splitting them up. But a round for 500
	// must not conclude that 900's handles have all been closed merely because it
	// did not mention them.
	//
	// So an entry remembers which scope it was written under, and a round touches
	// only its own: it marks only those entries unseen and reports only those as
	// removed. Scope 0 is everything, which is what the system wide lists use.
	//
	// The alternative - a whole cache per program - is worse: a map of caches to
	// create, find and eventually expire, and one lifetime rule per list.
	//
	// The entries are bucketed by scope rather than tagged with one, and that is
	// not a detail. A round is opened once per program, so anything here that
	// walks the whole cache is walked once per program as well - with a few
	// hundred programs and tens of thousands of handles between them that is
	// millions of nodes per refresh, four times a second, on the one thread that
	// serves the connection. It is what makes the difference between this paying
	// for itself and costing more than the full resend it replaced.
	//
	// For the same reason nothing is marked unseen here: the scope carries a round
	// counter and an entry remembers the round it was last mentioned in, so
	// starting a round is one increment however many entries the scope holds.
	//
	void BeginRound(uint64 Scope = 0)
	{
		m_Changed.Clear();
		m_Scope = Scope;
		m_pScope = m_Scopes.GetOrAddValue(Scope).GetPtr();
		m_pLastEntry = nullptr;
		++m_pScope->RoundNr;
		m_bInRound = true;
	}

	//
	// Says an entry is still present without offering any field for it, and
	// returns whether the reader already has it.
	//
	// That return is what lets a caller skip the fields of an entry that cannot
	// change - a handle's path and owner are fixed when it is opened - and turn
	// the whole of its per round cost into one lookup. Writing them anyway is not
	// wrong, only wasteful: a value has to be built before it can be compared, and
	// for tens of thousands of entries that is tens of thousands of variants built
	// and thrown away every round.
	//
	bool Keep(const TKey& Id)
	{
		ASSERT(m_bInRound);

		SEntry& Entry = GetEntry(Id);
		Entry.SeenRound = m_pScope->RoundNr;
		return Entry.bSent;
	}

	//
	// Records one field of one entry. It reaches the output only if it differs
	// from what was last sent, and mentioning an entry at all is what keeps it
	// from being reported as removed - so a field that never changes still has to
	// be written every round.
	//
	void Write(const TKey& Id, uint32 Field, const StVariant& Value)
	{
		ASSERT(m_bInRound);

		//
		// The key is not a field the caller owns, FinishRound stamps it into
		// every entry that travels. Writing it here would put it through the
		// change filter, where it would be dropped as unchanged from the second
		// round on and leave the reader unable to tell which entry it holds.
		//
		ASSERT(Field != m_KeyField);
		if (Field == m_KeyField)
			return;

		//
		// An entry belongs to the scope it was written under and does not move: a
		// handle stays with the process that opened it for as long as it exists.
		//
		SEntry& Entry = GetEntry(Id);
		Entry.SeenRound = m_pScope->RoundNr;

		//
		// The comparison is the whole mechanism, and it works on any type because
		// CVariant::Compare compares its own payload and nothing else - this
		// class never learns what a field means. For anything that came out of a
		// StVariantWriter::Finish() the payload is already flat, so even a whole
		// sub list compares as a memcmp rather than being serialised again.
		//
		StVariant* pOld = Entry.Fields.FindValue(Field).GetPtr();
		if (pOld && pOld->Compare(Value) == 0)
			return;		// unchanged, so it does not travel

		//
		// StVariant::operator= clears first and clones anything derived - a
		// variant that merely points into a packet buffer would otherwise be
		// compared against freed memory next round.
		//
		//
		// GetOrAddValue and not operator[], which does not compile for a Table - it
		// returns SetValuePtr whole where Map returns its first. Left alone, being
		// shared with the driver.
		//
		Entry.Fields.GetOrAddValue(Field) = Value;
		m_Changed.GetOrAddValue(Id).GetPtr()->GetOrAddValue(Field) = Value;
	}

	//
	// The same, for a value that is not a variant yet.
	//
	// Nearly everything a round writes is the same as it was last time, and the
	// overload above cannot find that out without being handed a variant - which
	// means building one, for every field of every entry, every round, only to
	// compare it and drop it again. For a few hundred processes of two dozen
	// fields that is most of what a round costs, and every one of the strings
	// among them is an allocation.
	//
	// These compare against the bytes that are already here and build nothing at
	// all unless the value really did change, which it almost never has.
	//
	void Write(const TKey& Id, uint32 Field, const uint64& Value)		{ WriteRaw(Id, Field, VAR_TYPE_UINT, &Value, sizeof(uint64)); }
	void Write(const TKey& Id, uint32 Field, const uint32& Value)		{ WriteRaw(Id, Field, VAR_TYPE_UINT, &Value, sizeof(uint32)); }
	void Write(const TKey& Id, uint32 Field, const std::wstring& Value)	{ WriteRaw(Id, Field, VAR_TYPE_UNICODE, Value.c_str(), Value.length() * sizeof(wchar_t)); }
	void Write(const TKey& Id, uint32 Field, const std::string& Value)	{ WriteRaw(Id, Field, VAR_TYPE_ASCII, Value.c_str(), Value.length()); }

	//
	// Files a whole entry, as its own ToVariant built it, field by field.
	//
	// For the lists whose entries are written by a virtual WriteIVariant with one
	// override per subclass - programs, libraries - where writing the field list out
	// again here would mean writing it once per subclass and keeping all of them in
	// step with the originals forever. This way there is only ever one field list.
	//
	// The key field is skipped, being the cache's own; FinishRound stamps it back in.
	// The entry has to be an index, which is what SVarWriteOpt::eIndex produces.
	//
	// What this does not save is the building of the entry: the whole thing is
	// serialised every round and only then compared. For an entry that is a couple
	// of dozen small fields that is cheap next to putting it on the wire; for one
	// that walks a few thousand sub entries it is not, which is why processes write
	// their fields directly and their handles and sockets have caches of their own.
	//
	void WriteEntry(const TKey& Id, const StVariant& Entry)
	{
		ASSERT(m_bInRound);

		//
		// Mentioned even if every one of its fields turns out unchanged, or it would
		// be reported as removed.
		//
		Keep(Id);

		FW::CVariantReader::ReadRawIndex(Entry, [&](uint32 Field, const FW::CVariant& Data) {
			if (Field != m_KeyField)
				Write(Id, Field, *(StVariant*)&Data);
		});
	}

	//
	// Records a field that travels whether or not it looks like what was sent last
	// time, and is not remembered afterwards.
	//
	// For values that are already deltas of their own, where two rounds really can
	// produce byte for byte the same thing: a transfer rate that goes back to zero,
	// a state that returns to what it was before. Dropping the second one as
	// unchanged would leave the reader holding the value in between, forever.
	//
	// It still counts as mentioning the entry, so a process whose own fields did
	// not change but whose handles did still appears in the round.
	//
	void WriteAlways(const TKey& Id, uint32 Field, const StVariant& Value)
	{
		ASSERT(m_bInRound);
		ASSERT(Field != m_KeyField);
		if (Field == m_KeyField)
			return;

		SEntry& Entry = GetEntry(Id);
		Entry.SeenRound = m_pScope->RoundNr;

		//
		// Deliberately not stored in Entry.Fields: it must never be compared against,
		// and it must not make the entry look changed next round on its own account.
		//
		m_Changed.GetOrAddValue(Id).GetPtr()->GetOrAddValue(Field) = Value;
	}

	//
	// Ends the round. Returns the changed entries as a list, and fills Removed
	// with a list of the keys the round did not mention.
	//
	StVariant FinishRound(StVariant& Removed)
	{
		//
		// Anything the round did not mention is gone. Reported once and then
		// forgotten, so a reader is told exactly once and a key that comes back
		// starts fresh.
		//
		StVariantWriter RemovedList(m_pMemPool);
		RemovedList.BeginList();
		for (auto I = m_pScope->Entries.begin(); I != m_pScope->Entries.end(); )
		{
			if (I.Value().SeenRound == m_pScope->RoundNr)
				++I;
			else
			{
				//
				// Only if it ever travelled. An entry that was Keep()ed and never
				// written has never been sent, so the reader has not heard of it
				// and telling it something was removed would be a lie.
				//
				if (I.Value().bSent)
					WriteKey(RemovedList, I.Key());
				I = m_pScope->Entries.erase(I);
			}
		}
		Removed = RemovedList.Finish();

		StVariantWriter ChangedList(m_pMemPool);
		ChangedList.BeginList();
		for (auto I = m_Changed.begin(); I != m_Changed.end(); ++I)
		{
			StVariantWriter Entry(m_pMemPool);
			Entry.BeginIndex();

			//
			// The key first and always, whether it changed or not - it is what
			// the entry is, not something about it.
			//
			WriteKey(Entry, m_KeyField, I.Key());

			for (auto J = I.Value().begin(); J != I.Value().end(); ++J)
				Entry.WriteVariant(J.Key(), J.Value());

			ChangedList.WriteVariant(Entry.Finish());

			SEntry* pEntry = m_pScope->Entries.FindValue(I.Key()).GetPtr();
			if (pEntry)
				pEntry->bSent = true;
		}

		m_Changed.Clear();
		m_bInRound = false;
		m_pLastEntry = nullptr;

		//
		// A scope with nothing left in it is dropped, or a machine that has started
		// and stopped ten thousand programs would carry ten thousand empty buckets
		// for the life of the connection.
		//
		StVariant Changed = ChangedList.Finish();
		if (m_pScope->Entries.IsEmpty() && m_pScope->Token == 0 && m_Scope != 0)
			m_Scopes.Remove(m_Scope);
		m_pScope = nullptr;
		return Changed;
	}

	//
	// Forgets everything, so the next round sends every field of every entry. For
	// when a reader has lost its copy and says so.
	//
	void Clear()
	{
		m_Scopes.Clear();
		m_Changed.Clear();
		m_pScope = nullptr;
		m_pLastEntry = nullptr;
		m_bInRound = false;
	}

	//
	// With a scope, forgets only that part - a viewer that has just opened the
	// handle tab of one program needs everything about that program and nothing
	// about the others, which are very likely still on screen.
	//
	void Clear(uint64 Scope)
	{
		m_Scopes.Remove(Scope);
		m_Changed.Clear();
		m_pLastEntry = nullptr;
	}

	//
	// A number of the caller's own, kept per scope and for the life of the cache.
	//
	// For a collection that can say in one comparison whether it has changed at
	// all - a list that counts its insertions and removals, say - this is where to
	// keep the last answer. When it matches there is no need to open a round: not
	// opening one leaves every entry of that scope mentioned as it was, which is
	// exactly right when nothing about them can have changed.
	//
	uint64& ScopeToken(uint64 Scope)		{ return m_Scopes.GetOrAddValue(Scope).GetPtr()->Token; }

	//
	// The same idea one level down: a number of the caller's own kept against a
	// single entry, for a field that is expensive to produce and rarely changes.
	// Where the field is cheap the comparison in Write is cheaper than this.
	//
	// Only valid inside a round, and it goes when the entry does - so an entry
	// that was swept and came back is told everything again, as it should be.
	//
	uint64& EntryToken(const TKey& Id)		{ return GetEntry(Id).Token; }

	//
	// Whether this cache has ever sent anything. A cache lives as long as one
	// client's connection, so an empty one means that client holds nothing of
	// ours - which is the one thing the reader cannot always work out for itself
	// when the transport reconnects underneath it.
	//
	bool IsEmpty() const
	{
		for (auto I = m_Scopes.begin(); I != m_Scopes.end(); ++I) {
			if (!I.Value().Entries.IsEmpty())
				return false;
		}
		return true;
	}

	size_t GetEntryCount() const
	{
		size_t Count = 0;
		for (auto I = m_Scopes.begin(); I != m_Scopes.end(); ++I)
			Count += I.Value().Entries.Count();
		return Count;
	}

protected:

	struct SEntry
	{
		//
		// Handed the pool so its table allocates from the connection's own, see
		// the DefaultContainer policy on the tables below.
		//
		SEntry(FW::AbstractMemPool* pMem = nullptr) : Fields(pMem) {}

		//
		// A hash and not a tree. This is looked at once per field per entry per
		// round - the single most repeated thing the cache does - and a tree pays
		// a pointer chase per level for it.
		//
		FW::Table<uint32, StVariant>	Fields;

		//
		// The round this entry was last mentioned in. Compared against the scope's
		// counter rather than cleared at the start of every round, see BeginRound.
		//
		uint64						SeenRound = 0;

		//
		// Whether the reader has ever been told about this entry, see FinishRound.
		//
		bool						bSent = false;

		//
		// The caller's, see EntryToken.
		//
		uint64						Token = 0;
	};

	//
	// One bucket per scope - zero for the system wide lists, the program UID for
	// the per program ones - so that a round costs what its own scope holds and
	// not what the whole cache holds. See BeginRound.
	//
	struct SScope
	{
		SScope(FW::AbstractMemPool* pMem = nullptr) : Entries(pMem) {}

		//
		// A hash as well, and for the same reason: this is looked at once per
		// entry per round, which for the handles is tens of thousands of times.
		//
		FW::Table<TKey, SEntry, FW::DefaultHasher<TKey>, FW::DefaultContainer<SEntry> >	Entries;
		uint64						RoundNr = 0;

		//
		// The caller's, see ScopeToken.
		//
		uint64						Token = 0;
	};
	//
	// Where the typed overloads above end up. The comparison is type, then length,
	// then the bytes themselves - which is what CVariant::Compare works out to for
	// anything that is not a container, only without a variant having to exist
	// first.
	//
	//
	// Finds an entry, remembering the last one.
	//
	// A caller writes all of one entry's fields together before moving to the
	// next, so without this a process of two dozen fields is two dozen lookups of
	// the same key - which over a few hundred processes is the bulk of what is
	// left to look up at all.
	//
	SEntry& GetEntry(const TKey& Id)
	{
		if (m_pLastEntry && m_LastId == Id)
			return *m_pLastEntry;

		SEntry& Entry = *m_pScope->Entries.GetOrAddValue(Id).GetPtr();
		m_LastId = Id;
		m_pLastEntry = &Entry;
		return Entry;
	}

	void WriteRaw(const TKey& Id, uint32 Field, uint8 Type, const void* pData, size_t Size)
	{
		ASSERT(m_bInRound);
		ASSERT(Field != m_KeyField);
		if (Field == m_KeyField)
			return;

		SEntry& Entry = GetEntry(Id);
		Entry.SeenRound = m_pScope->RoundNr;

		StVariant* pOld = Entry.Fields.FindValue(Field).GetPtr();
		if (pOld
		 && pOld->GetType() == Type
		 && pOld->GetSize() == Size
		 && (Size == 0 || memcmp(pOld->GetData(), pData, Size) == 0))
			return;		// unchanged, and nothing had to be built to find that out

		StVariant Value(m_pMemPool, (const byte*)pData, Size, (StVariant::EType)Type);
		//
		// GetOrAddValue and not operator[], which does not compile for a Table - it
		// returns SetValuePtr whole where Map returns its first. Left alone, being
		// shared with the driver.
		//
		Entry.Fields.GetOrAddValue(Field) = Value;
		m_Changed.GetOrAddValue(Id).GetPtr()->GetOrAddValue(Field) = Value;
	}

	//
	// The only thing the key types differ in: how a key is written. Plain
	// overloads, no specialisation - a name is its own key and needs no
	// translation, and neither does an id now that it travels as a value.
	//
	static void WriteKey(StVariantWriter& Writer, uint32 Field, const uint64& Id)		{ Writer.Write(Field, Id); }
	static void WriteKey(StVariantWriter& Writer, uint32 Field, const FW::StringW& Id)	{ Writer.WriteEx(Field, Id); }

	static void WriteKey(StVariantWriter& Writer, const uint64& Id)						{ Writer.Write(Id); }
	static void WriteKey(StVariantWriter& Writer, const FW::StringW& Id)				{ Writer.WriteEx(Id); }

	FW::AbstractMemPool*			m_pMemPool;
	uint32							m_KeyField;

	FW::Table<uint64, SScope, FW::DefaultHasher<uint64>, FW::DefaultContainer<SScope> >	m_Scopes;

	//
	// The bucket this round is for, valid only between BeginRound and FinishRound.
	//
	SScope*							m_pScope = nullptr;

	//
	// The entry the last write landed on, see GetEntry. Dropped whenever anything
	// could have moved it.
	//
	TKey							m_LastId = TKey();
	SEntry*							m_pLastEntry = nullptr;

	//
	// This round's changes, collected as they are written and turned into a
	// variant at the end. Building it incrementally is not possible, a
	// StVariantWriter is written once and finished.
	//
	FW::Table<TKey, FW::Table<uint32, StVariant>, FW::DefaultHasher<TKey>, FW::DefaultContainer<FW::Table<uint32, StVariant> > >	m_Changed;

	bool							m_bInRound = false;
	uint64							m_Scope = 0;
};

typedef CVariantCacheT<uint64>			CVariantCache;
//
// FW::StringW and not std::wstring: the tables hash their keys, and the default
// hasher works on the bytes of the key object itself - which for a std::wstring
// is a pointer or a small buffer, not the characters. FW::StringW has a hasher
// of its own that does the right thing.
//
typedef CVariantCacheT<FW::StringW>	CVariantCacheStr;
