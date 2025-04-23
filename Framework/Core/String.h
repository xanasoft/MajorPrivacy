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
#include "Array.h"

FW_NAMESPACE_BEGIN

class CVariant;

template <typename C>
class String : public AbstractContainer
{
public:
	typedef C CharType;

	struct SStringData
	{
		volatile LONG Refs;
		size_t Length;
		size_t Capacity;
		C Data[1];
	};

	const static size_t NPos = (size_t)-1;

	String(AbstractMemPool* pMem = nullptr, const C* pStr = nullptr, size_t Length = NPos) : AbstractContainer(pMem) { if (pStr) Assign(pStr, Length); }
	String(const String& Other) : AbstractContainer(nullptr) { Assign(Other); }
	String(String&& Other) : AbstractContainer(nullptr) { Move(Other); }
	//template <typename T>
	//String(const String<T>& Other) : AbstractContainer(Other) { Assign(Other.ConstData(), Other.Length()); }
	~String()										{ DetachData(); }

	String& operator=(const String& Other)			{ if(m_ptr != Other.m_ptr || m_pMem != Other.m_pMem) Assign(Other); return *this; }
	String& operator=(String&& Other)				{ if(m_ptr != Other.m_ptr || m_pMem != Other.m_pMem) Move(Other); return *this; }
	String& operator=(const C* pStr)				{ Assign(pStr); return *this; }
	String& operator+=(const String& Other)			{ if (!m_pMem) m_pMem = Other.m_pMem; Append(Other.ConstData(), Other.Length()); return *this; }
	String& operator+=(const C* pStr)				{ Append(pStr); return *this; }
	String& operator+=(const C Char)				{ Append(Char); return *this; }
	String operator+(const String& Other) const		{ String str(*this); str.Append(Other.ConstData(), Other.Length()); return str; }
	String operator+(const C* pStr) const			{ String str(*this); str.Append(pStr); return str; }
	String operator+(const C Char) const			{ String str(*this); str.Append(Char); return str; }

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
	size_t length() const							{ return Length(); } // for STL compatibility
	size_t size() const								{ return Length(); } // for STL compatibility
	bool IsEmpty() const							{ return m_ptr ? m_ptr->Length == 0 : true; }
	bool empty() const								{ return IsEmpty(); } // for STL compatibility
	size_t Capacity() const							{ return m_ptr ? m_ptr->Capacity : 0; }

	void Clear()									{ DetachData(); }
	void clear()									{ Clear(); } // for STL compatibility

	bool Reserve(size_t Capacity)
	{
		if (!m_ptr) // we need to allocate new data
			return Alloc(Capacity);
		else if(m_ptr->Refs > 1 || m_ptr->Capacity < Capacity) // we need to make a copy of the data
			return ReAlloc(Capacity);
		else // we already have exclusive access to the data and the capacity is sufficient
			return true;
	}

