#include "pch.h"
#include "ListReplies.h"
#include "../ServiceCore.h"
#include "../Processes/ProcessList.h"
#include "../Programs/ProgramManager.h"

//
// Turn this on to find out where a round actually goes, rather than guessing.
//
// It prints one line every two seconds with the microseconds spent in each part
// of a process round and how many things were in it, to OutputDebugString - so
// DebugView, or a debugger attached to the agent, will show it. Leave it off in
// anything shipped: it costs two QueryPerformanceCounter calls per process per
// round on top of everything else.
//
#ifdef _DEBUG
//#define CACHE_PROFILE
#endif

#ifdef CACHE_PROFILE

#include "../../Library/Common/DbgHelp.h"

struct SRoundProfile
{
	uint64	Fields = 0;		// the plain fields, which now compare without building
	uint64	SignInfo = 0;	// CImageSignInfo copied, serialised and compared
	uint64	Services = 0;	// the service name set copied and turned into a list
	uint64	Enclave = 0;	// the enclave guid turned into a variant
	uint64	Handles = 0;
	uint64	Sockets = 0;
	uint64	Finish = 0;		// the removal sweep and building the reply
	uint64	Total = 0;

	uint64	Rounds = 0;
	uint64	Processes = 0;
	uint64	HandleCount = 0;
	uint64	SocketCount = 0;

	uint64	LastReport = 0;
};

static SRoundProfile g_Profile;

//
// GetUSTickCount wraps at 32 bits, so the difference is taken there too.
//
#define PROF_BEGIN(v)		const uint64 v = GetUSTickCount()
#define PROF_END(f, v)		g_Profile.f += (GetUSTickCount() - (v)) & 0xffffffff
#define PROF_COUNT(f, n)	g_Profile.f += (n)
#else
#define PROF_BEGIN(v)
#define PROF_END(f, v)
#define PROF_COUNT(f, n)
#endif

/////////////////////////////////////////////////////////////////////////////
// Handles and Sockets
//

//
// These hang off a process and stay with it for their whole life, so they are
// cached under the scope of the owning process: a round for one process marks
// only that process's entries unseen and reports only those as removed, and a
// round that does not mention process 900 does not conclude that its handles
// have all been closed. See CVariantCacheT::BeginRound.
//
// A handle is two fields, both of which are fixed the moment it is opened, so
// after the first round a process with two thousand handles sends none of them.
// That case - thousands of entries, none of them changing, resent four times a
// second - is the whole reason any of this exists.
//

static StVariant BuildHandleDelta(CVariantCache& Cache, const CProcessPtr& pProcess, uint64 Puid, StVariant& Removed, FW::AbstractMemPool* pMemPool)
{
	//
	// Nothing opened and nothing closed since the last round means there is
	// nothing to say: a handle's path and owner are fixed when it is opened, so
	// the only thing that can ever make this list differ is the list itself
	// changing. One comparison then stands in for walking every handle the
	// process has, which for most processes most of the time is all of them.
	//
	// Not opening a round is what makes this safe: the entries keep the round they
	// were last mentioned in, so none of them is swept.
	//
	uint64& LastVersion = Cache.ScopeToken(Puid);
	const uint64 Version = pProcess->GetHandleListVersion();
	if (LastVersion == Version)
		return StVariant();
	LastVersion = Version;

	Cache.BeginRound(Puid);

	PROF_COUNT(HandleCount, pProcess->GetHandleCount());

	pProcess->EnumHandles([&](const CHandlePtr& pHandle) {

		if (!pHandle->HasFileName())
			return; // skip unnamed handles, and do so before the cache learns of them

		const uint64 Uid = pHandle->GetObjectUid();

		//
		// Both of a handle's fields are fixed when it is opened - there is no way to
		// rename one or move it to another process - so once the reader has it there
		// is nothing left to compare and saying it is still there is the whole of the
		// work. Building the two values again every round only to find them unchanged
		// is what this skips, and with tens of thousands of handles that is the
		// difference between a refresh costing nothing and costing more than the full
		// resend it replaced.
		//
		if (Cache.Keep(Uid))
			return;

		Cache.Write(Uid, API_V_ACCESS_PATH, pHandle->GetFileName());
		Cache.Write(Uid, API_V_PID, pHandle->GetProcessId());
	});

	return Cache.FinishRound(Removed);
}

