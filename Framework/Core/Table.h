/*
* Copyright (c) 2023-2025 David Xanatos, xanasoft.com
* All rights reserved.
*
* This file is part of MajorPrivacy.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
*/

#pragma once

#include "Framework.h"
#include "Array.h"
#include "SafeRef.h"
#include "Pair.h"
#include "String.h"

FW_NAMESPACE_BEGIN

template<typename K>
struct DefaultHasher {
	ULONG operator()(const K& key) const {
		unsigned int hash = 5381;
		for (unsigned char* ptr = (unsigned char*)&key; ptr < ((unsigned char*)&key) + sizeof(key); ptr++)
			hash = ((hash << 5) + hash) ^ *ptr;
		return hash;
	}
};

template<typename C>
ULONG HashString(const C* pStr) { // Time: 1532ms
	size_t hash = 14695981039346656037ULL; // FNV offset basis
	if(pStr)
		for (; *pStr; pStr++) {
		hash ^= static_cast<size_t>(*pStr);
		hash *= 1099511628211ULL; // FNV prime
	}
	return (ULONG)hash | (ULONG)(hash >> 32);
}
template<>
struct DefaultHasher<FW::StringA> {
	ULONG operator()(const FW::StringA& key) const {
		return HashString(key.ConstData());
	}
};

template<>
struct DefaultHasher<FW::StringW> {
	ULONG operator()(const FW::StringW& key) const {
		return HashString(key.ConstData());
	}
};

//template<typename K>
//ULONG MurmurHash3(const K* key, int len, ULONG seed = 0) 
//{
//	const BYTE* data = (BYTE*)(key);
//	const int nblocks = len / 4;
//	ULONG h1 = seed, k1;
//	const ULONG c1 = 0xcc9e2d51, c2 = 0x1b873593;
//
//	// Process body
//	const ULONG* blocks = reinterpret_cast<const ULONG*>(data);
//	for (int i = 0; i < nblocks; i++) {
//		k1 = blocks[i];
//		k1 *= c1; k1 = (k1 << 15) | (k1 >> 17); k1 *= c2;
//		h1 ^= k1;
//		h1 = (h1 << 13) | (h1 >> 19);
//		h1 = h1 * 5 + 0xe6546b64;
//	}
//
//	// Process tail
//	k1 = 0;
//	const BYTE* tail = data + nblocks * 4;
//	switch (len & 3) {
//	case 3: k1 ^= tail[2] << 16;
//	case 2: k1 ^= tail[1] << 8;
//	case 1: k1 ^= tail[0];
//		k1 *= c1; k1 = (k1 << 15) | (k1 >> 17); k1 *= c2; h1 ^= k1;
//	}
//
//	// Finalization
//	h1 ^= len;
//	h1 ^= (h1 >> 16);
//	h1 *= 0x85ebca6b;
//	h1 ^= (h1 >> 13);
//	h1 *= 0xc2b2ae35;
//	h1 ^= (h1 >> 16);
//	return h1;
//}
//
//template<>
//struct DefaultHasher<FW::StringW> {
//	ULONG operator()(const FW::StringW& key) const {
//		return MurmurHash3(key.ConstData(), key.Length() * sizeof(wchar_t));
//	}
//};

template <typename K, typename V, typename H = DefaultHasher<K>, typename NEW = DefaultValue<V>>
class Table : public AbstractContainer
{
public:
	struct SBucketEntry
	{
		SBucketEntry(const K& Key, const V& Value) : Key(Key), Value(Value) { Hash = Table::Hash(Key); Next = nullptr; }
		ULONG Hash;
		SBucketEntry* Next;
		K Key;
		V Value;
	};

	Table(AbstractMemPool* pMem = nullptr) : AbstractContainer(pMem) {}
	Table(const Table& Other) : AbstractContainer(nullptr) { Assign(Other); }
	Table(Table&& Other) : AbstractContainer(nullptr)	{ Move(Other); }
	~Table()										{ DetachData(); }

