#pragma once
#include "../lib_global.h"

#include "../Types.h"

#ifndef FRAMEWORK_EXPORT
#define FRAMEWORK_EXPORT LIBRARY_EXPORT
#endif

#include "../../Framework/Memory.h"

#ifndef BUFF_NO_STL_STR
#include <string>
#endif

class LIBRARY_EXPORT CBuffer: public FW::AbstractContainer
{
public:
#ifdef KERNEL_MODE
	CBuffer(FW::AbstractMemPool* pMemPool);
#else
	CBuffer(FW::AbstractMemPool* pMemPool = nullptr);
	CBuffer(size_t uLength, bool bUsed = false) : CBuffer((FW::AbstractMemPool*)nullptr, uLength, bUsed) {}
	CBuffer(void* pBuffer, const size_t uSize, bool bDerived = false) : CBuffer((FW::AbstractMemPool*)nullptr, pBuffer, uSize, bDerived) {}
	CBuffer(const void* pBuffer, const size_t uSize, bool bDerived = false) : CBuffer((FW::AbstractMemPool*)nullptr, pBuffer, uSize, bDerived) {}
#endif
	CBuffer(FW::AbstractMemPool* pMemPool, size_t uLength, bool bUsed = false);
	CBuffer(FW::AbstractMemPool* pMemPool, void* pBuffer, const size_t uSize, bool bDerived = false);
	CBuffer(FW::AbstractMemPool* pMemPool, const void* pBuffer, const size_t uSize, bool bDerived = false);
	CBuffer(const CBuffer& Buffer);
	~CBuffer();

	CBuffer& operator=(const CBuffer &Other)			
	{
		Clear();
		if(!m_pMem && Other.m_pMem)
			m_pMem = Other.m_pMem;
		CopyBuffer(Other.GetBuffer(), Other.GetSize()); 
		return *this;
	}

	void	Clear();

#ifdef USING_QT
	CBuffer(const QByteArray& Buffer, bool bDerive = false)		
	{
		Init(); 
		if(bDerive) 
			SetBuffer((byte*)Buffer.data(), Buffer.size(), true); 
		else
			CopyBuffer((byte*)Buffer.data(), Buffer.size()); 
	}
	QByteArray	ToByteArray() const						{return QByteArray((char*)GetBuffer(), (int)GetSize());}
#endif

	void	AllocBuffer(size_t uSize, bool bUsed = false, bool bFixed = true);

	bool	SetBuffer(void* pBuffer, size_t uSize, bool bDerived = false);
	void	CopyBuffer(const void* pBuffer, size_t uSize);
	byte*	GetBuffer(bool bDetatch = false);
	const byte*	GetBuffer() const						{return m_pBuffer;}

	size_t	GetSize() const								{return m_uSize;}
	bool	SetSize(size_t uSize, bool bExpend = false, size_t uPreAlloc = 0);

	bool	IsValid() const								{return m_pBuffer != NULL;}
	size_t	GetCapacity() const							{return m_uCapacity;}

	int		Compare(const CBuffer& Buffer) const;


#ifdef HAS_ZLIB
	bool	Pack();
	bool	Unpack();
#endif

	///////////////////////////////
	// Read Write

	void	SetReadOnly(bool bReadOnly = true)			{m_bReadOnly = bReadOnly;}
	bool	IsReadOnly() const							{return m_bReadOnly;}
	bool	IsDerived()	const							{return m_eType == eDerived;}

	size_t	GetPosition() const							{return m_uPosition;}
	bool	SetPosition(size_t uPosition) const;
	byte*	GetData(size_t uLength = -1) const;
	byte*	ReadData(size_t uLength = -1) const;
#ifdef USING_QT
	QByteArray ReadQData(size_t uLength = -1) const		{return QByteArray((char*)ReadData(uLength), (int)(uLength == -1 ? GetSizeLeft() : uLength));}
#endif
	byte*	SetData(const void* pData, size_t uLength);
	bool	WriteData(const void* pData, size_t uLength);
#ifdef USING_QT
	void	WriteQData(const QByteArray& Buffer, int Size = 0){if(Size == 0) Size = Buffer.size(); ASSERT(Size <= Buffer.size()); WriteData(Buffer.data(),Size);}
#endif
	size_t	GetSizeLeft() const							{ASSERT(m_uSize >= m_uPosition); return m_uSize - m_uPosition;}
	size_t	GetCapacityLeft() const						{ASSERT(m_uCapacity >= m_uSize); return m_uCapacity - m_uSize;}