static StVariant BuildSocketDelta(CVariantCache& Cache, const CProcessPtr& pProcess, uint64 Puid, StVariant& Removed, FW::AbstractMemPool* pMemPool)
{
	Cache.BeginRound(Puid);

	PROF_COUNT(SocketCount, pProcess->GetSocketCount());

	pProcess->EnumSockets([&](const CSocketPtr& pSocket) {

		const uint64 Uid = pSocket->GetObjectUid();

		//
		// The half of a socket that holds still: what it is, where it goes, and whose
		// it is. Most of that is known the moment the socket is seen, but the owning
		// service and the scope ids arrive later and all at once, in InitStaticDataEx
		// - so the token follows that rather than simply meaning sent or not sent,
		// and the group is written again when it turns up.
		//
		// Nought means never sent, so a socket new to the cache always gets it.
		//
		const uint64 StaticVersion = pSocket->HasStaticDataEx() ? 2 : 1;
		uint64& LastStatic = Cache.EntryToken(Uid);
		if (LastStatic != StaticVersion)
		{
			LastStatic = StaticVersion;

			Cache.Write(Uid, API_V_SOCK_TYPE, pSocket->GetProtocolType());
			Cache.Write(Uid, API_V_SOCK_LADDR, pSocket->GetLocalAddress().ToString());
			Cache.Write(Uid, API_V_SOCK_LPORT, (uint32)pSocket->GetLocalPort());
			Cache.Write(Uid, API_V_SOCK_RADDR, pSocket->GetRemoteAddress().ToString());
			Cache.Write(Uid, API_V_SOCK_RPORT, (uint32)pSocket->GetRemotePort());
			Cache.Write(Uid, API_V_SOCK_LSCOPE, pSocket->GetLocalScopeId()); // Ipv6
			Cache.Write(Uid, API_V_SOCK_RSCOPE, pSocket->GetRemoteScopeId()); // Ipv6

			Cache.Write(Uid, API_V_PID, pSocket->GetProcessId());
			Cache.Write(Uid, API_V_SERVICE_TAG, pSocket->GetOwnerServiceName());

			Cache.Write(Uid, API_V_CREATE_TIME, pSocket->GetCreateTimeStamp()); // in ms
		}

		//
		// And the half that does not: the state, what it has moved, and the name the
		// remote address resolved to - which is filled in whenever the lookup gets
		// round to it, so it has to be looked at every round.
		//
		Cache.Write(Uid, API_V_SOCK_STATE, pSocket->GetState());

		CHostNamePtr pRemoteHost = pSocket->GetRemoteHostName();
		Cache.Write(Uid, API_V_SOCK_RHOST, pRemoteHost ? pRemoteHost->ToString() : std::wstring());

		Cache.Write(Uid, API_V_SOCK_LAST_NET_ACT, pSocket->GetLastActivity());

		Cache.Write(Uid, API_V_SOCK_UPLOAD, pSocket->GetUpload());
		Cache.Write(Uid, API_V_SOCK_DOWNLOAD, pSocket->GetDownload());
		Cache.Write(Uid, API_V_SOCK_UPLOADED, pSocket->GetUploaded());
		Cache.Write(Uid, API_V_SOCK_DOWNLOADED, pSocket->GetDownloaded());
	});

	return Cache.FinishRound(Removed);
}


/////////////////////////////////////////////////////////////////////////////
// Processes
//

//
// WARNING: this field list is the same one as in CProcess::ToVariant, which is
// still used on its own by SVC_API_GET_PROCESS for a single process. A field
// added to one has to be added to the other, or it will reach a viewer that
// opened one process and never reach the list.
//
// The fields are written unconditionally, all of them, every round. Nearly all
// of them are constant for the life of a process - the name, the path, the
// command line, the signature, the user - so after the first round the cache
// sends none of them, which is why there is no reason to be sparing here.
//
// Note that a field can change but not vanish: the cache only ever hears about
// fields that are written, so one that stops being written keeps its last value
// on the reader forever. Write them unconditionally, as here.
//