	Table& operator=(const Table& Other)			{ if(m_ptr != Other.m_ptr || m_pMem != Other.m_pMem) Assign(Other); return *this; }
	Table& operator=(Table&& Other)					{ if(m_ptr != Other.m_ptr || m_pMem != Other.m_pMem) Move(Other); return *this; }
	
	const SafeRef<V> operator[](const K& Key) const	{ return GetContrPtr(Key); }
	SafeRef<V> operator[](const K& Key)				{ return SetValuePtr(Key, nullptr); }

	size_t Count() const							{ return m_ptr ? m_ptr->Count : 0; }
	size_t size() const								{ return Count(); } // for STL compatibility
	bool IsEmpty() const							{ return m_ptr ? m_ptr->Count == 0 : true; }
	bool empty() const								{ return IsEmpty(); } // for STL compatibility

	void Clear()									{ DetachData(); }
	void clear()									{ Clear(); } // for STL compatibility

	bool MakeExclusive()							{ if(m_ptr && m_ptr->Refs > 1) return MakeExclusive(nullptr); return true; }

	EInsertResult Assign(const Table& Other)
	{
		if (!m_pMem)
			m_pMem = Other.m_pMem;
		else if(m_pMem != Other.m_pMem) { // if the containers use different memory pools, we need to make a copy of the data
			Clear();
			return Merge(Other);
		}

		AttachData(Other.m_ptr);
		return EInsertResult::eOK;
	}

	EInsertResult Move(Table& Other)
	{
		if (!m_pMem)
			m_pMem = Other.m_pMem;
		else if(m_pMem != Other.m_pMem) { // if the containers use different memory pools, we need to make a copy of the data
			Clear();
			return Merge(Other);
		}

		DetachData();
		m_ptr = Other.m_ptr; 
		Other.m_ptr = nullptr;
		return EInsertResult::eOK;
	}

	EInsertResult Merge(const Table& Other)
	{
		if (!MakeExclusive(nullptr))
			return EInsertResult::eNoMemory;

		for (auto I = Other.begin(); I != Other.end(); ++I) {
			if(!InsertValue(I.Key(), &I.Value())) 
				return EInsertResult::eNoMemory;
		}
		return EInsertResult::eOK;
	}

	Array<K> Keys() const // keys may repeat if there are multiple entries with the same key
	{
		Array<K> Keys(m_pMem);
		if(!m_ptr || !Keys.Reserve(Count()))
			return Keys;
		for (uint32 i = 0; i < m_ptr->BucketCount; ++i) {
			for (SBucketEntry* pEntry = m_ptr->Buckets[i]; pEntry != nullptr; pEntry = pEntry->Next) {
				Keys.Append(pEntry->Key);
			}
		}
		return Keys;
	}

	V* GetContrPtr(const K& Key, size_t Index = 0) const	
	{ 
		if (!m_ptr) return nullptr;
		SBucketEntry* pEntry = FindEntry(m_ptr->Buckets, m_ptr->BucketCount, Key, Index);
		return pEntry ? &pEntry->Value : nullptr;
	}
	V* GetValuePtr(const K& Key, bool bCanAdd = false, size_t Index = 0) 
	{ 
		if (!MakeExclusive(nullptr)) 
			return nullptr; 

		if (!m_ptr) return nullptr;
		SBucketEntry* pEntry = FindEntry(m_ptr->Buckets, m_ptr->BucketCount, Key, Index);
		if(pEntry)
			return &pEntry->Value;
		if(!bCanAdd)
			return nullptr;
		pEntry = InsertValue(Key, nullptr);
		return pEntry ? &pEntry->Value : nullptr;
	}
	const SafeRef<V> FindValue(const K& Key) const { return GetContrPtr(Key); }
	SafeRef<V> FindValue(const K& Key) { return GetContrPtr(Key); }
	SafeRef<V> GetOrAddValue(const K& Key) { return SetValuePtr(Key, nullptr).first; }