	byte*	GetData(size_t uOffset, size_t uLength) const;
	byte*	SetData(size_t uOffset, void* pData, size_t uLength);
	byte*	InsertData(size_t uOffset, void* pData, size_t uLength);
	byte*	ReplaceData(size_t uOffset, size_t uOldLength, void* pData, size_t uNewLength);
	bool	RemoveData(size_t uOffset, size_t uLength);
	bool	AppendData(const void* pData, size_t uLength);
	bool	ShiftData(size_t uOffset);

	template <class T>
	bool			WriteValue(T Data, bool toBigEndian = false)
	{
		if(toBigEndian)
		{
			for (size_t i = sizeof(T); i > 0; i--) {
				if (!WriteData(((byte*)&Data) + i - 1, 1))
					return false;
			}
			return true;
		}
		else
			return WriteData(&Data, sizeof(T));
	}
	template <class T>
	T				ReadValue(bool* pOk = NULL, bool fromBigEndian = false) const
	{
		if(fromBigEndian)
		{
			T Data;
			for (size_t i = sizeof(T); i > 0; i--)
			{
				byte* pByte = ReadData(1);
				if (!pByte) {
					if (pOk) *pOk = false;
					return T();
				}
				*(((byte*)&Data) + i - 1) = *pByte;
			}
			if (pOk) *pOk = true;
			return Data;
		}
		else
		{
			byte* pData = ReadData(sizeof(T)); 
			if (!pData) {
				if (pOk) *pOk = false;
				return T();
			}
			if (pOk) *pOk = true;
			return *((T*)pData);
		}
	}

#ifndef BUFF_NO_STL_STR
	enum EStrSet
	{
		eAscii,
		eUtf8_BOM,
		eUtf8
	};
	enum EStrLen
	{
		e8Bit,
		e16Bit,
		e32Bit,
	};

	bool			WriteString(const std::wstring& String, EStrSet Set = eUtf8, EStrLen Len = e16Bit);
	bool			WriteString(const std::string& String, EStrLen Len = e16Bit);
	std::wstring	ReadString(EStrSet Set = eUtf8, EStrLen Len = e16Bit) const;
	std::wstring	ReadString(EStrSet Set, size_t uRawLength) const;
#endif

protected:
	void			Init();
	bool			PrepareWrite(size_t uOffset, size_t uLength);

	void*			Alloc(size_t size);
	void			Free(void* ptr);

	byte*			m_pBuffer;
	size_t			m_uSize; // size of data
	size_t			m_uCapacity; // Length of the allocated memory

	mutable size_t	m_uPosition; // read/write position

	enum EType
	{
		eNormal,
		eFixed,
		eDerived,
	}				m_eType;
	bool			m_bReadOnly;
};



#ifndef BUFF_NO_STL_STR
LIBRARY_EXPORT void WStrToAscii(std::string& dest, const std::wstring& src);
LIBRARY_EXPORT void WStrToUtf8(std::string& dest, const std::wstring& src);
LIBRARY_EXPORT char* WCharToUtf8(const wchar_t* str, size_t len, size_t* out_len);
LIBRARY_EXPORT void AsciiToWStr(std::wstring& dest, const std::string& src);
LIBRARY_EXPORT void Utf8ToWStr(std::wstring& dest, const std::string& src);

LIBRARY_EXPORT std::wstring ToHex(const byte* Data, size_t uSize);
LIBRARY_EXPORT CBuffer FromHex(std::wstring Hex);
#endif