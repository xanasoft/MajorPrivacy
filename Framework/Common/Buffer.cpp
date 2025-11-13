//#include "pch.h"

// machine types
#include "../Core/Types.h"
#include "../Core/Header.h"

#include "Buffer.h"
//#include "../zlib/zlib.h"

#ifndef NO_CRT
#include <memory>
#endif

#define BUFFER_MAX_PREALLOC 0x08000000 // 128 MB

CBuffer::CBuffer(FW::AbstractMemPool* pMemPool)
	: FW::AbstractContainer(pMemPool) 
{
	m_uFlags = 0;
	m_uPreAllocated = 0;
	m_p.Void = nullptr;
	m_uSize = 0;
	m_uPosition = 0;
}

CBuffer::CBuffer(const CBuffer& Other) 
	: CBuffer(Other.Allocator()) 
{ 
	Assign(Other); 
}

CBuffer::CBuffer(CBuffer&& Other)
	: FW::AbstractContainer(Other.Allocator())
{
	m_uFlags = Other.m_uFlags;
	m_uPreAllocated = Other.m_uPreAllocated;
	m_p.Void = Other.m_p.Void;
	Other.m_p.Void = nullptr;
	m_uSize = Other.m_uSize;
	m_uPosition = Other.m_uPosition;
}

CBuffer& CBuffer::operator=(CBuffer&& Other)
{
	if (this == &Other)
		return *this;
	DetachData();
	
	m_pMem = Other.m_pMem;
	m_uFlags = Other.m_uFlags;
	m_uPreAllocated = Other.m_uPreAllocated;
	m_p.Void = Other.m_p.Void;
	Other.m_p.Void = nullptr;
	m_uSize = Other.m_uSize;
	m_uPosition = Other.m_uPosition;
	
	return *this;
}

CBuffer::~CBuffer()
{
	DetachData();
}

void CBuffer::Clear()
{
	DetachData();
	m_uSize = 0;
	m_uPosition = 0;
}

bool CBuffer::Assign(const CBuffer& Other)
{
	if (this == &Other)
		return true;
		
	//if (!m_pMem)
	//	m_pMem = Other.m_pMem;
	if (m_pMem != Other.m_pMem || !Other.IsContainerized()) // if the containers use different memory pools, we need to make a copy of the data
	{
		if(!CopyBuffer(Other.GetBuffer(), Other.GetSize()))
			return false;
		m_bFixed = Other.m_bFixed;
		m_bReadOnly = Other.m_bReadOnly;
	}
	else
	{
		AttachData(Other.m_p.Buffer);
		m_uSize = Other.m_uSize;
		m_uFlags = Other.m_uFlags;
	}

	m_uPosition = Other.m_uPosition;
	return true;
}

bool CBuffer::AllocBuffer(size_t uLength, bool bUsed, bool bFixed, bool bDetachable)
{
	Clear();
	if (bDetachable)
	{
		m_p.Data = (byte*)MemAlloc(uLength);
		m_bDetachable = true;
		if (!bUsed) {
			ASSERT(uLength < 0xFFFFFFFFull);
			m_uPreAllocated = (uint32)uLength;
		}
	}
	else
	{
		SBuffer* pNewBuffer = MakeData(uLength);
		if (!pNewBuffer)
			return false;
		AttachData(pNewBuffer);
	}
	if (bUsed)
		m_uSize = uLength;
	m_bFixed = bFixed;
	m_uPosition = 0;
	return true;
}

bool CBuffer::SetBuffer(void* pBuffer, size_t uSize, bool bDerived, bool bFixed)
{
	Clear();
	if (pBuffer)
	{
		if(bDerived)
			m_bDerived = true;
		else
			m_bDetachable = true;
		m_p.Data = (byte*)pBuffer;
	}
	else
	{
		m_bDetachable = true;
		m_p.Data = (byte*)MemAlloc(uSize);
	}
	m_uSize = uSize;
	m_bFixed = !!m_bDerived || bFixed;
	return m_p.Data != nullptr;
}

bool CBuffer::CopyBuffer(const void* pBuffer, size_t uSize)
{
	ASSERT(!m_p.Void || GetBuffer() + m_uSize < (byte*)pBuffer || GetBuffer() > (byte*)pBuffer + uSize);
	if(!AllocBuffer(uSize, true))
		return false;
	MemCopy(GetBuffer(), pBuffer, uSize);
	return true;
}

