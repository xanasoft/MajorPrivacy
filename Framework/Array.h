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
class Array : public AbstractContainer
{
public:
	Array(AbstractMemPool* pMem = nullptr) : AbstractContainer(pMem) {}
	Array(const Array& Other) : AbstractContainer(Other) { Assign(Other); }
	Array(Array&& Other) : AbstractContainer(Other)	{ m_ptr = Other.m_ptr; Other.m_ptr = nullptr; }
	~Array()										{ DetachData(); }

	Array& operator=(const Array& Other)			{ if(m_ptr == Other.m_ptr) return *this; Assign(Other); return *this; }
	Array& operator=(Array&& Other)					{ if(m_ptr == Other.m_ptr) return *this; DetachData(); m_pMem = Other.m_pMem; m_ptr = Other.m_ptr; Other.m_ptr = nullptr; return *this; }
	Array& operator+=(const Array& Other)			{ if (!m_pMem) m_pMem = Other.m_pMem; Append(Other.Data(), Other.Count()); return *this; } 
	Array& operator+=(const V& Value)				{ Append(&Value, 1); return *this; }
	Array operator+(const Array& Other) const		{ Array str(*this); str.Append(Other.Data(), Other.Count()); return str; }
	Array operator+(const V& Value) const			{ Array str(*this); str.Append(&Value, 1); return str; }

	const V& operator[](size_t Index) const			{ return m_ptr->Data[Index]; }
	V& operator[](size_t Index)						{ MakeExclusive(); return m_ptr->Data[Index]; }

	const V* Data() const							{ return m_ptr ? m_ptr->Data : nullptr; }
	size_t Count() const							{ return m_ptr ? m_ptr->Count : 0; }
	bool IsEmpty() const							{ return m_ptr ? m_ptr->Count == 0 : true; }
	size_t Capacity() const							{ return m_ptr ? m_ptr->Capacity : 0; }

	void Clear()									{ DetachData(); }

	bool Reserve(size_t Capacity)
	{
		if (!m_ptr) // we need to allocate new data
			return Alloc(Capacity);
		else if (m_ptr->Refs > 1 || m_ptr->Capacity < Capacity) // we need to make a copy of the data
		{
			size_t ExtraCapacity = Capacity / 2;
			if (ExtraCapacity > 1000)
				ExtraCapacity = ExtraCapacity;
			return ReAlloc(Capacity + ExtraCapacity);
		}
		else // we already have exclusive access to the data and the capacity is sufficient
			return true;
	}

	bool MakeExclusive()							{ return MakeExclusive(nullptr); }

	bool Assign(const Array& Other)
	{
		if (!m_pMem)
			m_pMem = Other.m_pMem;
		else if(m_pMem != Other.m_pMem) // if the containers use different memory pools, we need to make a copy of the data
			return Assign(Other.Data(), Other.Count());
		AttachData(Other.m_ptr);
		return true;
	}

	bool Assign(const V* pData, size_t Count)
	{
		if(!Alloc(Count)) return false;

		for (size_t i = 0; i < Count; i++)
			new (m_ptr->Data + i) V(pData[i]);
		m_ptr->Count = Count;
		return true;
	}

	bool Append(const V& Data)						{ return Append(&Data, 1); }

	bool Append(const V* pData, size_t Count)
	{
		if (!Reserve((m_ptr ? m_ptr->Count : 0) + Count)) return false;

		for (size_t i = 0; i < Count; i++)
			new (m_ptr->Data + m_ptr->Count + i) V(pData[i]);
		m_ptr->Count += Count;
		return true;
	}

	// todo make this check the bounds
	const V& GetValue(size_t Index) const			{ return m_ptr->Data[Index]; }
	V& GetValue(size_t Index)						{ MakeExclusive(); return m_ptr->Data[Index]; }

	// find
	// mid
	// remove
	// insert

