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

FW_NAMESPACE_BEGIN

enum class ETableInsertMode
{
	eNormal = 0,
	eMulti,
	eNoReplace,
	eNoInsert
};

enum class ETableResult
{
	eOK = 0,
	eNoMemory,
	eNoEntry,
	eKeyExists
};

template<typename K>
struct DefaultHasher {
	ULONG operator()(const K& key) const {
		unsigned int hash = 5381;
		for (unsigned char* ptr = (unsigned char*)&key; ptr < ((unsigned char*)&key) + sizeof(key); ptr++)
			hash = ((hash << 5) + hash) ^ *ptr;
		return hash;
	}
};

template <typename K, typename V, typename H = DefaultHasher<K>>
class Table : public AbstractContainer
{
	struct SBucketEntry
	{
		SBucketEntry(const K& Key, const V& Value = V()) : Key(Key), Value(Value) {}
		ULONG Hash = 0;
		SBucketEntry* Next = nullptr;
		K Key;
		V Value;
	};

public:
	Table(AbstractMemPool* pMem = nullptr) : AbstractContainer(pMem) {}
	Table(const Table& Other) : AbstractContainer(Other) { Assign(Other); }
	Table(Table&& Other) : AbstractContainer(Other)	{ m_ptr = Other.m_ptr; Other.m_ptr = nullptr; }
	~Table()										{ DetachData(); }

	Table& operator=(const Table& Other)			{ if(m_ptr == Other.m_ptr) return *this; Assign(Other); return *this; }
	Table& operator=(Table&& Other)					{ if(m_ptr == Other.m_ptr) return *this; DetachData(); m_pMem = Other.m_pMem; m_ptr = Other.m_ptr; Other.m_ptr = nullptr; return *this; }
	
	const SafeRef<V> operator[](const K& Key) const	{ return GetContrPtr(Key); }
	SafeRef<V> operator[](const K& Key)				{ return SetValuePtr(Key, nullptr); }

	size_t Count() const							{ return m_ptr ? m_ptr->Count : 0; }
	bool IsEmpty() const							{ return m_ptr ? m_ptr->Count == 0 : true; }

	void Clear()									{ DetachData(); }

	bool MakeExclusive()							{ if(m_ptr && m_ptr->Refs > 1) return MakeExclusive(nullptr); return true; }

	ETableResult Assign(const Table& Other)
	{
		if (!m_pMem)
			m_pMem = Other.m_pMem;
		else if(m_pMem != Other.m_pMem) { // if the containers use different memory pools, we need to make a copy of the data
			Clear();
			return Merge(Other);
		}
		AttachData(Other.m_ptr);
		return ETableResult::eOK;
	}

	ETableResult Merge(const Table& Other)
	{
		if (!MakeExclusive(nullptr))
			return ETableResult::eNoMemory;

		for (auto I = Other.begin(); I != Other.end(); ++I) {
			if(!InsertValue(I.Key(), &I.Value())) 
				return ETableResult::eNoMemory;
		}
		return ETableResult::eOK;
	}

	Array<K> Keys() const // keys may repeat if there are multiple entries with the same key
	{
		Array<K> Keys(m_pMem);
		if(!m_ptr || !Keys.Reserve(Count()))
			return Keys;
		for (size_t i = 0; i < m_ptr->BucketCount; ++i) {
			for (SBucketEntry* pEntry = m_ptr->Buckets[i]; pEntry != nullptr; pEntry = pEntry->Next) {
				Keys.Append(pEntry->Key);
			}
		}
		return Keys;
	}

	V* GetContrPtr(const K& Key, size_t Index = 0) const	
	{ 
		SBucketEntry* pEntry = FindEntry(Key, Index);
		return pEntry ? &pEntry->Value : nullptr;
	}
	V* GetValuePtr(const K& Key, size_t Index = 0) 
	{ 
		if (!MakeExclusive(nullptr)) 
			return nullptr; 

		SBucketEntry* pEntry = FindEntry(Key, Index);
		return pEntry ? &pEntry->Value : nullptr;
	}
	const SafeRef<V> FindValue(const K& Key) const { return GetContrPtr(Key); }
	SafeRef<V> FindValue(const K& Key) { return GetContrPtr(Key); }
	SafeRef<V> GetOrAddValue(const K& Key) { return SetValuePtr(Key, nullptr).first; }