	Pair<V*, bool> SetValuePtr(const K& Key, const V* pValue) 
	{
		if(!MakeExclusive(nullptr)) 
			return MakePair((V*)nullptr, false);

		if (!m_ptr) return nullptr;
		SBucketEntry* pEntry = FindEntry(m_ptr->Buckets, m_ptr->BucketCount, Key);
		if (pEntry) {
			if (pValue) pEntry->Value = *pValue;
			return MakePair(&pEntry->Value, false);
		}

		pEntry = InsertValue(Key, pValue);
		if(!pEntry) return MakePair((V*)nullptr, false);
		return MakePair(&pEntry->Value, true); // inserted true
	}
	V* AddValuePtr(const K& Key, const V* pValue)
	{
		if(!MakeExclusive(nullptr)) 
			return nullptr;

		return &InsertValue(Key, pValue)->Value;
	}
	bool SetValue(const K& Key, const V& Value)		{ return SetValuePtr(Key, &Value).second;}

	EInsertResult Insert(const K& Key, const V& Value, EInsertMode Mode = EInsertMode::eNormal)
	{ 
		if (!MakeExclusive(nullptr))
			return EInsertResult::eNoMemory;

		if (Mode != EInsertMode::eMulti && m_ptr) {
			SBucketEntry* pEntry = FindEntry(m_ptr->Buckets, m_ptr->BucketCount, Key);
			if (pEntry) {
				if (Mode == EInsertMode::eNoReplace) 
					return EInsertResult::eKeyExists;
				pEntry->Value = Value;
				return EInsertResult::eOK;
			}
		}

		if(Mode == EInsertMode::eNoInsert)
			return EInsertResult::eNoEntry;

		return InsertValue(Key, &Value) ? EInsertResult::eOK : EInsertResult::eNoMemory;
	}

	int Remove(const K& Key, bool bAll = true)
	{
		if (!MakeExclusive(nullptr))
			return 0;

		int Count = RemoveImpl(m_pMem, m_ptr->Buckets, m_ptr->BucketCount, nullptr, nullptr, Key, bAll);
		m_ptr->Count -= Count;
		return Count;
	}

	int Remove(const K& Key, const K& Value, bool bAll = true)
	{
		if (!MakeExclusive(nullptr))
			return 0;

		struct SParams {
			SParams(const K& Key, const V& Value) : Key(Key), Value(Value) {}
			const K& Key;
			const V& Value;
		};
		SParams Params = SParams(Key, Value);

		int Count = RemoveImpl(m_pMem, m_ptr->Buckets, m_ptr->BucketCount, [](SBucketEntry* pEntry, void* Param) {
			SParams &Params = *(SParams*)(Param);
			return pEntry->Key == Params.Key && pEntry->Value == Params.Value;
		}, (void*)&Params, Key, bAll);
		m_ptr->Count -= Count;
		return Count;
	}

	V Take(const K& Key)
	{
		V Value;
		if (MakeExclusive(nullptr)) {
			size_t Index = MapBucketIndex(m_ptr->BucketCount, Hash(Key));
			SBucketEntry** pEntry = &m_ptr->Buckets[Index];
			while (*pEntry != nullptr) {
				if ((*pEntry)->Key != Key) {
					pEntry = &(*pEntry)->Next;
					continue;
				}
				SBucketEntry* pOld = *pEntry;
				*pEntry = (*pEntry)->Next;
				pOld->~SBucketEntry();
				Value = pOld->Value;
				if(m_pMem) m_pMem->Free(pOld);
				else ::MemFree(pOld, 0);
				break;
			}
		}
		return Value;
	}