bool CBuffer::ReAllocBuffer(size_t uCapacity)
{
	if (m_bDetachable)
	{
		byte* pNewData = (byte*)MemAlloc(uCapacity);
		if (!pNewData)
			return false;
		if (m_p.Data && pNewData)
			MemCopy(pNewData, m_p.Data, m_uSize);
		DetachData();
		m_p.Data = pNewData;
		m_bDetachable = true;
	}
	else
	{
		SBuffer* pNewBuffer = MakeData(uCapacity);
		if (!pNewBuffer)
			return false;
		if (m_p.Buffer && pNewBuffer)
			MemCopy(pNewBuffer->Data, GetBuffer(), m_uSize); // Use GetBuffer, can be m_bDerived
		AttachData(pNewBuffer);
	}
	return true;
}

bool CBuffer::SetSize(size_t uSize, bool bExpend, size_t uPreAlloc)
{
	size_t uCapacity = GetCapacity();

	// check if we need to alocate more space
	if(uPreAlloc == -1 || uCapacity < uSize + uPreAlloc)
	{
		if(!bExpend || m_bReadOnly) {
			ASSERT(0);
			return false;
		}

		uCapacity = uSize;
		if (uPreAlloc != -1)
			uCapacity += uPreAlloc;

		ReAllocBuffer(uCapacity);
	}

	m_uSize = uSize;
	if(m_uSize < m_uPosition)
		m_uPosition = m_uSize;

	// check if we have to much unused space
	if (uCapacity - uSize > BUFFER_MAX_PREALLOC && uPreAlloc < BUFFER_MAX_PREALLOC) {
		uCapacity = BUFFER_MAX_PREALLOC + uSize;
		ReAllocBuffer(uCapacity);
	}
	else if (m_bDetachable) {
		ASSERT(uPreAlloc < 0xFFFFFFFFull);
		m_uPreAllocated = (uint32)(uCapacity - uSize);
	}

	return true;
}

bool CBuffer::PrepareWrite(size_t uOffset, size_t uLength)
{
	if(m_bReadOnly)
		return false;

	size_t uEndOffset = uOffset + uLength;
	size_t uCapacity = GetCapacity();

	// check if there is enough space allocated for the data or if we need to copy on write
	if(uEndOffset > uCapacity || (IsContainerized() && m_p.Buffer->Refs > 1))
	{
		size_t uPreAlloc = 0;

		if (m_bFixed)
			return false; // fail if reallocation is not allowed

		if (uEndOffset > uCapacity)
			uPreAlloc = max(max(uLength * 2, GetSize()), 16);
		//  uPreAlloc = max(min(uLength*10,GetSize()/2), 16);

		return SetSize(uEndOffset, true, uPreAlloc);
	}

	// check if we are overwriting data or adding data, if the latter then increase the size accordingly
	if (uEndOffset > m_uSize)
	{
		if (m_bDetachable)
		{
			ASSERT(m_uPreAllocated >= uEndOffset - m_uSize);
			m_uPreAllocated -= (uint32)(uEndOffset - m_uSize);
		}
		m_uSize = uEndOffset;
	}
	return true;
}

byte* CBuffer::GetBuffer(bool bDetatch)	
{
	if(!bDetatch)
		return IsContainerized() ? m_p.Buffer->Data : m_p.Data;

	if(IsDetachable())
	{
		byte* pBuffer = m_p.Data;
		m_p.Data = nullptr;
		m_uSize = 0;
		return pBuffer;
	}
	else if(IsContainerized())
	{
		byte* pBuffer = (byte*)MemAlloc(m_uSize);
		MemCopy(pBuffer, m_p.Buffer->Data, m_uSize);
		return pBuffer;
	}
	return nullptr;
}

int CBuffer::Compare(const CBuffer& Buffer) const
{
	size_t uSizeL = GetSize();
	size_t uSizeR = Buffer.GetSize();
	if (uSizeL < uSizeR)
		return -1;
	if (uSizeL > uSizeR)
		return 1;
	return memcmp(GetBuffer(), Buffer.GetBuffer(), uSizeL);
}

