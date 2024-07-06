/*
* Copyright (c) 2023-2024 David Xanatos, xanasoft.com
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

template <typename C>
class String : public AbstractContainer
{
public:
	typedef C CharType;

	String(AbstractMemPool* pMem = nullptr, const C* pStr = nullptr, size_t Length = -1) : AbstractContainer(pMem) { if (pStr) Assign(pStr, Length); }
	String(const String& Other) : AbstractContainer(Other) { Assign(Other); }
	String(String&& Other) : AbstractContainer(Other) { m_ptr = Other.m_ptr; Other.m_ptr = nullptr; }
	template <typename T>
	String(const String<T>& Other) : AbstractContainer(Other) { Assign(Other.ConstData(), Other.Length()); }
	~String()										{ DetachData(); }

	String& operator=(const String& Other)			{ if(m_ptr == Other.m_ptr) return *this; Assign(Other); return *this; }
	String& operator=(String&& Other)				{ if(m_ptr == Other.m_ptr) return *this; DetachData(); m_pMem = Other.m_pMem; m_ptr = Other.m_ptr; Other.m_ptr = nullptr; return *this; }
	String& operator=(const C* pStr)				{ Assign(pStr); return *this; }
	String& operator+=(const String& Other)			{ if (!m_pMem) m_pMem = Other.m_pMem; Append(Other.ConstData(), Other.Length()); return *this; }
	String& operator+=(const C* pStr)				{ Append(pStr); return *this; }
	String operator+(const String& Other) const		{ String str(*this); str.Append(Other.ConstData(), Other.Length()); return str; }
	String operator+(const C* pStr) const			{ String str(*this); str.Append(pStr); return str; }

	bool operator== (const String& Other) const		{ return Compare(Other) == 0; }
	bool operator!= (const String& Other) const		{ return Compare(Other) != 0; }
	bool operator>= (const String& Other) const		{ return Compare(Other) >= 0; }
	bool operator<= (const String& Other) const		{ return Compare(Other) <= 0; }
	bool operator> (const String& Other) const		{ return Compare(Other) > 0; }
	bool operator< (const String& Other) const		{ return Compare(Other) < 0; }

	bool operator== (const C* pStr) const			{ return Compare(pStr) == 0; }
	bool operator!= (const C* pStr) const			{ return Compare(pStr) != 0; }
	bool operator>= (const C* pStr) const			{ return Compare(pStr) >= 0; }
	bool operator<= (const C* pStr) const			{ return Compare(pStr) <= 0; }
	bool operator> (const C* pStr) const			{ return Compare(pStr) > 0; }
	bool operator< (const C* pStr) const			{ return Compare(pStr) < 0; }

	const C& operator[](size_t Index) const			{ return m_ptr->Data[Index]; }
	C& operator[](size_t Index)						{ MakeExclusive(); return m_ptr->Data[Index]; }

	C* Data()										{ MakeExclusive(); return m_ptr ? m_ptr->Data : nullptr; }
	const C* ConstData() const						{ return m_ptr ? m_ptr->Data : nullptr; }
	size_t Length() const							{ return m_ptr ? m_ptr->Length : 0; }
	bool IsEmpty() const							{ return m_ptr ? m_ptr->Length == 0 : true; }
	size_t Capacity() const							{ return m_ptr ? m_ptr->Capacity : 0; }

	void Clear()									{ DetachData(); }

	bool Reserve(size_t Capacity)
	{
		if (!m_ptr) // we need to allocate new data
			return Alloc(Capacity);
		else if(m_ptr->Refs > 1 || m_ptr->Capacity < Capacity) // we need to make a copy of the data
			return ReAlloc(Capacity);
		else // we already have exclusive access to the data and the capacity is sufficient
			return true;
	}

	bool MakeExclusive()							{ if(m_ptr && m_ptr->Refs > 1) return ReAlloc(m_ptr->Capacity); return true; }

	bool Assign(const String& Other)
	{
		if (!m_pMem)
			m_pMem = Other.m_pMem;
		else if(m_pMem != Other.m_pMem) // if the containers use different memory pools, we need to make a copy of the data
			return Assign(Other.ConstData(), Other.Length());
		AttachData(Other.m_ptr);
		return true;
	}

	bool Assign(const C* pStr, size_t Length = -1)
	{
		if(Length == -1)
			Length = StrLen(pStr);
		if(!Alloc(Length)) return false;

		MemCopy(m_ptr->Data, pStr, Length * sizeof(C));
		m_ptr->Length = Length;
		m_ptr->Data[m_ptr->Length] = 0;
		return true;
	}

	template <typename T>
	bool Assign(const T* pStr, size_t Length = -1)
	{
		if(Length == -1)
			Length = StrLen(pStr);
		if(!Alloc(Length)) return false;

		for(size_t i = 0; i < Length; i++)
			m_ptr->Data[i] = (C)pStr[i];
		m_ptr->Length = Length;
		m_ptr->Data[m_ptr->Length] = 0;
		return true;
	}

	bool Append(const C* pStr, size_t Length = -1)
	{
		if (Length == -1)
			Length = StrLen(pStr);
		if (!Reserve((m_ptr ? m_ptr->Length : 0) + Length)) return false;

		MemCopy(m_ptr->Data + m_ptr->Length, pStr, Length * sizeof(C));
		m_ptr->Length += Length;
		m_ptr->Data[m_ptr->Length] = 0;
		return true;
	}

	template <typename T>
	static size_t StrLen(const T* pStr)
	{
		if (!pStr) return 0;
		const T* pBegin = pStr;
		for (; *pStr; ++pStr);
		return pStr - pBegin;
	}

	int Compare(const String& Other) const
	{
		if (!m_ptr || !m_ptr->Data)
			return Other.m_ptr && Other.m_ptr->Data ? -1 : 0;
		if (!Other.m_ptr || !Other.m_ptr->Data)
			return 1;

		size_t len = Length();
		size_t lenO = Other.Length();
		if(len < lenO) return -1;
		if(len > lenO) return 1;
		if(len == 0) return 0;

		return MemCmp(m_ptr->Data, Other.m_ptr->Data, len * sizeof(C));
	}

	int Compare(const C* pStr) const
	{
		if (!m_ptr || !m_ptr->Data)
			return pStr ? -1 : 0;
		if (!pStr)
			return 1;

		size_t len = Length();
		size_t lenO = StrLen(pStr);
		if(len < lenO) return -1;
		if(len > lenO) return 1;
		if(len == 0) return 0;

		return MemCmp(m_ptr->Data, pStr, len * sizeof(C));
	}

	// remove
	// insert
	// replace
	// split
	// trim
	// tolower
	// toupper

	size_t Find(const String& Str, size_t uStart = 0) const
	{
		return Find(Str.ConstData(), uStart);
	}

	size_t Find(const C* pStr, size_t uStart = 0) const
	{
		if (!pStr || uStart >= Length())
			return (size_t)-1;
		size_t len = StrLen(pStr);
		if (len == 0)
			return uStart;
		if (len > Length() - uStart)
			return (size_t)-1;
		for (size_t i = uStart; i <= Length() - len; i++)
			if (MemCmp(m_ptr->Data + i, pStr, len * sizeof(C)) == 0)
				return i;
		return (size_t)-1;
	}

	size_t Find(const C Char, size_t uStart = 0) const
	{
		if (uStart >= Length())
			return (size_t)-1;
		for (size_t i = uStart; i < Length(); i++)
			if (m_ptr->Data[i] == Char)
				return i;
		return (size_t)-1;
	}

	String SubStr(size_t uStart, size_t uLength = -1) const
	{
		if (uLength == -1)
			uLength = Length() - uStart;
		if (uStart >= Length() || uLength == 0)
			return String(m_pMem);
		if (uStart + uLength > Length())
			uLength = Length() - uStart;
		return String(m_pMem, ConstData() + uStart, uLength);
	}

	C At(size_t Index) const
	{
		return Index < Length() ? m_ptr->Data[Index] : 0;
	}

	// String View

	template <typename T>
	void ToNtString(T* Str) const
	{
		ASSERT(sizeof(C) == sizeof(Str->Buffer[0]));
		Str->Length = (USHORT)(Length() * sizeof(C));
		Str->MaximumLength = (USHORT)((Capacity() + 1) * sizeof(C));
		Str->Buffer = (C*)ConstData();
	}

protected:

	struct SStringData
	{
		volatile LONG Refs = 0;
		size_t Length = 0;
		size_t Capacity = 0;
		C Data[1];
	}* m_ptr = nullptr;

	bool Alloc(size_t Capacity)
	{
		SStringData* ptr = MakeData(Capacity);
		AttachData(ptr);
		return ptr != nullptr;
	}

	bool ReAlloc(size_t Capacity)
	{
		SStringData* ptr = MakeData(m_ptr->Capacity > Capacity ? m_ptr->Capacity : Capacity);
		if (!ptr) return false;
		MemCopy(ptr->Data, m_ptr->Data, (m_ptr->Length + 1) * sizeof(C));
		ptr->Length = m_ptr->Length;
		AttachData(ptr);
		return true;
	}

	SStringData* MakeData(size_t Capacity)
	{
		if (!m_pMem) 
			return nullptr;
		SStringData* ptr = (SStringData*)m_pMem->Alloc(sizeof(SStringData) + Capacity * sizeof(C));
		if(!ptr) 
			return nullptr;
		new (ptr) SStringData();
		ptr->Capacity = Capacity;
		ptr->Data[0] = 0;
		return ptr;
	}

	void AttachData(SStringData* ptr)
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
			if (InterlockedDecrement(&m_ptr->Refs) == 0)
				m_pMem->Free(m_ptr);
			m_ptr = nullptr;
		}
	}
};

typedef String<char> StringA;
typedef String<wchar_t> StringW;

FW_NAMESPACE_END