	// Support for range-based for loops
	class Iterator {
	public:
		Iterator(SBucketEntry** Buckets = nullptr, uint32 Count = 0) : pBuckets(Buckets), uRemBuckets(Count > 1 ? Count - 1 : 0) { if (pBuckets) SetNext(pBuckets[0]); }
		//Iterator(SBucketEntry** Buckets, SBucketEntry* Entry, uint32 Count) : pEntry(Entry), pBuckets(Buckets), uRemBuckets(Count > 1 ? Count - 1 : 0) { }
		Iterator(const K& Key, SBucketEntry** Buckets, uint32 BucketCount) 
		{
			size_t Index = MapBucketIndex(BucketCount, Hash(Key));
			pEntry = Buckets[Index];
			while (pEntry != nullptr) {
				if (pEntry->Key == Key)
					break;
				pEntry = pEntry->Next;
			}
			pBuckets = Buckets + Index;
			uRemBuckets = BucketCount - (uint32)Index;
			if(uRemBuckets) uRemBuckets--;
		}

		Iterator& operator++() { if (pEntry) SetNext(pEntry->Next); return *this; }

		V& operator*() const { return pEntry->Value; }
		V* operator->() const { return &pEntry->Value; }
		V& Value() const { return pEntry->Value; }
		const K& Key() const { return pEntry->Key; }

		bool operator!=(const Iterator& other) const { return pEntry != other.pEntry; }
		bool operator==(const Iterator& other) const { return pEntry == other.pEntry; }

	protected:
		friend Table;

		void SetNext(SBucketEntry* pNext) 
		{
			pEntry = pNext;

			/*if (Key) {
				while(pEntry && pEntry->Key != *Key)
					pEntry = pEntry->Next;
				return;
			}*/

			while (pEntry == nullptr && uRemBuckets > 0) {
				--uRemBuckets;
				pEntry = *++pBuckets;
			}
		}

		SBucketEntry* pEntry = nullptr;
		SBucketEntry** pBuckets = nullptr;
		uint32 uRemBuckets = 0;
	};

	Iterator begin() const							{ return m_ptr ? Iterator(m_ptr->Buckets, m_ptr->BucketCount) : end(); }
	Iterator end() const							{ return Iterator(); }
	Iterator find(const K& Key) const				{ return m_ptr ? Iterator(Key, m_ptr->Buckets, m_ptr->BucketCount) : end(); }

	Iterator erase(Iterator I)
	{
		if (!I.pEntry || !MakeExclusive(&I))
			return end();
		SBucketEntry* pEntry = I.pEntry;
		++I;
		RemoveImpl(m_pMem, m_ptr->Buckets, m_ptr->BucketCount, [](SBucketEntry* pEntry, void* Param) {
			SBucketEntry* pOld = (SBucketEntry*)Param;
			return pEntry == pOld;
		}, (void*)pEntry, pEntry->Key, false);
		return I;
	}

	/*void Dump(void(*printKey)(const K& Key), void(*printValue)(const V& Value))
	{
		DbgPrint("Table dump:\n");

		if (!m_ptr) {
			DbgPrint("[EMPTY]");
			return;
		}

		for (uint32 i = 0; i < m_ptr->BucketCount; i++) {
			DbgPrint("bucket: %d\n", i);

			SBucketEntry* pEntry = m_ptr->Buckets[i];
			while (pEntry != nullptr) {
				DbgPrint("  ");
				printKey(pEntry->Key);
				DbgPrint(" (0x%08X): ", pEntry->Hash);
				printValue(pEntry->Value);
				pEntry = pEntry->Next;
				DbgPrint("\n");
			}
		}

		DbgPrint("+++\n");
	}*/

protected:

	struct STableData
	{
		volatile LONG Refs;
		uint32 BucketCount;
		size_t Count;
#pragma warning( push )
#pragma warning( disable : 4200)
		SBucketEntry* Buckets[0];
#pragma warning( pop )
	}* m_ptr = nullptr;