StVariant BuildProcessDelta(SListCaches& Caches, StVariant& Removed, FW::AbstractMemPool* pMemPool)
{
	std::map<SProcessUID, CProcessPtr> Processes = theCore->ProcessList()->List();

	PROF_BEGIN(uTotal);

	Caches.Processes.BeginRound();

	for (const auto& I : Processes)
	{
		PROF_COUNT(Processes, 1);
		//
		// The key the reader knows this entry by. It is already the key of the
		// collector's own map, and the cache stamps it into every entry it emits
		// - it must not be written as a field here, see CVariantCacheT::Write.
		//
		const uint64 Puid = I.first.Get();

		const CProcessPtr& pProcess = I.second;

		PROF_BEGIN(uFields);

		Caches.Processes.Write(Puid, API_V_PID, pProcess->GetProcessId());
		Caches.Processes.Write(Puid, API_V_CREATE_TIME, pProcess->GetCreationTime());
		Caches.Processes.Write(Puid, API_V_PARENT_PID, pProcess->GetParentId());
		Caches.Processes.Write(Puid, API_V_CREATOR_PID, pProcess->GetCreatorId());

		Caches.Processes.Write(Puid, API_V_NAME, pProcess->GetName());
		Caches.Processes.Write(Puid, API_V_FILE_NT_PATH, pProcess->GetNtFilePath());
		Caches.Processes.Write(Puid, API_V_CMD_LINE, pProcess->GetCommandLine());

		Caches.Processes.Write(Puid, API_V_STATUS, (uint32)(pProcess->IsMarkedForRemoval() ? MP_PROCESS_FLAG_TERMINATED : 0));

		PROF_END(Fields, uFields);

		PROF_BEGIN(uEnclave);
		//
		// Spelt out as a StVariant: ToVariant hands back a plain CVariant, which has
		// conversion operators to the integer types and would otherwise be an
		// ambiguous match against the value overloads of Write.
		//
		Caches.Processes.Write(Puid, API_V_ENCLAVE, StVariant(pProcess->GetEnclave().ToVariant(false, pMemPool)));
		PROF_END(Enclave, uEnclave);

		PROF_BEGIN(uFields2);
		Caches.Processes.Write(Puid, API_V_KPP_STATE, pProcess->GetSecState());

		Caches.Processes.Write(Puid, API_V_FLAGS, pProcess->GetFlags());
		Caches.Processes.Write(Puid, API_V_SFLAGS, pProcess->GetSecFlags());

		PROF_END(Fields, uFields2);

		PROF_BEGIN(uSignInfo);
		//
		// Unlike the fields above this one cannot be compared without first being
		// produced, and producing it means copying a CImageSignInfo under the lock
		// and serialising it. For a process whose signature has not been looked at
		// again since the last round - which is nearly all of them, nearly always -
		// that is all wasted, so the version stands in for the comparison.
		//
		// Held one above the real version so that nought means never sent, and an
		// entry new to the cache is told about it whether it has changed or not.
		//
		// One token carries two of these, the signature in the high half and the
		// service list in the low one. Neither will see four billion changes, and
		// a second token per entry would be eight more bytes on every entry the
		// cache holds - including the handles, which have no use for one.
		//
		uint64& Tokens = Caches.Processes.EntryToken(Puid);

		const uint32 SignVersion = (uint32)pProcess->GetSignInfoVersion() + 1;
		if ((uint32)(Tokens >> 32) != SignVersion)
		{
			Tokens = ((uint64)SignVersion << 32) | (uint32)Tokens;
			Caches.Processes.Write(Puid, API_V_SIGN_INFO, pProcess->GetSignInfo().ToVariant(SVarWriteOpt(), pMemPool));
		}
		PROF_END(SignInfo, uSignInfo);

		PROF_BEGIN(uFields3);
		Caches.Processes.Write(Puid, API_V_NUM_IMG, pProcess->GetNumberOfImageLoads());
		Caches.Processes.Write(Puid, API_V_NUM_MS_IMG, pProcess->GetNumberOfMicrosoftImageLoads());
		Caches.Processes.Write(Puid, API_V_NUM_AV_IMG, pProcess->GetNumberOfAntimalwareImageLoads());
		Caches.Processes.Write(Puid, API_V_NUM_V_IMG, pProcess->GetNumberOfVerifiedImageLoads());
		Caches.Processes.Write(Puid, API_V_NUM_S_IMG, pProcess->GetNumberOfSignedImageLoads());
		Caches.Processes.Write(Puid, API_V_NUM_U_IMG, pProcess->GetNumberOfUntrustedImageLoads());

		PROF_END(Fields, uFields3);

		PROF_BEGIN(uServices);
		//
		// The same, in the low half of the token.
		//
		const uint32 SvcVersion = (uint32)pProcess->GetServiceListVersion() + 1;
		if ((uint32)Tokens != SvcVersion)
		{
			Tokens = (Tokens & 0xFFFFFFFF00000000ull) | SvcVersion;
			Caches.Processes.Write(Puid, API_V_SERVICES, StVariant(pMemPool, pProcess->GetServices()));
		}
		PROF_END(Services, uServices);

		PROF_BEGIN(uFields4);

		Caches.Processes.Write(Puid, API_V_APP_SID, pProcess->GetAppContainerSid());
		Caches.Processes.Write(Puid, API_V_APP_NAME, pProcess->GetAppContainerName());
		Caches.Processes.Write(Puid, API_V_PACK_NAME, pProcess->GetAppPackageName());

		Caches.Processes.Write(Puid, API_V_USER_SID, pProcess->GetUserSid());

		Caches.Processes.Write(Puid, API_V_SOCK_LAST_NET_ACT, pProcess->GetLastNetActivity());

		Caches.Processes.Write(Puid, API_V_SOCK_UPLOAD, pProcess->GetUpload());
		Caches.Processes.Write(Puid, API_V_SOCK_DOWNLOAD, pProcess->GetDownload());
		Caches.Processes.Write(Puid, API_V_SOCK_UPLOADED, pProcess->GetUploaded());
		Caches.Processes.Write(Puid, API_V_SOCK_DOWNLOADED, pProcess->GetDownloaded());

		//
		// The handle and socket lists ride inside the process entry, as deltas of
		// their own. They go through the process cache like any other field, which
		// is what makes a process whose handles changed but whose own fields did
		// not appear in the round at all.
		//
		// They go in with WriteAlways and not Write, because they are deltas already:
		// two rounds can produce the same sub delta byte for byte - an idle socket
		// whose transfer rate falls back to zero does it constantly - and the change
		// filter would swallow the second one and strand the reader on the value in
		// between. WriteAlways still counts as mentioning the process, so one whose
		// own fields did not change but whose handles did still appears in the round.
		//
		// An empty sub delta, which is the normal case, is not written at all. GetSize
		// and not Count, which would parse the payload just to learn it is empty.
		//
		PROF_END(Fields, uFields4);

		PROF_BEGIN(uHandles);
		StVariant HandlesRemoved(pMemPool);
		StVariant Handles = BuildHandleDelta(Caches.Handles, pProcess, Puid, HandlesRemoved, pMemPool);
		if (Handles.GetSize() > 0)
			Caches.Processes.WriteAlways(Puid, API_V_HANDLES, Handles);
		if (HandlesRemoved.GetSize() > 0)
			Caches.Processes.WriteAlways(Puid, API_V_OLD_HANDLES, HandlesRemoved);

		PROF_END(Handles, uHandles);

		PROF_BEGIN(uSockets);
		StVariant SocketsRemoved(pMemPool);
		StVariant Sockets = BuildSocketDelta(Caches.Sockets, pProcess, Puid, SocketsRemoved, pMemPool);
		if (Sockets.GetSize() > 0)
			Caches.Processes.WriteAlways(Puid, API_V_SOCKETS, Sockets);
		if (SocketsRemoved.GetSize() > 0)
			Caches.Processes.WriteAlways(Puid, API_V_OLD_SOCKETS, SocketsRemoved);
		PROF_END(Sockets, uSockets);
	}

	PROF_BEGIN(uFinish);
	StVariant Changed = Caches.Processes.FinishRound(Removed);

	//
	// A process that is gone takes its handles and sockets with it. Nobody will
	// ever run a round for that scope again, so the sub caches would hold their
	// entries for the life of the connection if they were not dropped here.
	//
	StVariantReader(Removed).ReadRawList([&](const FW::CVariant& vData) {
		const uint64 Puid = ((StVariant*)&vData)->To<uint64>(0);
		Caches.Handles.Clear(Puid);
		Caches.Sockets.Clear(Puid);
	});
	PROF_END(Finish, uFinish);

	PROF_END(Total, uTotal);
	PROF_COUNT(Rounds, 1);

#ifdef CACHE_PROFILE
	{
		SRoundProfile& P = g_Profile;
		const uint64 uNow = GetUSTickCount();
		if (((uNow - P.LastReport) & 0xffffffff) > 2000000)
		{
			const uint64 R = P.Rounds ? P.Rounds : 1;
			DbgPrint(L"MP delta: %llu rounds | per round: %llu proc %llu hnd %llu sok | us/round: fields %llu sign %llu svc %llu encl %llu hnd %llu sok %llu fin %llu = %llu\n",
				P.Rounds, P.Processes / R, P.HandleCount / R, P.SocketCount / R,
				P.Fields / R, P.SignInfo / R, P.Services / R, P.Enclave / R,
				P.Handles / R, P.Sockets / R, P.Finish / R, P.Total / R);
			P = SRoundProfile();
			P.LastReport = uNow;
		}
	}
#endif

	return Changed;
}