	bool SetSize(size_t Length)
	{
		if(!Reserve(Length))
			return false;
		m_ptr->Length = Length;
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

	bool Assign(const C* pStr, size_t uLength = NPos)
	{
		if(uLength == NPos)
			uLength = StrLen(pStr);
		if(!Alloc(uLength)) return false;

		MemCopy(m_ptr->Data, pStr, uLength * sizeof(C));
		m_ptr->Length = uLength;
		m_ptr->Data[m_ptr->Length] = 0;
		return true;
	}

	bool Move(String& Other)
	{
		if (!m_pMem)
			m_pMem = Other.m_pMem;
		else if(m_pMem != Other.m_pMem) // if the containers use different memory pools, we need to make a copy of the data
			return Assign(Other.ConstData(), Other.Length());

		DetachData();
		m_ptr = Other.m_ptr; 
		Other.m_ptr = nullptr;
		return true;
	}

	template <typename T>
	bool Assign(const T* pStr, size_t uLength = NPos)
	{
		if(uLength == NPos)
			uLength = StrLen(pStr);
		if(!Alloc(uLength)) return false;

		for(size_t i = 0; i < uLength; i++)
			m_ptr->Data[i] = (C)pStr[i];
		m_ptr->Length = uLength;
		m_ptr->Data[m_ptr->Length] = 0;
		return true;
	}

	bool Append(const C* pStr, size_t uLength = NPos)
	{
		if (uLength == NPos)
			uLength = StrLen(pStr);
		if (!Reserve((m_ptr ? m_ptr->Length : 0) + uLength)) return false;

		MemCopy(m_ptr->Data + m_ptr->Length, pStr, uLength * sizeof(C));
		m_ptr->Length += uLength;
		m_ptr->Data[m_ptr->Length] = 0;
		return true;
	}

	bool Append(const C Char)
	{
		if (!Reserve((m_ptr ? m_ptr->Length : 0) + 1)) return false;

		m_ptr->Data[m_ptr->Length++] = Char;
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

	int Compare(const String& Other, bool bCaseInsensitive = false) const
	{
		if (!m_ptr || !m_ptr->Data)
			return Other.m_ptr && Other.m_ptr->Data ? -1 : 0;
		if (!Other.m_ptr || !Other.m_ptr->Data)
			return 1;

		if(bCaseInsensitive)
			return CompareI(Other.m_ptr->Data, Other.m_ptr->Length);
		return Compare(Other.m_ptr->Data, Other.m_ptr->Length);
	}

	int Compare(const C* pStr, size_t uLength = NPos) const
	{
		if (!m_ptr || !m_ptr->Data)
			return pStr ? -1 : 0;
		if (!pStr)
			return 1;

		if(uLength == NPos)
			uLength = StrLen(pStr);
		size_t len = Length();
		if(len < uLength) return -1;
		if(len > uLength) return 1;
		if(len == 0) return 0;

		return MemCmp(m_ptr->Data, pStr, len * sizeof(C));
	}

	int CompareI(const C* pStr, size_t uLength = NPos) const
	{
		if (!m_ptr || !m_ptr->Data)
			return pStr ? -1 : 0;
		if (!pStr)
			return 1;

		if(uLength == NPos)
			uLength = StrLen(pStr);
		size_t len = Length();
		if(len < uLength) return -1;
		if(len > uLength) return 1;
		if(len == 0) return 0;

		for (size_t i = 0; i < len; ++i)
		{
			C c1 = m_ptr->Data[i];
			if ((c1 >= (C)'A') && (c1 <= (C)'Z'))
				c1 += 32;

			C c2 = pStr[i];
			if ((c2 >= (C)'A') && (c2 <= (C)'Z'))
				c2 += 32;

			if (c1 < c2) return -1;
			if (c1 > c2) return  1;
		}
		return 0;
	}

	int CompareAt(size_t uStart, const C* pStr, size_t uLength = NPos) const
	{
		if (!m_ptr || !m_ptr->Data)
			return pStr ? -1 : 0;
		if (!pStr)
			return 1;

		if (uStart >= Length())
			return -2;

		if(uLength == NPos)
			uLength = StrLen(pStr);
		size_t len = Length() - uStart;
		if(len < uLength) return -1;
		if(len == 0) return 0;

		return MemCmp(m_ptr->Data + uStart, pStr, len * sizeof(C));
	}

	void Remove(size_t uStart, size_t uLength)
	{
		if (!m_ptr || uStart >= m_ptr->Length)
			return;
		if (uStart + uLength > m_ptr->Length)
			uLength = m_ptr->Length - uStart;

		MakeExclusive();

		size_t tail = m_ptr->Length - (uStart + uLength);
		for (size_t i = 0; i <= tail; ++i)
		{
			m_ptr->Data[uStart + i] = m_ptr->Data[uStart + uLength + i];
		}
		m_ptr->Length -= uLength;
	}

	bool Insert(size_t uOffset, const String& pStr) { return Insert(uOffset, pStr.ConstData(), pStr.Length()); }
	bool Insert(size_t uOffset, const C* pStr, size_t uLen = NPos)
	{
		if (!pStr)
			return true;
		if(uLen == NPos)
			uLen = StrLen(pStr);
		if (uLen == 0)
			return true;
		if (uOffset > Length())
			uOffset = Length();

		MakeExclusive();

		if (!Reserve(m_ptr->Length + uLen))
			return false;

		for (size_t i = m_ptr->Length + 1; i > uOffset; --i)
			m_ptr->Data[i - 1 + uLen] = m_ptr->Data[i - 1];
		
		MemCopy(m_ptr->Data + uOffset, pStr, uLen * sizeof(C));
		m_ptr->Length += uLen;
		return true;
	}

	int ReplaceAll(const String& what, const String& with) { return ReplaceAll(what.ConstData(), what.Length(), with.ConstData(), with.Length()); }
	int ReplaceAll(const C* what, const C* with) { return ReplaceAll(what, StrLen(what), with, StrLen(with)); }
	int ReplaceAll(const C* what, size_t whatLen, const C* with, size_t withLen)
	{
		if (!what || !what[0])
			return 0;

		int count = 0;
		size_t pos = 0;
		while (true)
		{
			pos = Find(what, pos);
			if (pos == NPos)
				break;
			Remove(pos, whatLen);
			Insert(with, pos);
			count++;
			pos += withLen;
		}
		return count;
	}

	static bool IsWhitespace(C ch)
	{
		return (ch == (C)' ' || ch == (C)'\t' || ch == (C)'\n' || ch == (C)'\r');
	}

	String Trim()
	{
		if (!m_ptr || m_ptr->Length == 0)
			return String(m_pMem);

		size_t start = 0;
		size_t end = m_ptr->Length - 1;

		while (start < m_ptr->Length && IsWhitespace(m_ptr->Data[start]))
			start++;

		while (end > start && IsWhitespace(m_ptr->Data[end]))
			end--;

		return SubStr(start, (start <= end ? end - start + 1 : 0));
	}

	Array< String<C> > Split(const String& separator, bool bSkipEmpty = true, bool bTrimEntries = true) const { return Split(separator.ConstData(), bSkipEmpty, bTrimEntries); }
	Array< String<C> > Split(const C* separator, bool bSkipEmpty = true, bool bTrimEntries = true) const
	{
		Array< String<C> > tokens(m_pMem);

		// If the separator is null or empty, return the original string as the sole token.
		if (!separator || !separator[0])
		{
			tokens += *this;
			return tokens;
		}

		size_t sepLen = StrLen(separator);
		size_t startPos = 0;
		size_t pos = 0;

		while (startPos < Length())
		{
			// Find next occurrence of the separator.
			pos = Find(separator, startPos);
			if (pos == NPos)
				pos = Length();

			// Create a token from the current segment.
			size_t tokenLen = pos - startPos;
			String<C> token(m_pMem, ConstData() + startPos, tokenLen);

			// Optionally trim the token.
			if (bTrimEntries)
				token = token.Trim();

			// Add token only if not empty (when skipping empties).
			if (!(bSkipEmpty && token.Length() == 0))
				tokens += token;

			// Move start past the separator.
			startPos = pos + sepLen;

			// If no separator was found, we are done.
			if (pos == Length())
				break;
		}
		return tokens;
	}

	static String Join(const Array< String<C> >& List, const String& separator) { return Join(List, separator.ConstData()); }
	static String Join(const Array< String<C> >& List, const C* separator)
	{
		String<C> result;

		for (size_t i = 0; i < List.Count(); i++)
		{
			if (i > 0)
				result += separator;
			result += List[i];
		}
		return result;
	}

	void MakeUpper()
	{
		MakeExclusive();
		if (!m_ptr || !m_ptr->Data)
			return;
		for (size_t i = 0; i < m_ptr->Length; i++) {
			if((m_ptr->Data[i] >= (C)'a') && (m_ptr->Data[i] <= (C)'z'))
				m_ptr->Data[i] -= 32;
		}
	}

	void MakeLower()
	{
		MakeExclusive();
		if (!m_ptr || !m_ptr->Data)
			return;
		for (size_t i = 0; i < m_ptr->Length; i++) {
			if ((m_ptr->Data[i] >= (C)'A') && (m_ptr->Data[i] <= (C)'Z'))
				m_ptr->Data[i] += 32;
		}
	}

	size_t Find(const String& Str, size_t uStart = 0) const
	{
		return Find(Str.ConstData(), uStart);
	}

	size_t Find(const C* pStr, size_t uStart = 0) const
	{
		if (!pStr || uStart >= Length())
			return NPos;
		size_t len = StrLen(pStr);
		if (len == 0)
			return uStart;
		if (len > Length() - uStart)
			return NPos;
		for (size_t i = uStart; i <= Length() - len; i++)
			if (MemCmp(m_ptr->Data + i, pStr, len * sizeof(C)) == 0)
				return i;
		return NPos;
	}

	size_t Find(const C Char, size_t uStart = 0) const
	{
		if (uStart >= Length())
			return NPos;
		for (size_t i = uStart; i < Length(); i++)
			if (m_ptr->Data[i] == Char)
				return i;
		return NPos;
	}

	String SubStr(size_t uStart, size_t uLength = NPos) const
	{
		if (uLength == NPos)
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
	friend FW::CVariant;

	SStringData* m_ptr = nullptr;

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
		SStringData* ptr = (SStringData*)MemAlloc(sizeof(SStringData) + Capacity * sizeof(C));
		if(!ptr) 
			return nullptr;
		ptr->Refs = 0;
		ptr->Length = 0;
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
				MemFree(m_ptr);
			m_ptr = nullptr;
		}
	}
};

typedef String<char> StringA;
typedef String<wchar_t> StringW;

FW_NAMESPACE_END