	bool MakeExclusive(Iterator* pI)
	{
		if (!m_ptr) 
		{
			STableData* ptr = MakeData(1);
			AttachData(ptr);
			return ptr != nullptr;
		}  
		else if (m_ptr->Refs > 1)
		{
			STableData* ptr = MakeData(m_ptr->BucketCount);
			if (!ptr) return false;

			SBucketEntry* pHead = nullptr;

			for(uint32 i = 0; i < m_ptr->BucketCount; i++) {
				for (SBucketEntry* pEntry = m_ptr->Buckets[i]; pEntry != nullptr; pEntry = pEntry->Next) {
					SBucketEntry* New = (SBucketEntry*)MemAlloc(sizeof(SBucketEntry));
					if (!New) break; // todo
					new (New) SBucketEntry(pEntry->Key, pEntry->Value);
					New->Hash = pEntry->Hash;
					New->Next = pHead;
					pHead = New;
					if (pI && pI->pBuckets && pEntry == pI->pEntry) {
						pI->pEntry = New;
						pI->pBuckets = nullptr;
						pI->uRemBuckets = 0;
					}
				}
			}

			ptr->Count = m_ptr->Count;
			AttachData(ptr);

			while (pHead != nullptr) {
				SBucketEntry* pNext = pHead->Next;
				InsertEntry(m_ptr->Buckets, m_ptr->BucketCount, pHead);
				pHead = pNext;
				if (pI && pHead == pI->pEntry) {
					size_t Index = MapBucketIndex(pHead->Hash);
					pI->pBuckets = m_ptr->Buckets + Index;
					pI->uRemBuckets = ptr->BucketCount - (uint32)Index;
					pI = nullptr;
				}
			}
		}
		return true;
	}

	SBucketEntry* InsertValue(const K& Key, const V* pValue)
	{
		ASSERT(m_ptr && m_ptr->BucketCount > 0);

		//
		// check and if we need to grow our bucket array
		// 
		// we limit the max bucket count to 2G so that we can use a uint32 as bucket count
		// having miore then 2G buckets is not practical anyway
		// 

		const int ReduceBuckets = 0; // average 1.5 nodes/bucket and 49% of buckets used
		//const int ReduceBuckets = 1; // average 1.9 nodes/bucket and 75% of buckets used
		//const int ReduceBuckets = 2; // average 3.2 nodes/bucket and 91% of buckets used
		//const int ReduceBuckets = 3; // average 5.9 nodes/bucket and 98% of buckets used
		//const int ReduceBuckets = 4; // average 11.8 nodes/bucket and 100% of buckets used
		if ((m_ptr->Count >> ReduceBuckets) >= m_ptr->BucketCount && (m_ptr->BucketCount & 0x80000000) == 0)
		{
			uint32 BucketCount = (m_ptr->BucketCount > 0) ? (m_ptr->BucketCount << 1) : 1;
			STableData* ptr = MakeData(BucketCount);
			if(!ptr)
				return nullptr;

			MoveTable(ptr->Buckets, ptr->BucketCount, m_ptr->Buckets, m_ptr->BucketCount);
			ptr->Count = m_ptr->Count;
			m_ptr->BucketCount = 0; // all empty no need to inspect tehem on free
			
			AttachData(ptr);
		}

		SBucketEntry* pEntry = (SBucketEntry*) MemAlloc(sizeof(SBucketEntry));
		if (!pEntry) return nullptr;
		if(pValue) 
			new (pEntry) SBucketEntry(Key, *pValue);
		else
		{
			NEW New;
			new (pEntry) SBucketEntry(Key, New(m_pMem));
		}

		m_ptr->Count++;
		InsertEntry(m_ptr->Buckets, m_ptr->BucketCount, pEntry);

		return pEntry;
	}

	static size_t MapBucketIndex(uint32 BucketCount, ULONG Hash)
	{
		// Note: this works only with bucket sizes which are a power of 2
		//          this can be generalized by using % instead of &
		return Hash & (BucketCount - 1);
	}

	size_t MapBucketIndex(ULONG Hash) const
	{
		return MapBucketIndex(m_ptr->BucketCount, Hash);
	}