#ifdef HAS_ZLIB
bool CBuffer::Pack()
{
	ASSERT(m_uSize + 300 < 0xFFFFFFFF);

	uLongf uNewSize = (uLongf)(m_uSize + 300);
	byte* Buffer = new byte[uNewSize];
	int Ret = compress2(Buffer,&uNewSize,m_pBuffer,(uLongf)m_uSize,Z_BEST_COMPRESSION);
	if (Ret != Z_OK || m_uSize < uNewSize) // does the compression helped?
	{
		delete Buffer;
		return false;
	}
	SetBuffer(Buffer, uNewSize);
	return true;
}

bool CBuffer::Unpack()
{
	ASSERT(m_uSize*10+300 < 0xFFFFFFFF);

	byte* Buffer = nullptr;
	uLongf uAllocSize = (uLongf)(m_uSize*10+300);
	uLongf uNewSize = 0;
	int Ret = 0;
	do 
	{
		delete Buffer;
		Buffer = new byte[uAllocSize];
		uNewSize = uAllocSize;
		Ret = uncompress(Buffer,&uNewSize,m_pBuffer,(uLongf)m_uSize);
		uAllocSize *= 2; // size for the next try if needed
	} 
	while (Ret == Z_BUF_ERROR && uAllocSize < Max(MB2B(16), m_uSize*100));	// do not allow the unzip buffer to grow infinetly, 
	// assume that no packetcould be originaly larger than the UnpackLimit nd those it must be damaged
	if (Ret != Z_OK)
	{
		delete Buffer;
		return false;
	}
	SetBuffer(Buffer, uNewSize);
	return true;
}
#endif

///////////////////////////////////////////////////////////////////////////////////
// Position based Data Operations

bool CBuffer::SetPosition(size_t uPosition) const
{
	size_t uSize = GetSize();
	if(uPosition == -1)
		uPosition = uSize;
	else if(uPosition > uSize)
		return false;

	m_uPosition = uPosition;

	return true;
}

const byte* CBuffer::ReadData(size_t uLength) const
{
	size_t uSize = GetSize();
	if(uLength == -1)
		uLength = GetSizeLeft();

	if(m_uPosition + uLength > uSize)
		return nullptr;

	const byte* pData = GetBuffer() + m_uPosition;
	m_uPosition += uLength;

	return pData;
}

byte* CBuffer::WriteData(const void* pData, size_t uLength)
{
	if(!PrepareWrite(m_uPosition, uLength))
		return nullptr;

	if(pData)
	{
		ASSERT(GetBuffer() + m_uPosition + uLength < (byte*)pData || GetBuffer() + m_uPosition > (byte*)pData + uLength);
		MemCopy(GetBuffer() + m_uPosition, pData, uLength);
	}
	pData = GetBuffer() + m_uPosition;
	m_uPosition += uLength;

	return (byte*)pData;
}

///////////////////////////////////////////////////////////////////////////////////
// Offset based Data Operations

const byte* CBuffer::GetDataAt(size_t uOffset, size_t uLength) const
{
	size_t uSize = GetSize();
	if(uOffset + uLength > uSize)
		return nullptr;

	return GetBuffer() + uOffset;
}

byte* CBuffer::SetDataAt(size_t uOffset, void* pData, size_t uLength)
{
	size_t uSize = GetSize();
	if(uOffset == -1)
		uOffset = uSize;

	ASSERT(uOffset <= uSize);

	if(!PrepareWrite(uOffset, uLength))
		return nullptr;

	if(pData)
	{
		ASSERT(GetBuffer() + uOffset + uLength < (byte*)pData || GetBuffer() + uOffset > (byte*)pData + uLength);
		MemCopy(GetBuffer() + uOffset, pData, uLength);
	}
	else
		MemZero(GetBuffer() + uOffset, uLength);

	return GetBuffer() + uOffset;
}

byte* CBuffer::InsertData(size_t uOffset, void* pData, size_t uLength)
{
	size_t uSize = GetSize();
	ASSERT(uOffset <= uSize);

	if(!PrepareWrite(uSize, uLength))
		return nullptr;

	MemMove(GetBuffer() + uOffset + uLength, GetBuffer() + uOffset, uSize - uOffset);

	if(pData)
	{
		ASSERT(GetBuffer() + uOffset + uLength < (byte*)pData || GetBuffer() + uOffset > (byte*)pData + uLength);
		MemCopy(GetBuffer() + uOffset, pData, uLength);
	}

	return GetBuffer() + uOffset;
}

byte* CBuffer::ReplaceData(size_t uOffset, size_t uOldLength, void* pData, size_t uNewLength)
{
	if(!RemoveData(uOffset, uOldLength))
		return nullptr;
	if(!InsertData(uOffset, pData, uNewLength))
		return nullptr;
	return GetBuffer() + uOffset;
}

