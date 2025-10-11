#pragma once

#include "../Core/Types.h"
#include "../Core/Memory.h"
#include "../Core/String.h"

FW_NAMESPACE_BEGIN
class CVariant;
FW_NAMESPACE_END

class FRAMEWORK_EXPORT CBuffer: public FW::AbstractContainer
{
public:
	struct SBuffer
	{
		volatile LONG Refs;
		//uint32 uUnused;
		size_t uCapacity;
		// 16
#pragma warning( push )
#pragma warning( disable : 4200)
		byte Data[0];
#pragma warning( pop )
	};
	//const int SBuffer_Size = sizeof(SBuffer); 

	CBuffer(FW::AbstractMemPool* pMemPool = nullptr);
#ifndef KERNEL_MODE
	CBuffer(size_t uLength, bool bUsed = false) : CBuffer((FW::AbstractMemPool*)nullptr, uLength, bUsed) {}
	CBuffer(void* pBuffer, const size_t uSize, bool bDerived = false) : CBuffer((FW::AbstractMemPool*)nullptr, pBuffer, uSize, bDerived) {}
	CBuffer(const void* pBuffer, const size_t uSize, bool bDerived = false) : CBuffer((FW::AbstractMemPool*)nullptr, pBuffer, uSize, bDerived) { SetReadOnly(); }
#endif
	CBuffer(FW::AbstractMemPool* pMemPool, size_t uLength, bool bUsed = false) : CBuffer(pMemPool)								{ AllocBuffer(uLength, bUsed); }
	CBuffer(FW::AbstractMemPool * pMemPool, void* pBuffer, const size_t uSize, bool bDerived = false) : CBuffer(pMemPool)		{ SetBuffer(pBuffer, uSize, bDerived); }
	CBuffer(FW::AbstractMemPool * pMemPool, const void* pBuffer, const size_t uSize, bool bDerived = false) : CBuffer(pMemPool) { SetBuffer((void*)pBuffer, uSize, bDerived); SetReadOnly(); }
	CBuffer(const CBuffer& Other);
	CBuffer(CBuffer&& Other);
	~CBuffer();

	CBuffer& operator=(const CBuffer& Other)			{Assign(Other); return *this;}

	bool operator== (const CBuffer& Other) const		{ return Compare(Other) == 0; }
	bool operator!= (const CBuffer& Other) const		{ return Compare(Other) != 0; }
	bool operator>= (const CBuffer& Other) const		{ return Compare(Other) >= 0; }
	bool operator<= (const CBuffer& Other) const		{ return Compare(Other) <= 0; }
	bool operator> (const CBuffer& Other) const			{ return Compare(Other) > 0; }
	bool operator< (const CBuffer& Other) const			{ return Compare(Other) < 0; }

	void	Clear();
	bool	Assign(const CBuffer& Other);


#ifdef USING_QT
	CBuffer(const QByteArray& Buffer, bool bDerive = false) : CBuffer((FW::AbstractMemPool*)nullptr) {
		if(bDerive) SetBuffer((byte*)Buffer.data(), Buffer.size(), true); 
		else CopyBuffer((byte*)Buffer.data(), Buffer.size()); 
	}
	QByteArray	ToByteArray() const						{return QByteArray((char*)GetBuffer(), (int)GetSize());}
#endif

	bool	AllocBuffer(size_t uSize, bool bUsed = false, bool bFixed = false, bool bDetachable = true);

	bool	SetBuffer(void* pBuffer, size_t uSize, bool bDerived = false, bool bFixed = false);
	bool	CopyBuffer(const void* pBuffer, size_t uSize);
	byte*	GetBuffer(bool bDetatch = false);
	const byte*	GetBuffer() const						{return IsContainerized() ? m_p.Buffer->Data : m_p.Data;}

	size_t	GetSize() const								{return m_uSize;}
	bool	SetSize(size_t uSize, bool bExpend = false, size_t uPreAlloc = 0);

	bool	IsValid() const								{return m_p.Void != nullptr;}
	size_t	GetCapacity() const							{return IsContainerized() ? (m_p.Buffer ? m_p.Buffer->uCapacity : 0) : (m_uSize + m_uPreAllocated);}