	STableData* MakeData(uint32 BucketCount)
	{
		STableData* ptr = (STableData*)MemAlloc(sizeof(STableData) + BucketCount * sizeof(SBucketEntry*));
		if(!ptr) 
			return nullptr;
		ptr->Refs = 0;
		ptr->Count = 0;
		ptr->BucketCount = BucketCount;
		MemZero(ptr->Buckets, BucketCount * sizeof(SBucketEntry*));
		return ptr;
	}

	void AttachData(STableData* ptr)
	{
		DetachData();

		if (ptr) {
			m_ptr = ptr;
			InterlockedIncrement(&m_ptr->Refs);
		}
	}

	void DetachData()
	{
		if (m_ptr) {
			if (InterlockedDecrement(&m_ptr->Refs) == 0) {
				for(uint32 i = 0; i < m_ptr->BucketCount; i++) { // for each bucket
					for (SBucketEntry* Curr = m_ptr->Buckets[i]; Curr != nullptr; ) { // for each entry
						SBucketEntry* Next = Curr->Next;
						Curr->~SBucketEntry();
						MemFree(Curr);
						Curr = Next;
					}
				}
				MemFree(m_ptr);
			}
			m_ptr = nullptr;
		}
	}

public:

	static ULONG Hash(const K& Key)
	{
		H Hasher;
		return Hasher(Key);
	}

	static void MoveTable(SBucketEntry** newBuckets, uint32 newBucketCount, SBucketEntry** oldBuckets, uint32 oldBucketCount)
	{
		SBucketEntry* pHead = nullptr;

		for (uint32 i = oldBucketCount; i--; ) {
			SBucketEntry* &pEntry = oldBuckets[i];
			while (pEntry != nullptr) {
				SBucketEntry* pNext = pEntry->Next;
				pEntry->Next = pHead;
				pHead = pEntry;
				pEntry = pNext;
			}
		}

		while (pHead != nullptr) {
			SBucketEntry* pNext = pHead->Next;
			InsertEntry(newBuckets, newBucketCount, pHead);
			pHead = pNext;
		}
	}

	static void InsertEntry(SBucketEntry** Buckets, uint32 BucketCount, SBucketEntry* pEntry)
	{
		size_t Index = MapBucketIndex(BucketCount, pEntry->Hash);
		SBucketEntry* &pBucket = Buckets[Index];
		pEntry->Next = pBucket;
		pBucket = pEntry;
	}

	static SBucketEntry* FindEntry(SBucketEntry** Buckets, uint32 BucketCount, const K& Key, size_t ToSkip = 0)
	{
		size_t Index = MapBucketIndex(BucketCount, Hash(Key));
		for (SBucketEntry* pEntry = Buckets[Index]; pEntry != nullptr; pEntry = pEntry->Next) {
			if (pEntry->Key == Key) {
				if (ToSkip-- == 0)
					return pEntry;
			}
		}
		return nullptr;
	}

	static int RemoveImpl(AbstractMemPool* pMem, SBucketEntry** Buckets, uint32 BucketCount, bool(*Match)(SBucketEntry* pEntry, void* Param), void* Param, const K &Key, bool bAll = true)
	{
		int Count = 0;
		size_t Index = MapBucketIndex(BucketCount, Hash(Key));
		SBucketEntry** pEntry = &Buckets[Index];
		while (*pEntry != nullptr) {
			bool bMatch = Match ? Match(*pEntry, Param) : (*pEntry)->Key == Key;
			if (!bMatch) {
				pEntry = &(*pEntry)->Next;
				continue;
			}
			SBucketEntry* pOld = *pEntry;
			*pEntry = (*pEntry)->Next;
			pOld->~SBucketEntry();
			if(pMem) pMem->Free(pOld);
			else ::MemFree(pOld, 0);
			Count++;
			if(!bAll)
				break;
		}
		return Count;
	}
};

FW_NAMESPACE_END