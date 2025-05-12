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
#include "Memory.h"

FW_NAMESPACE_BEGIN

template <typename V>
class List : public AbstractContainer
{
public:
	List(AbstractMemPool* pMem = nullptr) : AbstractContainer(pMem) {}
	List(const List& Other) : AbstractContainer(nullptr) { Assign(Other); }
	List(List&& Other) : AbstractContainer(nullptr)	{ Move(Other); }
	~List()											{ DetachData(); }

	List& operator=(const List& Other)				{ if(m_ptr != Other.m_ptr || m_pMem != Other.m_pMem) Assign(Other); return *this; }
	List& operator=(List&& Other)					{ if(m_ptr != Other.m_ptr || m_pMem != Other.m_pMem) Move(Other); return *this; }
	List& operator+=(const List& Other)				{ if (!m_pMem) m_pMem = Other.m_pMem; Append(Other); return *this; } 
	List& operator+=(const V& Value)				{ Append(Value); return *this; }
	List operator+(const List& Other) const			{ List str(*this); str.Append(Other); return str; }
	List operator+(const V& Value) const			{ List str(*this); str.Append(Value); return str; }

	struct SListEntry
	{
		SListEntry(const V& Data) : Data(Data) {}
		V Data;
		SListEntry* Next = nullptr;
		SListEntry* Prev = nullptr;
	};

	size_t Count() const							{ return m_ptr ? m_ptr->Count : 0; }
	size_t size() const								{ return Count(); } // for STL compatibility
	bool IsEmpty() const							{ return m_ptr ? m_ptr->Count == 0 : true; }
	bool empty() const								{ return IsEmpty(); } // for STL compatibility

	void Clear()									{ DetachData(); }
	void clear()									{ Clear(); } // for STL compatibility

	bool MakeExclusive()							{ if(m_ptr && m_ptr->Refs > 1) return MakeExclusive(nullptr); return true; }

	bool Assign(const List& Other)
	{
		if (!m_pMem)
			m_pMem = Other.m_pMem;
		else if(m_pMem != Other.m_pMem) { // if the containers use different memory pools, we need to make a copy of the data
			Clear();
			return Append(Other);
		}

		AttachData(Other.m_ptr);
		return true;
	}

	bool Move(List& Other)
	{
		if (!m_pMem)
			m_pMem = Other.m_pMem;
		else if(m_pMem != Other.m_pMem) { // if the containers use different memory pools, we need to make a copy of the data
			Clear();
			return Append(Other);
		}

		DetachData();
		m_ptr = Other.m_ptr; 
		Other.m_ptr = nullptr;
		return true;
	}

	V* Append(const V& Value)
	{ 
		if (!MakeExclusive(nullptr))
			return nullptr;

		SListEntry* ptr = (SListEntry*)MemAlloc(sizeof(SListEntry));
		if (!ptr) return nullptr;
		new (ptr) SListEntry(Value);
		InsertAfter(m_ptr, nullptr, ptr);
		return &ptr->Data;
	}

	V* Prepand(const V& Value)
	{ 
		if (!MakeExclusive(nullptr))
			return false;

		SListEntry* ptr = (SListEntry*)MemAlloc(sizeof(SListEntry));
		if (!ptr) return nullptr;
		new (ptr) SListEntry(Value);
		InsertBefore(m_ptr, nullptr, ptr);
		return &ptr->Data;
	}

	bool Append(const List& Other)
	{
		for (auto I = Other.begin(); I != Other.end(); ++I) {
			if (!Append(*I)) return false;
		}
		return true;
	}

	bool Prepand(const List& Other)
	{
		for (auto I = Other.begin(true); I != Other.end(); ++I) {
			if (!Prepent(*I)) return false;
		}
		return true;
	}

	// Support for range-based for loops
	class Iterator {
	public:
		Iterator(SListEntry* pEntry = nullptr, bool bReverse = false) : pEntry(pEntry), bReverse(bReverse) {}

		Iterator& operator++() {
			if (pEntry) pEntry = bReverse ? pEntry->Prev : pEntry->Next;
			return *this;
		}

		V& operator*() const { return pEntry->Data; }
		V* operator->() const { return &pEntry->Data; }

		bool operator!=(const Iterator& other) const { return pEntry != other.pEntry; }
		bool operator==(const Iterator& other) const { return pEntry == other.pEntry; }

	protected:
		friend List;
		SListEntry* pEntry = nullptr;
		bool bReverse = false;
	};