bool CBuffer::AppendData(const void* pData, size_t uLength)
{
	ASSERT(pData);

	size_t uSize = GetSize();
	size_t uOffset = uSize; // append to the very end
	if(!PrepareWrite(uOffset, uLength))
	{
		ASSERT(0); // append must usually always succeed
		return false;
	}

	//ASSERT(GetBuffer() + uOffset + uLength < (byte*)pData || GetBuffer() + uOffset > (byte*)pData + uLength);
	MemCopy(GetBuffer() + uOffset, pData, uLength);
	return true;
}

bool CBuffer::ShiftData(size_t uOffset)
{
	if(!m_p.Void || uOffset > m_uSize)
	{
		ASSERT(0); // shift must always success
		return false;
	}

	m_uSize -= uOffset;

	MemMove(GetBuffer(), GetBuffer() + uOffset, m_uSize);

	if(m_uPosition > uOffset)
		m_uPosition -= uOffset;
	else
		m_uPosition = 0;
	return true;
}

bool CBuffer::RemoveData(size_t uOffset, size_t uLength)
{
	if(m_bReadOnly)
		return false;

	size_t uSize = GetSize();
	if(uLength == -1)
		uLength = uSize;
	else if(uOffset + uLength > uSize)
		return false;

	MemMove(GetBuffer() + uOffset, GetBuffer() + uOffset + uLength, uSize - uOffset - uLength);

	m_uSize -= uLength;

	return true;
}

///////////////////////////////////////////////////////////////////////////////////
// Memory Operations

CBuffer::SBuffer* CBuffer::MakeData(size_t uCapacity)
{
	SBuffer* pBuffer = (SBuffer*)MemAlloc(sizeof(SBuffer) + uCapacity);
	if(!pBuffer) 
		return nullptr;
	memset(pBuffer, 0, sizeof(SBuffer));
	pBuffer->uCapacity = uCapacity;
	return pBuffer;
}

void CBuffer::AttachData(SBuffer* pBuffer)
{
	DetachData();

	if (pBuffer) {
		m_p.Buffer = pBuffer;
		InterlockedIncrement(&m_p.Buffer->Refs);
	}
}

void CBuffer::DetachData()
{
	if (!m_p.Void)
		return;

	if (IsContainerized()) {
		if (InterlockedDecrement(&m_p.Buffer->Refs) == 0)
			MemFree(m_p.Buffer);
	}
	else if (m_bDetachable)
	{
		MemFree(m_p.Data);
	}
	// else m_bDerived // nothing to do
	m_uFlags = 0;
	m_uPreAllocated = 0;
	m_p.Void = nullptr;
}

///////////////////////////////////////////////////////////////////////////////////
// Specifyed Data Operations

bool CBuffer::WriteString(const FW::StringW& String, EStrSet Set, EStrLen Len)
{
	FW::StringA RawString;
	if(Set == eAscii)
		RawString = ToAscii(Allocator(), String.ConstData(), String.Length());
	else
		RawString = ToUtf8(Allocator(), String.ConstData(), String.Length());

	if(Set == eUtf8_BOM)
		RawString.Insert(0, "\xEF\xBB\xBF");

	size_t uLength = RawString.Length();
	ASSERT(uLength < 0xFFFFFFFF);

	bool bOk = true;
	switch(Len)
	{
	case e8Bit:		ASSERT(RawString.Length() < 0xFF);			bOk = WriteValue<uint8>((uint8)uLength);	break;
	case e16Bit:	ASSERT(RawString.Length() < 0xFFFF);		bOk = WriteValue<uint16>((uint16)uLength);	break;
	case e32Bit:	ASSERT(RawString.Length() < 0xFFFFFFFF);	bOk = WriteValue<uint32>((uint32)uLength);	break;
	}

	return bOk && WriteData(RawString.ConstData(), RawString.Length());
}

bool CBuffer::WriteString(const FW::StringA& String, EStrLen Len)
{
	size_t uLength = String.Length();
	ASSERT(uLength < 0xFFFFFFFF);

	bool bOk = true;
	switch(Len)
	{
	case e8Bit:		ASSERT(String.Length() < 0xFF);			bOk = WriteValue<uint8>((uint8)uLength);	break;
	case e16Bit:	ASSERT(String.Length() < 0xFFFF);		bOk = WriteValue<uint16>((uint16)uLength);	break;
	case e32Bit:	ASSERT(String.Length() < 0xFFFFFFFF);	bOk = WriteValue<uint32>((uint32)uLength);	break;
	}

	return bOk && WriteData(String.ConstData(), String.Length());
}