	const V* LastPtr() const						{ return m_ptr ? m_ptr->Data + m_ptr->Count - 1 : nullptr; }
	V* LastPtr()									{ return m_ptr ? m_ptr->Data + m_ptr->Count - 1 : nullptr; }
	const V* FirstPtr() const						{ return m_ptr ? m_ptr->Data : nullptr; }
	V* FirstPtr()									{ return m_ptr ? m_ptr->Data : nullptr; }


	// Support for range-based for loops
	class Iterator {
	public:
		Iterator(V* pData = nullptr, size_t uCount = 0) : pData(pData), uRemCount(uCount) {}

		Iterator& operator++() {
			if(uRemCount > 0) uRemCount--;
			if(uRemCount > 0) pData++;
			else pData = nullptr;
			return *this;
		}

		Iterator& operator+=(size_t uCount) {
			if(uRemCount >= uCount) uRemCount -= uCount;
			if(uRemCount >= 0) pData += uCount;
			else pData = nullptr;
			return *this;
		}

		V& operator*() const { return *pData; }
		V* operator->() const { return pData; }

		bool operator!=(const Iterator& other) const { return pData != other.pData; }
		bool operator==(const Iterator& other) const { return pData == other.pData; }

	protected:
		friend Array;
		V* pData = nullptr;
		size_t uRemCount = 0;
	};

	Iterator begin() const							{ return m_ptr ? Iterator(m_ptr->Data, m_ptr->Count) : end(); }
	Iterator end() const							{ return Iterator(); }

	Iterator erase(Iterator I)
	{
		if (!I.pData || !MakeExclusive(&I))
			return end();
		for (size_t i = I.pData - m_ptr->Data; i < m_ptr->Count - 1; i++)
			m_ptr->Data[i] = m_ptr->Data[i + 1];
		m_ptr->Count--;
		return I;
	}

protected:

	struct SArrayData
	{
		volatile LONG Refs = 0;
		size_t Count = 0;
		size_t Capacity = 0;
#pragma warning( push )
#pragma warning( disable : 4200)
		V Data[0];
#pragma warning( pop )
	}* m_ptr = nullptr;

	bool MakeExclusive(Iterator* pI)
	{ 
		if (!m_ptr)
			return Alloc(1);
		else if(m_ptr->Refs > 1) 
			return ReAlloc(m_ptr->Capacity, pI);
		return true; 
	}

	bool Alloc(size_t Capacity)
	{
		SArrayData* ptr = MakeData(Capacity);
		AttachData(ptr);
		return ptr != nullptr;
	}

	bool ReAlloc(size_t Capacity, Iterator* pI = nullptr)
	{
		SArrayData* ptr = MakeData(m_ptr->Capacity > Capacity ? m_ptr->Capacity : Capacity);
		if (!ptr) return false;
		for (size_t i = 0; i < m_ptr->Count; i++) {
			new (ptr->Data + i) V(m_ptr->Data[i]);
			// we need to update the iterator if it points to the current entry
			if (pI && m_ptr->Data + i == pI->pData) { // todo improve
				pI->pData = ptr->Data + i;
				pI = NULL;
			}
		}
		ptr->Count = m_ptr->Count;
		AttachData(ptr);
		return true;
	}

	SArrayData* MakeData(size_t Capacity)
	{
		if (!m_pMem) 
			return nullptr;
		SArrayData* ptr = (SArrayData*)m_pMem->Alloc(sizeof(SArrayData) + Capacity * sizeof(V));
		if(!ptr) 
			return nullptr;
		new (ptr) SArrayData();
		ptr->Capacity = Capacity;
		return ptr;
	}

	void AttachData(SArrayData* ptr)
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
				for(size_t i = 0; i < m_ptr->Count; i++)
					m_ptr->Data[i].~V();
				m_pMem->Free(m_ptr);
			}
			m_ptr = nullptr;
		}
	}
};

FW_NAMESPACE_END