/////////////////////////////////////////////////////////////////////////////
// Programs and Libraries
//

//
// Unlike the processes above these do not write out a field list of their own.
// A program entry is written by a virtual WriteIVariant with an override per
// subclass - CProgramFile, CWindowsService, CAppPackage, CProgramSet and the
// rest - so repeating it here would mean repeating it once per subclass and
// keeping every one of them in step with the original forever. Instead the
// entry is built as it always was and handed to the cache whole, which files it
// field by field. See CVariantCacheT::WriteEntry.
//
// That leaves the entry being built every round and only then compared, which is
// affordable here: a program is a couple of dozen small fields and its rules,
// not the few thousand sub entries a process carries. What it buys is that the
// wire and the reader see nothing at all in the normal round, and that these two
// functions cannot fall out of step with what they serialise.
//
// Both are keyed by the program UID, which is ours and is not recycled, and both
// entries already carry it as API_V_PROG_UID - the cache skips it on the way in
// and stamps it back on the way out.
//

StVariant BuildProgramDelta(CVariantCache& Cache, StVariant& Removed, FW::AbstractMemPool* pMemPool)
{
	// todo restrict to a certain type by argument

	Cache.BeginRound();

	for (const auto& I : theCore->ProgramManager()->GetItems())
		Cache.WriteEntry(I.first, I.second->ToVariant(SVarWriteOpt(), pMemPool));

	return Cache.FinishRound(Removed);
}

//
// A library is a UID, a path and a signature, and not one of them ever changes
// once it is enlisted - the client says as much, it refuses to re-read one. So
// after the first round this sends nothing at all, which is what the abandoned
// LibraryCacheToken in ServiceCoreAPI.cpp was reaching for.
//

StVariant BuildLibraryDelta(CVariantCache& Cache, StVariant& Removed, FW::AbstractMemPool* pMemPool)
{
	Cache.BeginRound();

	for (const auto& I : theCore->ProgramManager()->GetLibraries())
	{
		//
		// WriteEntry has to build the entry before it can compare it, and on a
		// machine with thousands of libraries loaded that was the single most
		// expensive thing any request did - it showed up as CProgramLibrary::ToVariant
		// in more than half the samples taken inside OnRequest. Nothing about a
		// library changes except its signature, so the version answers it instead.
		//
		const uint64 Version = I.second->GetVersion() + 1;	// nought means never sent
		uint64& LastVersion = Cache.EntryToken(I.first);
		if (LastVersion == Version) {
			Cache.Keep(I.first);
			continue;
		}
		LastVersion = Version;

		Cache.WriteEntry(I.first, I.second->ToVariant(SVarWriteOpt(), pMemPool));
	}

	return Cache.FinishRound(Removed);
}