FW::StringW CBuffer::ReadString(EStrSet Set, EStrLen Len) const
{
	size_t uRawLength = 0;
	switch(Len)
	{
	case e8Bit:		uRawLength = ReadValue<uint8>();	break;
	case e16Bit:	uRawLength = ReadValue<uint16>();	break;
	case e32Bit:	uRawLength = ReadValue<uint32>();	break;
	}
	return ReadString(Set, uRawLength);
}

FW::StringW CBuffer::ReadString(EStrSet Set, size_t uRawLength) const
{
	if(uRawLength == -1)
		uRawLength = GetSizeLeft();
	char* Data = (char*)ReadData(uRawLength) ;
	FW::StringA RawString(Allocator(), Data, uRawLength);

	if(RawString.CompareAt(0, "\xEF\xBB\xBF", 3) == 0)
	{
		Set = eUtf8;
		RawString.Remove(0,3);
	}
	else if(Set == eUtf8_BOM)
		Set = eAscii;

	FW::StringW String(Allocator());
	if(Set == eAscii)
		String.Assign(RawString.ConstData(), RawString.Length());
	else
		String = FromUtf8(Allocator(), RawString.ConstData(), RawString.Length());

	return String;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utf8 conversion

FW::StringW FromUtf8(FW::AbstractMemPool * pMem, const char* pStr, size_t uLen)
{
	FW::StringW StrW(pMem);

	if (uLen == FW::StringA::NPos)
		uLen = FW::StringA::StrLen(pStr);
	if (uLen == 0)
		return StrW;

	if (!StrW.Reserve(uLen))
		return StrW;

	wchar_t* ptr16 = StrW.Data();

	uint32 u = 0;
	int bytes = 0;
	wchar_t err = L'?';

	for (size_t i = 0; i < uLen; i++)
	{
		unsigned char c = pStr[i];

		if (c <= 0x7F)
		{
			if (bytes)
			{
				*ptr16++ = err;
				bytes = 0;
			}
			*ptr16++ = (wchar_t)c;
		}
		else if (c <= 0xBF)
		{
			if (bytes)
			{
				u = (u << 6) | (c & 0x3F);
				bytes--;
				if (bytes == 0)
				{
					if (u <= 0xFFFF)
					{
						*ptr16++ = (wchar_t)u;
					}
					else if (u <= 0x10FFFF)
					{
						// Convert to UTF-16 surrogate pair
						u -= 0x10000;
						*ptr16++ = (wchar_t)(0xD800 + (u >> 10));
						*ptr16++ = (wchar_t)(0xDC00 + (u & 0x3FF));
					}
					else
					{
						*ptr16++ = err;
					}
				}
			}
			else
			{
				*ptr16++ = err;
			}
		}
		else if (c <= 0xDF)
		{
			bytes = 1;
			u = c & 0x1F;
		}
		else if (c <= 0xEF)
		{
			bytes = 2;
			u = c & 0x0F;
		}
		else if (c <= 0xF7)
		{
			bytes = 3;
			u = c & 0x07;
		}
		else
		{
			*ptr16++ = err;
			bytes = 0;
		}
	}

	if (bytes)
		*ptr16++ = err;

	*ptr16 = 0;
	StrW.SetSize(ptr16 - StrW.Data());
	return StrW;
}

FW::StringA ToUtf8(FW::AbstractMemPool* pMem, const wchar_t* pStr, size_t uLen)
{
	FW::StringA StrA(pMem);

	if (uLen == FW::StringW::NPos)
		uLen = FW::StringW::StrLen(pStr);
	if (uLen == 0)
		return StrA;

	// Worst-case: each wchar_t might turn into 3 UTF-8 bytes, or 4 bytes for surrogate pairs
	if(!StrA.Reserve(uLen * 4))
		return StrA;

	char* ptr8 = StrA.Data();

	uint32 u = 0;
	char err = '?';

	for (size_t i = 0; i < uLen; ++i)
	{
		const wchar_t wc = pStr[i];

		// Handle surrogate pairs (UTF-16)
		if (wc >= 0xD800 && wc <= 0xDBFF) // high surrogate
		{
			if (i + 1 < uLen)
			{
				wchar_t wc2 = pStr[i + 1];
				if (wc2 >= 0xDC00 && wc2 <= 0xDFFF) // low surrogate
				{
					u = 0x10000 + (((wc - 0xD800) << 10) | (wc2 - 0xDC00));
					++i; // consume both
				}
				else
				{
					// Invalid surrogate pair
					u = err;
				}
			}
			else
			{
				// Truncated surrogate pair
				u = err;
			}
		}
		else if (wc >= 0xDC00 && wc <= 0xDFFF)
		{
			// Unexpected low surrogate
			u = err;
		}
		else
		{
			u = wc;
		}

		// Encode as UTF-8
		if (u <= 0x7F)
			*ptr8++ = (char)u;
		else if (u <= 0x7FF)
		{
			*ptr8++ = (char)(0xC0 | (u >> 6));
			*ptr8++ = (char)(0x80 | (u & 0x3F));
		}
		else if (u <= 0xFFFF)
		{
			*ptr8++ = (char)(0xE0 | (u >> 12));
			*ptr8++ = (char)(0x80 | ((u >> 6) & 0x3F));
			*ptr8++ = (char)(0x80 | (u & 0x3F));
		}
		else if (u <= 0x10FFFF)
		{
			*ptr8++ = (char)(0xF0 | (u >> 18));
			*ptr8++ = (char)(0x80 | ((u >> 12) & 0x3F));
			*ptr8++ = (char)(0x80 | ((u >> 6) & 0x3F));
			*ptr8++ = (char)(0x80 | (u & 0x3F));
		}
		else
		{
			*ptr8++ = err;
		}
	}

	*ptr8 = 0;
	StrA.SetSize(ptr8 - StrA.Data());
	return StrA;
}

FW::StringA ToAscii(FW::AbstractMemPool* pMem, const wchar_t* pStr, size_t uLen)
{
	FW::StringA StrA(pMem);

	if (uLen == FW::StringW::NPos)
		uLen = FW::StringW::StrLen(pStr);
	if (uLen == 0)
		return StrA;

	if(!StrA.Reserve(uLen))
		return StrA;

	char* ptr8 = StrA.Data();

	for (size_t i = 0; i < uLen; ++i)
	{
		wchar_t wc = pStr[i];

		if (wc < 0x80)
			*ptr8++ = (char)wc;
		else
			*ptr8++ = '?'; // Replace non-ASCII characters with '?'
	}

	*ptr8 = 0;
	StrA.SetSize(ptr8 - StrA.Data());
	return StrA;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hexadecimal conversion

void ToHex(FW::StringW& dest, const byte* Data, size_t uSize)
{
	dest.Clear();
	for (size_t i = 0; i < uSize; i++)
	{
		wchar_t buf[3];

		buf[0] = (Data[i] >> 4) & 0xf;
		if (buf[0] < 10)
			buf[0] += '0';
		else
			buf[0] += 'A' - 10;

		buf[1] = (Data[i]) & 0xf;
		if (buf[1] < 10)
			buf[1] += '0';
		else
			buf[1] += 'A' - 10;

		buf[2] = 0;

		dest.Append(buf);
	}
}

CBuffer FromHex(FW::StringW str)
{
	size_t length = str.Length();
	if(length%2 == 1)
	{
		str.Insert(0, L"0");
		length++;
	}

	CBuffer Buffer(str.Allocator(), length/2, true);
	byte* buffer = Buffer.GetBuffer();
	for(size_t i=0; i < length/2; i++)
	{
		wchar_t ch1 = str[i*2];
		int dig1 = 0;
		if(isdigit(ch1)) 
			dig1 = ch1 - '0';
		else if(ch1>='A' && ch1<='F') 
			dig1 = ch1 - 'A' + 10;
		else if(ch1>='a' && ch1<='f') 
			dig1 = ch1 - 'a' + 10;

		wchar_t ch2 = str[i*2 + 1];
		int dig2 = 0;
		if(isdigit(ch2)) 
			dig2 = ch2 - '0';
		else if(ch2>='A' && ch2<='F') 
			dig2 = ch2 - 'A' + 10;
		else if(ch2>='a' && ch2<='f') 
			dig2 = ch2 - 'a' + 10;

		buffer[i] = (byte)(dig1*16 + dig2);
	}
	return Buffer;
}