	Iterator begin(bool bReverse = false) const		{ return m_ptr ? Iterator(m_ptr->Head, bReverse) : end(); }
	Iterator end() const							{ return Iterator(); }

	Iterator erase(Iterator I)
	{
		if (!I.pEntry || !MakeExclusive(&I))
			return end();
		SListEntry* ptr = I.pEntry;
		++I;
		RemoveEntry(m_ptr, ptr);
		ptr->~SListEntry();
		MemFree(ptr);
		return I;
	}

	Iterator insert(Iterator I, const V& Value)
	{
		if (!I.pEntry || !MakeExclusive(&I))
			return end();
		SListEntry* ptr = (SListEntry*)MemAlloc(sizeof(SListEntry));
		if (!ptr) return end();
		new (ptr) SListEntry(Value);
		InsertBefore(m_ptr, I.pEntry, ptr);
		return I;
	}

protected:

	struct SListData
	{
		volatile LONG Refs = 0;
		size_t Count = 0;
		SListEntry* Head = nullptr;
		SListEntry* Tail = nullptr;
	}* m_ptr = nullptr;

	bool MakeExclusive(Iterator* pI)
	{
		if (!m_ptr)
		{
			SListData* ptr = MakeData();
			AttachData(ptr);
			return ptr != nullptr;
		}
		else if (m_ptr->Refs > 1) 
		{
			SListData* ptr = MakeData();
			if (!ptr) return false;
			for(SListEntry* Curr = m_ptr->Head; Curr != nullptr; Curr = Curr->Next) {
				SListEntry* New = (SListEntry*)MemAlloc(sizeof(SListEntry));
				if (!New) break; // todo
				new (New) SListEntry(Curr->Data);
				InsertAfter(ptr, ptr->Tail, New);
				// we need to update the iterator if it points to the current entry
				if (pI && Curr == pI->pEntry) {
					pI->pEntry = New;
					pI = nullptr;
				}
			}
			AttachData(ptr);
		}
		return true;
	}

	static void InsertBefore(SListData* ptr, SListEntry* Before, SListEntry* Entry)
	{
		if (Before == ptr->Head || Before == nullptr) 
		{
			SListEntry* Next = ptr->Head;
			ptr->Head = Entry;
			Entry->Prev = nullptr;
			Entry->Next = Next;
			if (Next == nullptr)
				ptr->Tail = Entry;
			else
				Next->Prev = Entry;
		} 
		else 
		{
			SListEntry *Prev = Before->Prev;
			Prev->Next = Entry;
			Entry->Prev = Prev;
			Before->Prev = Entry;
			Entry->Next = Before;
		}
		ptr->Count++;
	}

	static void InsertAfter(SListData* ptr, SListEntry* After, SListEntry* Entry)
	{
		if (After == ptr->Tail || After == nullptr) 
		{
			SListEntry *Prev = ptr->Tail;
			ptr->Tail = Entry;
			Entry->Prev = Prev;
			Entry->Next = nullptr;
			if (Prev == nullptr)
				ptr->Head = Entry;
			else
				Prev->Next = Entry;
		}
		else
		{
			SListEntry *Next = After->Next;
			Next->Prev = Entry;
			Entry->Next = Next;
			After->Next = Entry;
			Entry->Prev = After;
		}
		ptr->Count++;
	}

	static void RemoveEntry(SListData* ptr, SListEntry* Entry)
	{
		SListEntry* Prev = Entry->Prev;
		SListEntry* Next = Entry->Next;
		if (Prev == nullptr && Next == nullptr) {
			ptr->Tail = nullptr;
			ptr->Head = nullptr;
		} 
		else if (Prev == nullptr) {
			ptr->Head = Next;
			Next->Prev = nullptr;
		} 
		else if (Next == nullptr) {
			ptr->Tail = Prev;
			Prev->Next = nullptr;
		} 
		else {
			Prev->Next = Next;
			Next->Prev = Prev;
		}
		ptr->Count--;
	}

	SListData* MakeData()
	{
		SListData* ptr = (SListData*)MemAlloc(sizeof(SListData));
		if(!ptr) 
			return nullptr;
		new (ptr) SListData();
		return ptr;
	}

	void AttachData(SListData* ptr)
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
				for(SListEntry* Curr = m_ptr->Head; Curr != nullptr; ) {
					SListEntry* Next = Curr->Next;
					Curr->~SListEntry();
					MemFree(Curr);
					Curr = Next;
				}
				MemFree(m_ptr);
			}
			m_ptr = nullptr;
		}
	}
};

FW_NAMESPACE_END