	Pair<V*, bool> SetValuePtr(const K& Key, const V* pValue) 
	{
		if(!MakeExclusive(nullptr)) 
			return MakePair((V*)nullptr, false);

		SBucketEntry* pEntry = FindEntry(Key);
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

	ETableResult Insert(const K& Key, const V& Value, ETableInsertMode Mode = ETableInsertMode::eNormal)
	{ 
		if (!MakeExclusive(nullptr))
			return ETableResult::eNoMemory;

		if (Mode != ETableInsertMode::eMulti) {
			SBucketEntry* pEntry = FindEntry(Key);
			if (pEntry) {
				if (Mode == ETableInsertMode::eNoReplace) 
					return ETableResult::eKeyExists;
				pEntry->Value = Value;
				return ETableResult::eOK;
			}
		}

		if(Mode == ETableInsertMode::eNoInsert)
			return ETableResult::eNoEntry;

		return InsertValue(Key, &Value) ? ETableResult::eOK : ETableResult::eNoMemory;
	}

	ETableResult Remove(const K& Key, bool bAll = true)
	{
		if (!MakeExclusive(nullptr))
			return ETableResult::eNoMemory;

		RemoveImpl([](SBucketEntry* pEntry, void* Param) {
			const K &Key = *(const K*)(Param);
			return pEntry->Key == Key;
		}, (void*)&Key, Key, bAll);

		return ETableResult::eOK;
	}

	ETableResult Remove(const K& Key, const K& Value, bool bAll = true)
	{
		if (!MakeExclusive(nullptr))
			return ETableResult::eNoMemory;

		struct SParams {
			SParams(const K& Key, const V& Value) : Key(Key), Value(Value) {}
			const K& Key;
			const V& Value;
		};
		SParams Params = SParams(Key, Value);

		RemoveImpl([](SBucketEntry* pEntry, void* Param) {
			SParams &Params = *(SParams*)(Param);
			return pEntry->Key == Params.Key && pEntry->Value == Params.Value;
		}, (void*)&Params, Key, bAll);

		return ETableResult::eOK;
	}

	// Support for range-based for loops
	class Iterator {
	public:
		Iterator(SBucketEntry** pBuckets = nullptr, size_t Count = 0) : pBuckets(pBuckets), uRemBuckets(Count > 1 ? Count - 1 : 0) { if (pBuckets) SetNext(pBuckets[0]); }
		Iterator(SBucketEntry** pBuckets, SBucketEntry* pEntry, size_t Count) : pEntry(pEntry), pBuckets(pBuckets), uRemBuckets(Count > 1 ? Count - 1 : 0) { }

		Iterator& operator++() { if (pEntry) SetNext(pEntry->Next); return *this; }

		V& operator*() const { return pEntry->Value; }
		V* operator->() const { return &pEntry->Value; }
		V& Value() const { return pEntry->Value; }
		const K& Key() const { return pEntry->Key; }

		bool operator!=(const Iterator& other) const { return pEntry != other.pEntry; }
		bool operator==(const Iterator& other) const { return pEntry == other.pEntry; }

	protected:
		friend Table;

		virtual void SetNext(SBucketEntry* pNext) 
		{
			pEntry = pNext;

			/*if (Key) {
				while(pEntry && pEntry->Key != *Key)
					pEntry = pEntry->Next;
				return;
			}*/

			while (pEntry == NULL && uRemBuckets > 0) {
				--uRemBuckets;
				pEntry = *++pBuckets;
			}
		}

		SBucketEntry* pEntry = nullptr;
		SBucketEntry** pBuckets = nullptr;
		size_t uRemBuckets = 0;
	};

	Iterator begin() const							{ return m_ptr ? Iterator(m_ptr->Buckets, m_ptr->BucketCount) : end(); }
	Iterator end() const							{ return Iterator(); }

	Iterator find(const K& Key) const
	{
		if(!m_ptr) return end();
		size_t Index = MapBucketIndex(Hash(Key));
		SBucketEntry* pEntry  = m_ptr->Buckets[Index];
		while (pEntry != NULL) {
			if (pEntry->Key == Key)
				break;
			pEntry = pEntry->Next;
		}
		return Iterator(m_ptr->Buckets + Index, pEntry, m_ptr->BucketCount - Index);
	}

	Iterator erase(Iterator I)
	{
		if (!I.pEntry || !MakeExclusive(&I))
			return end();
		SBucketEntry* pEntry = I.pEntry;
		++I;
		RemoveImpl([](SBucketEntry* pEntry, void* Param) {
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

		for (size_t i = 0; i < m_ptr->BucketCount; i++) {
			DbgPrint("bucket: %d\n", i);

			SBucketEntry* pEntry = m_ptr->Buckets[i];
			while (pEntry != NULL) {
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
		volatile LONG Refs = 0;
		size_t Count = 0;
		size_t BucketCount = 0;
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

			for(size_t i = 0; i < m_ptr->BucketCount; i++) {
				for (SBucketEntry* pEntry = m_ptr->Buckets[i]; pEntry != nullptr; pEntry = pEntry->Next) {
					SBucketEntry* New = (SBucketEntry*)m_pMem->Alloc(sizeof(SBucketEntry));
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
				InsertEntry(pHead);
				pHead = pNext;
				if (pI && pHead == pI->pEntry) {
					size_t Index = MapBucketIndex(pHead->Hash);
					pI->pBuckets = m_ptr->Buckets + Index;
					pI->uRemBuckets = ptr->BucketCount - Index;
					pI = NULL;
				}
			}
		}
		return true;
	}

	SBucketEntry* InsertValue(const K& Key, const V* pValue)
	{
		ASSERT(m_ptr && m_ptr->BucketCount > 0);

		// check and if we need to grow our bucket array
		const int ReduceBuckets = 0; // average 1.5 nodes/bucket and 49% of buckets used
		//const int ReduceBuckets = 1; // average 1.9 nodes/bucket and 75% of buckets used
		//const int ReduceBuckets = 2; // average 3.2 nodes/bucket and 91% of buckets used
		//const int ReduceBuckets = 3; // average 5.9 nodes/bucket and 98% of buckets used
		//const int ReduceBuckets = 4; // average 11.8 nodes/bucket and 100% of buckets used
		if ((m_ptr->Count >> ReduceBuckets) >= m_ptr->BucketCount)
		{
			size_t BucketCount = (m_ptr->BucketCount > 0) ? (m_ptr->BucketCount << 1) : 1; // * 2
			if(!ResizeTable(BucketCount))
				return nullptr;
		}

		SBucketEntry* pEntry = (SBucketEntry*) m_pMem->Alloc(sizeof(SBucketEntry));
		if (!pEntry) return nullptr;
		if(pValue) new (pEntry) SBucketEntry(Key, *pValue);
		else new (pEntry) SBucketEntry(Key);
		pEntry->Hash = Hash(Key);

		m_ptr->Count++;
		InsertEntry(pEntry);

		return pEntry;
	}

	bool ResizeTable(size_t BucketCount)
	{
		STableData* ptr = MakeData(BucketCount);
		if (!ptr) return false;

		SBucketEntry* pHead = nullptr;

		for (size_t i = m_ptr->BucketCount; i--; ) {
			SBucketEntry* &pEntry = m_ptr->Buckets[i];
			while (pEntry != nullptr) {
				SBucketEntry* pNext = pEntry->Next;
				pEntry->Next = pHead;
				pHead = pEntry;
				pEntry = pNext;
			}
		}
		m_ptr->BucketCount = 0; // all empty no need to inspect tehem on free

		ptr->Count = m_ptr->Count;
		AttachData(ptr);

		while (pHead != nullptr) {
			SBucketEntry* pNext = pHead->Next;
			InsertEntry(pHead);
			pHead = pNext;
		}
		return true;
	}

	ULONG Hash(const K& Key) const
	{
		H Hasher;
		return Hasher(Key);
	}

	size_t MapBucketIndex(ULONG Hash) const
	{
		// Note: this works only with bucket sizes which are a power of 2
		//          this can be generalized by using % instead of &
		return Hash & (m_ptr->BucketCount - 1);
	}

	void InsertEntry(SBucketEntry* pEntry)
	{
		size_t Index = MapBucketIndex(pEntry->Hash);
		SBucketEntry* &pBucket = m_ptr->Buckets[Index];
		pEntry->Next = pBucket;
		pBucket = pEntry;
	}

	SBucketEntry* FindEntry(const K& Key, size_t ToSkip = 0) const
	{
		if (!m_ptr) return nullptr;

		size_t Index = MapBucketIndex(Hash(Key));
		for (SBucketEntry* pEntry = m_ptr->Buckets[Index]; pEntry != nullptr; pEntry = pEntry->Next) {
			if (pEntry->Key == Key) {
				if (ToSkip-- == 0)
					return pEntry;
			}
		}
		return nullptr;
	}

	void RemoveImpl(bool(*Match)(SBucketEntry* pEntry, void* Param), void* Param, const K &Key, bool bAll = true)
	{
		if(!m_ptr) return;

		size_t Index = MapBucketIndex(Hash(Key));
		SBucketEntry** pEntry = &m_ptr->Buckets[Index];
		while (*pEntry != nullptr) {
			if (!Match(*pEntry, Param)) {
				pEntry = &(*pEntry)->Next;
				continue;
			}
			SBucketEntry* pOld = *pEntry;
			*pEntry = (*pEntry)->Next;
			pOld->~SBucketEntry();
			m_pMem->Free(pOld);
			m_ptr->Count--;
			if(!bAll)
				break;
		}
	}

	STableData* MakeData(size_t BucketCount)
	{
		if (!m_pMem) 
			return nullptr;
		STableData* ptr = (STableData*)m_pMem->Alloc(sizeof(STableData) + BucketCount * sizeof(SBucketEntry*));
		if(!ptr) 
			return nullptr;
		new (ptr) STableData();
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
				for(size_t i = 0; i < m_ptr->BucketCount; i++) { // for each bucket
					for (SBucketEntry* Curr = m_ptr->Buckets[i]; Curr != nullptr; ) { // for each entry
						SBucketEntry* Next = Curr->Next;
						Curr->~SBucketEntry();
						m_pMem->Free(Curr);
						Curr = Next;
					}
				}
				m_pMem->Free(m_ptr);
			}
			m_ptr = nullptr;
		}
	}
};

FW_NAMESPACE_END