	int		Compare(const CBuffer& Buffer) const;

#ifdef HAS_ZLIB
	bool	Pack();
	bool	Unpack();
#endif

	///////////////////////////////
	// Read Write

	void	SetReadOnly(bool bReadOnly = true)			{m_bReadOnly = bReadOnly;}
	bool	IsReadOnly() const							{return m_bReadOnly;}
	bool	IsDerived()	const							{return m_bDerived;}
	bool	IsDetachable()	const						{return m_bDetachable;}

	size_t	GetPosition() const							{return m_uPosition;}
	bool	SetPosition(size_t uPosition) const;
	const byte*	ReadData(size_t uLength = -1) const;
#ifdef USING_QT
	QByteArray ReadQData(size_t uLength = -1) const		{return QByteArray((char*)ReadData(uLength), (int)(uLength == -1 ? GetSizeLeft() : uLength));}
#endif
	byte* 	WriteData(const void* pData, size_t uLength);
#ifdef USING_QT
	void	WriteQData(const QByteArray& Buffer, int Size = 0){if(Size == 0) Size = Buffer.size(); ASSERT(Size <= Buffer.size()); WriteData(Buffer.data(),Size);}
#endif
	size_t	GetSizeLeft() const							{ASSERT(m_uSize >= m_uPosition); return m_uSize - m_uPosition;}
	size_t	GetCapacityLeft() const						{ASSERT(GetCapacity() >= m_uSize); return GetCapacity() - m_uSize;}

	const byte*	GetDataAt(size_t uOffset, size_t uLength) const;
	byte*	SetDataAt(size_t uOffset, void* pData, size_t uLength);
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
	T				ReadValue(bool* pOk = nullptr, bool fromBigEndian = false) const
	{
		if(fromBigEndian)
		{
			T Data;
			for (size_t i = sizeof(T); i > 0; i--)
			{
				const byte* pByte = ReadData(1);
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
			const byte* pData = ReadData(sizeof(T)); 
			if (!pData) {
				if (pOk) *pOk = false;
				return T();
			}
			if (pOk) *pOk = true;
			return *((T*)pData);
		}
	}

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

	bool			WriteString(const FW::StringW& String, EStrSet Set = eUtf8, EStrLen Len = e16Bit);
	bool			WriteString(const FW::StringA& String, EStrLen Len = e16Bit);
	FW::StringW		ReadString(EStrSet Set = eUtf8, EStrLen Len = e16Bit) const;
	FW::StringW		ReadString(EStrSet Set, size_t uRawLength) const;

protected:
	friend FW::CVariant;

	bool			PrepareWrite(size_t uOffset, size_t uLength);

	bool			ReAllocBuffer(size_t uCapacity);

	SBuffer*		MakeData(size_t uCapacity);
	void			AttachData(SBuffer* pBuffer);
	void			DetachData();

	bool			IsContainerized() const {return !m_bDerived && !m_bDetachable && m_p.Buffer;}

	union {
		uint32		m_uFlags;
		struct {
			uint32		
				m_bDerived		: 1,	// Derived from External Buffer
				m_bDetachable	: 1,	// Detachable Buffer
				m_bFixed		: 1,	// No Auto Resize
				m_bReadOnly		: 1,	// No Write
				m_uReserved		: 28;
		};
	};
	uint32			m_uPreAllocated; // used only when m_bDetachable is true
	union {
		byte*		Data;
		SBuffer*	Buffer;
		void*		Void;
	}				m_p;
	size_t			m_uSize;
	mutable size_t	m_uPosition; // read/write position
};

FRAMEWORK_EXPORT FW::StringW FromUtf8(FW::AbstractMemPool * pMem, const char* pStr, size_t uLen = FW::StringA::NPos);
FRAMEWORK_EXPORT FW::StringA ToUtf8(FW::AbstractMemPool* pMem, const wchar_t* pStr, size_t uLen = FW::StringW::NPos);
FRAMEWORK_EXPORT FW::StringA ToAscii(FW::AbstractMemPool* pMem, const wchar_t* pStr, size_t uLen = FW::StringW::NPos);

FRAMEWORK_EXPORT void ToHex(FW::StringW& dest, const byte* Data, size_t uSize);
FRAMEWORK_EXPORT CBuffer FromHex(FW::StringW Hex);