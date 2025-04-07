//#include "pch.h"

// machine types
#include "../Core/Types.h"
#include "../Core/Header.h"

#include "Variant.h"
#include "../Core/Memory.h"

#ifndef NO_CRT
#include <memory>
#endif

#ifndef VAR_NO_EXCEPTIONS
#ifdef KERNEL_MODE
#error Exceptions are not supported in kernel mode
#endif 
#include "Exception.h"
#endif

#include "../Core/UniquePtr.h"

FW_NAMESPACE_BEGIN

const size_t VAR_SIZE = sizeof(CVariant);
static_assert(VAR_SIZE == 24, "CVariant size mismatch");
//#define VAR_EMBEDDED_SIZE sizeof(CVariant) - FIELD_OFFSET(CVariant, m_Embedded)
const size_t VAR_EMBEDDED_SIZE = sizeof(CVariant) - FIELD_OFFSET(CVariant, m_Embedded);

CVariant::CVariant(FW::AbstractMemPool* pMemPool, EType Type)
#ifndef KERNEL_MODE
	: FW::AbstractContainer(pMemPool ? pMemPool : (FW::AbstractMemPool*)nullptr)
#else
	: FW::AbstractContainer(pMemPool)
#endif
{
	InitValue(Type, 0, nullptr);
}

CVariant::CVariant(const CVariant& Variant)
	: FW::AbstractContainer(Variant)
{
	EResult Err = InitAssign(Variant);
	if (Err) Throw(Err);
}

CVariant::CVariant(CVariant&& Variant)
	: FW::AbstractContainer(Variant)
{
	InitMove(Variant);
}

CVariant::~CVariant()
{
	DetachPayload();
}

CVariant::EResult CVariant::Throw(EResult Error)
{
	//
	// eErrNotFound is fine we dont waht to check presence each time its fine to 
	// check if we got a valid variant, and with the ref mechanism we can return empty refs without any problem
	//

	if (Error != eErrNotFound)
	{
#ifdef KERNEL_DEBUG
		DbgPrintEx(DPFLTR_DEFAULT_ID, 0xFFFFFFFF, "CVariant::Throw: %s\n", ErrorString(Error));
#elif defined(_DEBUG)
		//DbgPrint("CVariant::Throw: %s\n", ErrorString(Error));
		DebugBreak();
#endif
#ifndef VAR_NO_EXCEPTIONS
		throw CException(ErrorString(Error));
#endif
	}
	return Error;
}

const char* CVariant::ErrorString(EResult Error)
{
	switch(Error) {
	case eErrNone: return "No Error";
	case eErrAllocFailed: return "Memory Allocation Failed";
	case eErrBufferShort: return "Buffer Too Short";
	case eErrBufferWriteFailed: return "Buffer Write Failed";
	case eErrInvalidHeader: return "Invalid Header";
	case eErrIntegerOverflow: return "Integer Overflow";
	case eErrNotWritable: return "Variant Not Writable";
	case eErrTypeMismatch: return "Type Mismatch";
	case eErrIsEmpty: return "Variant Is Empty";
	case eErrWriteInProgrees: return "Write In Progress";
	case eErrWriteNotReady: return "Write Not Ready";
	case eErrInvalidName: return "Invalid Name";
	case eErrNotFound: return "Not Found";
	case eErrIndexOutOfBounds: return "Index Out Of Bounds";
	default: return "Unknown Error";
	}
}

CVariant::EResult CVariant::InitValue(EType Type, size_t Size, const void* Value, bool bTake)
{
	m_Type = Type;
	m_uFlags = 0;

	if (bTake)
	{
		m_bRawPayload = true;
		m_uPayloadSize = (uint32)Size;
		m_p.RawPayload = (byte*)Value;
		return eErrNone;
	}

	if (m_Type == VAR_TYPE_EMPTY || !Size)
	{
		MemSet(m_Embedded, 0, VAR_EMBEDDED_SIZE);
		return eErrNone;
	}

	void* pPtr = nullptr;
	if (Size <= VAR_DATA_SIZE) // small enough to fit data
	{
		m_uPayloadSize = (uint32)Size;
		pPtr = m_p.Data;
	}
	else if (Size <= VAR_EMBEDDED_SIZE) // small enough to fit the variant body
	{
		m_uEmbeddedSize = (uint8)Size;
		pPtr = m_Embedded;
	}
	else
	{
		m_uPayloadSize = (uint32)Size;
		m_bRawPayload = true;
		m_p.RawPayload = (byte*)MemAlloc(m_uPayloadSize);
		if (!m_p.RawPayload)
			return Throw(eErrAllocFailed);
		pPtr = m_p.RawPayload;
	}

	if(Value) 
		MemCopy(pPtr, Value, Size);
	else
		MemZero(pPtr, Size);
	return eErrNone;
}

CVariant::EResult CVariant::InitAssign(const CVariant& From)
{
	if (!m_pMem && From.m_pMem)
		m_pMem = From.m_pMem;

	if (!From.IsValid())
		return InitValue(VAR_TYPE_EMPTY, 0, nullptr);

	//
	// 1.) If the memory pool is different we must copy the variant
	//
	// 2.) We can not asign a derived buffer, as the original variant may be removed at any time
	//

	if (m_pMem != From.m_pMem || From.m_bDerived)
		return Clone(&From, this);
	
	if (!From.m_bContainer)
		return InitValue(From.m_Type, From.GetSize(), From.GetData());

	ASSERT(From.m_uEmbeddedSize == 0);

	m_Type = From.m_Type;
	m_uFlags = 0;
	m_bContainer = true;
	m_uPayloadSize = From.m_uPayloadSize;
	switch (From.m_Type)
	{
	case VAR_TYPE_MAP:
	case VAR_TYPE_LIST:
	case VAR_TYPE_INDEX:
		m_p.Container = From.m_p.Container;
		InterlockedIncrement(&m_p.Container->Refs);
		break;

	case VAR_TYPE_BYTES:
		m_p.Buffer = From.m_p.Buffer;
		InterlockedIncrement(&m_p.Buffer->Refs);
		break;
	case VAR_TYPE_ASCII:
	case VAR_TYPE_UTF8:
		m_p.StrA = From.m_p.StrA;
		InterlockedIncrement(&m_p.StrA->Refs);
		break;
	case VAR_TYPE_UNICODE:
		m_p.StrW = From.m_p.StrW;
		InterlockedIncrement(&m_p.StrW->Refs);
		break;
	}
	return eErrNone;
}

void CVariant::InitMove(CVariant& From)
{
	m_Type = From.m_Type;
	m_uFlags = From.m_uFlags;
	if (m_uEmbeddedSize)
	{
		m_uEmbeddedSize = From.m_uEmbeddedSize;
		MemCopy(m_Embedded, From.m_Embedded, m_uEmbeddedSize);
	}
	else
	{
		m_uPayloadSize = From.m_uPayloadSize;
		MemCopy(m_p.Data, From.m_p.Data, VAR_DATA_SIZE); // copy container pointer or data
	}
	From.m_uFlags = 0;
	From.m_Type = VAR_TYPE_EMPTY;
}

void CVariant::InitFromContainer(SContainer* pContainer, EType Type)
{
	ASSERT(m_bContainer == false); // must alrady be detached
	ASSERT(pContainer->Refs == 0); // must not yet be referenced

	m_Type = Type;
	if (m_uEmbeddedSize)
		m_uEmbeddedSize = 0;
	else if (m_bRawPayload) {
		pContainer->pRawPayload = m_p.RawPayload;
		m_bRawPayload = false;
	}
	m_p.Container = pContainer;
	m_p.Container->Refs = 1;
	m_bContainer = true;
}

CVariant::EResult CVariant::InitFromBuffer(CBuffer::SBuffer* pBuffer, size_t uSize)
{
	m_Type = VAR_TYPE_BYTES;
	m_uFlags = 0;
	m_p.Buffer = pBuffer;
	InterlockedIncrement(&m_p.Buffer->Refs);
	m_uPayloadSize = (uint32)uSize;
	m_bContainer = true;
	return eErrNone;
}

CVariant::EResult CVariant::InitFromStringA(FW::StringA::SStringData* pStrA, bool bUtf8)
{
	m_Type = bUtf8 ? VAR_TYPE_UTF8 : VAR_TYPE_ASCII;
	m_uFlags = 0;
	m_p.StrA = pStrA;
	InterlockedIncrement(&m_p.StrA->Refs);
	m_bContainer = true;
	return eErrNone;
}

CVariant::EResult CVariant::InitFromStringW(FW::StringW::SStringData* pStrW)
{
	m_Type = VAR_TYPE_UNICODE;
	m_uFlags = 0;
	m_p.StrW = pStrW;
	InterlockedIncrement(&m_p.StrW->Refs);
	m_bContainer = true;
	return eErrNone;
}

void CVariant::Clear()
{
	DetachPayload();
	InitValue(VAR_TYPE_EMPTY, 0, nullptr);
}

void CVariant::DetachPayload()
{
	if(m_uEmbeddedSize)
		m_uEmbeddedSize = 0;
	else if (m_bRawPayload)
	{
		m_uPayloadSize = 0;
		m_bRawPayload = false;
		if (m_bDerived)
			m_bDerived = false;
		else if (m_p.RawPayload) {
			MemFree(m_p.RawPayload);
			m_p.RawPayload = nullptr;
		}
	}
	else if(m_bContainer)
	{
		m_uPayloadSize = 0;
		m_bContainer = false;
		switch (m_Type)
		{
		case VAR_TYPE_MAP:
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:
			if (InterlockedDecrement(&m_p.Container->Refs) == 0)
			{
				if (m_bDerived)
					m_bDerived = false;
				else if (m_p.Container->pRawPayload)
					MemFree(m_p.Container->pRawPayload);

				switch (m_Type) {
				case VAR_TYPE_MAP:		FreeMap(m_p.Map); break;
				case VAR_TYPE_LIST:		FreeList(m_p.List); break;
				case VAR_TYPE_INDEX:	FreeIndex(m_p.Index); break;
				}
			}
			break;
		case VAR_TYPE_BYTES:	
			if (InterlockedDecrement(&m_p.Buffer->Refs) == 0)
				MemFree(m_p.Buffer); 
			break;
		case VAR_TYPE_ASCII:
		case VAR_TYPE_UTF8:
			if (InterlockedDecrement(&m_p.StrA->Refs) == 0)
				MemFree(m_p.StrA);
			break;
		case VAR_TYPE_UNICODE:
			if (InterlockedDecrement(&m_p.StrW->Refs) == 0)
				MemFree(m_p.StrW);
			break;
		}
		m_p.Void = nullptr;
	}
}

CVariant CVariant::Clone(bool Full) const
{
	CVariant NewVariant(m_pMem);
	Clone(this, &NewVariant, Full);
	return NewVariant;
}

const byte* CVariant::GetData() const
{ 
	if (m_uEmbeddedSize)
		return (byte*)m_Embedded;
	else if (m_bRawPayload)
		return m_p.RawPayload;
	else if (m_bContainer)
	{
		switch (m_Type)
		{
		case VAR_TYPE_MAP:
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:	return m_p.Container->pRawPayload;
		case VAR_TYPE_BYTES:	return m_p.Buffer->Data;
		case VAR_TYPE_ASCII:
		case VAR_TYPE_UTF8:		return (byte*)m_p.StrA->Data;
		case VAR_TYPE_UNICODE:	return (byte*)m_p.StrW->Data;
		default:				return nullptr;
		}
	}
	else
		return m_p.Data;
}

uint32 CVariant::GetSize() const
{ 
	if(m_uEmbeddedSize)
		return m_uEmbeddedSize;
	if (m_bContainer)
	{
		switch (m_Type)
		{
		case VAR_TYPE_ASCII:
		case VAR_TYPE_UTF8:		return (uint32)m_p.StrA->Length;
		case VAR_TYPE_UNICODE:	return (uint32)(m_p.StrW->Length * sizeof(wchar_t));
		}
	}
	return m_uPayloadSize; 
}

CVariant::EResult CVariant::Freeze()
{
	if (m_bReadOnly)
		return Throw(eErrNotWritable);

	CBuffer Packet(m_pMem);
	EResult Err = ToPacket(&Packet);
	if(Err) return Throw(Err);
	Packet.SetPosition(0);
	return FromPacket(&Packet);
}

CVariant::EResult CVariant::Unfreeze()
{
	if (m_bDerived)
		return Throw(eErrNotWritable);

	m_bReadOnly = false;
	return eErrNone;
}

CVariant::EResult CVariant::FromPacket(const CBuffer* pPacket, bool bDerived)
{
	Clear();

	size_t Size = ReadHeader(pPacket, &m_Type);
	if (Size == 0xFFFFFFFF) return Throw(eErrInvalidHeader);
	
	const byte* pData = pPacket->ReadData(Size);
	if (!pData) return Throw(eErrBufferShort);

	m_bReadOnly = true;

	if (Size <= VAR_DATA_SIZE) // small enough to fit data
	{
		m_uPayloadSize = (uint32)Size;
		MemCopy(m_p.Data, pData, Size);
	}
	else if (Size <= VAR_EMBEDDED_SIZE) // small enough to fit the variant body
	{
		m_uEmbeddedSize = (uint8)Size;
		MemCopy(m_Embedded, pData, Size);
	}
	else if(bDerived)
	{
		m_bRawPayload = true;
		m_bDerived = true;
		m_uPayloadSize = (uint32)Size;
		m_p.RawPayload = (byte*)pData;
	}
	else
	{
		m_bRawPayload = true;
		m_uPayloadSize = (uint32)Size;
		m_p.RawPayload = (byte*)MemAlloc(Size);
		if (!m_p.RawPayload) 
			return Throw(eErrAllocFailed);
		MemCopy(m_p.RawPayload, pData, Size);
	}
	return eErrNone;
}

uint32 CVariant::ReadHeader(const CBuffer* pPacket, EType* pType)
{
	bool bOk;
	byte Type = pPacket->ReadValue<uint8>(&bOk);
	if (!bOk) 
		return 0xFFFFFFFF;
	ASSERT(Type != 0xFF);

	uint32 Size;
	if (Type & VAR_LEN_FIELD)
	{
		if ((Type & VAR_LEN_MASK) == VAR_LEN8)
			Size = pPacket->ReadValue<uint8>(&bOk);
		else if ((Type & VAR_LEN_MASK) == VAR_LEN16)
			Size = pPacket->ReadValue<uint16>(&bOk);
		else //if ((Type & VAR_LEN_MASK) == VAR_LEN32) // VAR_LEN32 or packed in eider case teh length is 32 bit 
			Size = pPacket->ReadValue<uint32>(&bOk);
		if (!bOk) 
			return 0xFFFFFFFF;
	}
	else if ((Type & VAR_TYPE_MASK) == VAR_TYPE_EMPTY)
		Size = 0;
	else if ((Type & VAR_LEN_MASK) == VAR_LEN8)
		Size = 1;
	else if ((Type & VAR_LEN_MASK) == VAR_LEN16)
		Size = 2;
	else if ((Type & VAR_LEN_MASK) == VAR_LEN32)
		Size = 4;
	else //if ((Type & VAR_LEN_MASK) == VAR_LEN64)
		Size = 8;

	if (pPacket->GetSizeLeft() < Size)
		return 0xFFFFFFFF;

	if(pType) *pType = (EType)(Type & VAR_TYPE_MASK); // clear size flags

	return Size;
}

CVariant::EResult CVariant::ToPacket(CBuffer* pPacket) const
{
	CBuffer Payload(m_pMem);
	EResult Err = MkPayload(&Payload);
	if(Err) return Throw(Err);

	return ToPacket(pPacket, m_Type, Payload.GetSize(), Payload.GetBuffer());
}

CVariant::EResult CVariant::ToPacket(CBuffer* pPacket, EType Type, size_t uLength, const void* Value)
{
	ASSERT(uLength < 0xFFFFFFFF);

	ASSERT((Type & ~VAR_TYPE_MASK) == 0);

	bool bOk;

	if(Type == VAR_TYPE_EMPTY)
		bOk = pPacket->WriteValue<uint8>(Type);
	else if (uLength == 1)
		bOk = pPacket->WriteValue<uint8>(Type | VAR_LEN8);
	else if (uLength == 2)
		bOk = pPacket->WriteValue<uint8>(Type | VAR_LEN16);
	else if (uLength == 4)
		bOk = pPacket->WriteValue<uint8>(Type | VAR_LEN32);
	else if (uLength == 8)
		bOk = pPacket->WriteValue<uint8>(Type | VAR_LEN64);
	else
	{
		if (uLength > 0xFFFF) {
			bOk = pPacket->WriteValue<uint8>(Type | VAR_LEN_FIELD | VAR_LEN32);
			if(bOk) bOk = pPacket->WriteValue<uint32>((uint32)uLength);
		}
		else if (uLength > 0xFF) {
			bOk = pPacket->WriteValue<uint8>(Type | VAR_LEN_FIELD | VAR_LEN16);
			if(bOk) bOk = pPacket->WriteValue<uint16>((uint16)uLength);
		}
		else {
			bOk = pPacket->WriteValue<uint8>(Type | VAR_LEN_FIELD | VAR_LEN8);
			if(bOk) bOk = pPacket->WriteValue<uint8>((uint8)uLength);
		}
	}

	if(bOk) bOk =  pPacket->WriteData(Value, uLength);

	if(!bOk) 
		return Throw(eErrBufferWriteFailed);
	return eErrNone;
}

uint32 CVariant::Count() const // Map, List or Index
{
	switch(m_Type)
	{
	case VAR_TYPE_MAP: {
		auto Ret = GetMap(false, false);
		if (Ret.Error) return 0;
		return Ret.Value->Count;
	}
	case VAR_TYPE_LIST: {
		auto Ret = GetList(false, false);
		if (Ret.Error) return 0;
		return Ret.Value->Count;
	}
	case VAR_TYPE_INDEX: {
		auto Ret = GetIndex(false, false);
		if (Ret.Error) return 0;
		return Ret.Value->Count;
	}
	default: return 0;
	}
}

const char* CVariant::Key(uint32 Index) const // Map
{
	auto Ret = GetMap(false, false);
	if (Ret.Error) {
		Throw(Ret.Error);
		return nullptr;
	}
	
	if (Index >= Ret.Value->Count) {
		Throw(eErrIndexOutOfBounds);
		return nullptr;
	}

	auto I = MapBegin(Ret.Value);
	while(Index--)
		++I;
	return I.Key().ConstData();
}

FW::RetValue<const CVariant*, CVariant::EResult> CVariant::PtrAt(const char* Name) const // Map
{
	auto Ret = GetMap(false, false);
	if (Ret.Error) return Ret.Error;

	auto I = MapFind(Ret.Value, Name);
	if (I != MapEnd())
		return &I.Value();

	return nullptr;
}

FW::RetValue<CVariant*, CVariant::EResult> CVariant::PtrAt(const char* Name, bool bCanAdd) // Map
{
	auto Ret = GetMap(bCanAdd, true);
	if (Ret.Error) return Ret.Error;

	auto I = MapFind(Ret.Value, Name);
	if (I != MapEnd())
		return &I.Value();
	
	if (!bCanAdd) return eErrNotFound;
	if (m_bReadOnly) return eErrNotWritable;

	ASSERT(Ret.Value == m_p.Map);
	CVariant* pValue = MapInsert(m_p.Map, Name, strlen(Name), nullptr, true);
	if (!pValue) return eErrAllocFailed;
	return pValue;
}

CVariant CVariant::Get(const char* Name) const // Map
{
	auto Ret = PtrAt(Name); 
	if (!Ret.Value) 
		return CVariant(m_pMem); 
	return *Ret.Value;
}

bool CVariant::Has(const char* Name) const // Map
{
	auto Ret = GetMap(false, false);
	if (Ret.Error) return false;

	return MapFind(Ret.Value, Name) != MapEnd();
}

CVariant::EResult CVariant::Remove(const char* Name) // Map
{
	if (m_bReadOnly) return Throw(eErrNotWritable);

	auto Ret = GetMap(false, true);
	if (Ret.Error) return Throw(Ret.Error);

	MapRemove(Ret.Value, Name);
	return eErrNone;
}

CVariant::EResult CVariant::Insert(const char* Name, const CVariant& Variant) // Map
{
	if (m_bReadOnly) return Throw(eErrNotWritable);

	auto Ret = GetMap(true, true);
	if (Ret.Error) return Throw(Ret.Error);

	ASSERT(Ret.Value == m_p.Map);
	return MapInsert(m_p.Map, Name, strlen(Name), &Variant) ? eErrNone : Throw(eErrAllocFailed);
}

uint32 CVariant::Id(uint32 Index) const // Index
{
	auto Ret = GetIndex(false, false);
	if (Ret.Error) {
		Throw(Ret.Error);
		return 0xFFFFFFFF;
	}

	if (Index >= Ret.Value->Count) {
		Throw(eErrIndexOutOfBounds);
		return 0xFFFFFFFF;
	}

	auto I = IndexBegin(Ret.Value);
	while(Index--)
		++I;
	return I.Key().u;
}

FW::RetValue<const CVariant*, CVariant::EResult> CVariant::PtrAt(uint32 Index) const // List or Index
{
	if (m_Type == VAR_TYPE_LIST)
	{
		auto Ret = GetList(false, false);
		if (Ret.Error) return Ret.Error;

		if (Index >= Ret.Value->Count)
			return eErrIndexOutOfBounds;

		return &((const CVariant*)Ret.Value->Data)[Index];
	}

	auto Ret = GetIndex(false, false);
	if (Ret.Error) return Ret.Error;

	auto I = IndexFind(Ret.Value, Index);
	if (I != IndexEnd())
		return &I.Value();
	return nullptr;
}

FW::RetValue<CVariant*, CVariant::EResult> CVariant::PtrAt(uint32 Index, bool bCanAdd) // List or Index
{
	if (m_Type == VAR_TYPE_LIST)
	{
		auto Ret = GetList(false, false);
		if (Ret.Error) return Ret.Error;

		if (Index >= Ret.Value->Count)
			return eErrIndexOutOfBounds;

		return &((CVariant*)Ret.Value->Data)[Index];
	}

	auto Ret = GetIndex(bCanAdd, true);
	if (Ret.Error) return Ret.Error;

	auto I = IndexFind(Ret.Value, Index);
	if (I != IndexEnd())
		return &I.Value();

	if (!bCanAdd) return eErrNotFound;
	if (m_bReadOnly) return eErrNotWritable;

	ASSERT(Ret.Value == m_p.Index);
	CVariant* pValue = IndexInsert(m_p.Index, Index, nullptr, true);
	if (!pValue) return eErrAllocFailed;
	return pValue;
}

CVariant CVariant::Get(uint32 Index) const // List or Index
{
	auto Ret = PtrAt(Index); 
	if (!Ret.Value) 
		return CVariant(m_pMem); 
	return *Ret.Value;
}

bool CVariant::Has(uint32 Index) const // Index
{
	auto Ret = GetIndex(false, false);
	if (Ret.Error) return false;

	return IndexFind(Ret.Value, Index) != IndexEnd();
}

CVariant::EResult CVariant::Remove(uint32 Index) // List or Index
{
	if (m_bReadOnly) return Throw(eErrNotWritable);

	switch (m_Type)
	{
	case VAR_TYPE_EMPTY:
		return eErrIsEmpty;

	case VAR_TYPE_INDEX: {
		auto Ret = GetIndex(false, true);
		if (Ret.Error) return Throw(Ret.Error);

		IndexRemove(Ret.Value, Index);
		return eErrNone;
	}

	case VAR_TYPE_LIST: {
		auto Ret = GetList(false, true);
		if (Ret.Error) return Throw(Ret.Error);

		ListRemove(Ret.Value, Index);
		return eErrNone;
	}

	}

	return eErrTypeMismatch;
}

CVariant::EResult CVariant::Insert(uint32 Index, const CVariant& Variant) // Index
{
	if (m_bReadOnly) return Throw(eErrNotWritable);

	auto Ret = GetIndex(true, false);
	if (Ret.Error) return Throw(Ret.Error);

	ASSERT(Ret.Value == m_p.Index);
	return IndexInsert(m_p.Index, Index, &Variant) ? eErrNone : Throw(eErrAllocFailed);
}

CVariant::EResult CVariant::Append(const CVariant& Variant) // List
{
	if (m_bReadOnly) return Throw(eErrNotWritable);

	auto Ret = GetList(true, false);
	if (Ret.Error) return Throw(Ret.Error);

	ASSERT(Ret.Value == m_p.List);
	return ListAppend(m_p.List, &Variant) ? eErrNone : Throw(eErrAllocFailed);
}

CVariant::EResult CVariant::Merge(const CVariant& Variant)
{
	if (!IsValid()) // if this variant is void we can merge with anything
	{
		EResult Err = Assign(Variant);
		if (Err) return Throw(Err);
	}
	else if(Variant.IsList() && IsList())
	{
		auto Ret = Variant.GetList(false, false);
		if (Ret.Error) return Ret.Error == eErrIsEmpty ? eErrNone : Throw(Ret.Error);
		for (uint32 i = 0; i < Ret.Value->Count; i++) {
			EResult Err = Append(((CVariant*)Ret.Value->Data)[i]);
			if (Err) return Throw(Err);
		}
	}
	else if(Variant.IsMap() && IsMap())
	{
		auto Ret = Variant.GetMap(false, false);
		if (Ret.Error) return Ret.Error == eErrIsEmpty ? eErrNone : Throw(Ret.Error);
		for (auto I = MapBegin(Ret.Value); I != MapEnd(); ++I) {
			EResult Err = Insert(I.Key().ConstData(), I.Value());
			if (Err) return Throw(Err);
		}
	}
	else if (Variant.IsIndex() && IsIndex())
	{
		auto Ret = Variant.GetIndex(false, false);
		if (Ret.Error) return Ret.Error == eErrIsEmpty ? eErrNone : Throw(Ret.Error);
		for (auto I = IndexBegin(Ret.Value); I != IndexEnd(); ++I) {
			EResult Err = Insert(I.Key().u, I.Value());
			if (Err) return Throw(Err);
		}
	}
	else
		return Throw(eErrTypeMismatch);
	return eErrNone;
}

void CVariant::ToInt(size_t Size, void* Value) const
{
	EResult Err = GetInt(Size, Value);
	if (Err) Throw(Err);
}

CVariant::EResult CVariant::GetInt(size_t Size, void* Value) const
{
	if(m_Type == VAR_TYPE_EMPTY) {
		MemZero(Value, Size);
		return eErrNone;
	}

	if(m_Type != VAR_TYPE_SINT && m_Type != VAR_TYPE_UINT)
		return eErrTypeMismatch;

	size_t uPayloadSize = GetSize();
	const byte* pPayload = GetData();

	if (Size < uPayloadSize)
	{
		for (size_t i = Size; i < uPayloadSize; i++) {
			if (pPayload[i] != 0)
				return eErrIntegerOverflow;
		}
	}

	if (Size <= uPayloadSize)
		MemCopy(Value, pPayload, Size);
	else {
		MemCopy(Value, pPayload, uPayloadSize);
		MemZero((byte*)Value + uPayloadSize, Size - uPayloadSize);
	}
	return eErrNone;
}

#ifndef VAR_NO_FPU
void CVariant::ToDouble(size_t Size, void* Value) const
{
	EResult Err = GetDouble(Size, Value);
	if (Err) Throw(Err);
}

CVariant::EResult CVariant::GetDouble(size_t Size, void* Value) const
{
	if(m_Type == VAR_TYPE_EMPTY) {
		MemZero(Value, Size);
		return eErrNone;
	}

	if(m_Type != VAR_TYPE_DOUBLE)
		return eErrTypeMismatch;

	size_t uPayloadSize = GetSize();
	const byte* pPayload = GetData();

	if(Size != uPayloadSize)
		return eErrIntegerOverflow; // technicaly double but who cares

	MemCopy(Value, pPayload, Size);
	return eErrNone;
}
#endif

CBuffer CVariant::ToBuffer(bool bShared) const 
{
	CBuffer Buffer(Allocator());
	EResult Err = GetBuffer(Buffer, bShared);
	if(Err) Throw(Err);
	return Buffer;
}

CVariant::EResult CVariant::GetBuffer(CBuffer& Buffer, bool bShared) const
{
	if (m_Type == VAR_TYPE_EMPTY)
		return eErrIsEmpty;
	if (!IsRaw())
		return eErrTypeMismatch;

	if (m_bContainer)
	{
		switch (m_Type)
		{
		case VAR_TYPE_BYTES:
			Buffer.AttachData(m_p.Buffer);
			Buffer.m_uSize = m_uPayloadSize;
			Buffer.m_uPosition = 0;
			return eErrNone;
		case VAR_TYPE_ASCII:
		case VAR_TYPE_UTF8:
			if (!Buffer.CopyBuffer(m_p.StrA->Data, m_p.StrA->Length + 1)) // take the null terminator
				return eErrAllocFailed;
			return eErrNone;
		case VAR_TYPE_UNICODE:
			if (!Buffer.CopyBuffer(m_p.StrW->Data, m_p.StrW->Length + 1) * sizeof(wchar_t)) // take the null terminator
				return eErrAllocFailed;
			return eErrNone;
		default:
			return eErrTypeMismatch;
		}
	}

	if (!Buffer.CopyBuffer(GetData(), GetSize()))
		return eErrAllocFailed;

	if (m_Type == VAR_TYPE_BYTES && bShared)
	{
		CVariant* This = (CVariant*)this;
		This->DetachPayload();
		This->InitFromBuffer(Buffer.m_p.Buffer, Buffer.m_uSize);
	}

	return eErrNone;
}

FW::StringA CVariant::ToStringA(bool bShared) const
{
	FW::StringA StrA(Allocator());
	EResult Err = GetStringA(StrA, bShared);
	if(Err) Throw(Err);
	return StrA;
}

CVariant::EResult CVariant::GetStringA(FW::StringA& StrA, bool bShared) const
{
	if(m_Type == VAR_TYPE_EMPTY)
		return eErrIsEmpty;
	
	if (m_Type == VAR_TYPE_ASCII || m_Type == VAR_TYPE_UTF8)
	{
		if (m_bContainer)
		{
			StrA.AttachData(m_p.StrA);
		}
		else
		{
			StrA.Assign((char*)GetData(), GetSize());

			if (bShared)
			{
				CVariant* This = (CVariant*)this;
				This->DetachPayload();
				This->InitFromStringA(StrA.m_ptr);
			}
		}
	}
	else if (m_Type == VAR_TYPE_UNICODE)
	{
		FW::StringW StrW;
		GetStringW(StrW, bShared);
		StrA = ToUtf8(StrW.Allocator(), StrW.ConstData(), StrW.Length());
	}
	else if(m_Type == VAR_TYPE_BYTES)
		StrA.Assign((char*)GetData(), GetSize());
	else
		return eErrTypeMismatch;
	return eErrNone;
}

FW::StringW CVariant::ToStringW(bool bShared) const
{
	FW::StringW StrW(Allocator());
	EResult Err = GetStringW(StrW, bShared);
	if(Err) Throw(Err);
	return StrW;
}

CVariant::EResult CVariant::GetStringW(FW::StringW& StrW, bool bShared) const
{
	if(m_Type == VAR_TYPE_EMPTY)
		return eErrIsEmpty;

	if (m_Type == VAR_TYPE_UNICODE)
	{
		if (m_bContainer)
		{
			StrW.AttachData(m_p.StrW);
		}
		else
		{
			StrW.Assign((wchar_t*)GetData(), GetSize() / sizeof(wchar_t));

			if (bShared)
			{
				CVariant* This = (CVariant*)this;
				This->DetachPayload();
				This->InitFromStringW(StrW.m_ptr);
			}
		}
	}
	else if (m_Type == VAR_TYPE_UTF8)
		StrW = FromUtf8(m_pMem, (char*)GetData(), GetSize());
	else if(m_Type == VAR_TYPE_ASCII || m_Type == VAR_TYPE_BYTES)
		StrW.Assign((char*)GetData(), GetSize());
	else
		return eErrTypeMismatch;
	return eErrNone;
}

int	CVariant::Compare(const CVariant &R) const
{
	// we compare only actual payload, content not the declared type or auxyliary informations

	CBuffer l(m_pMem);
	auto lErr = MkPayload(&l); // for non containers this is fast, CBuffer only gets a pointer
	
	CBuffer r(m_pMem);
	auto rErr = R.MkPayload(&r); // same here

	if (lErr || rErr)
	{
		if(lErr && rErr) return 0;
		else if(lErr) return -1;
		else return 1;
	}
	return l.Compare(r); 
}

FW::RetValue<CVariant::TMap*, CVariant::EResult> CVariant::GetMap(bool bCanInit, bool bExclusive) const
{
	CVariant* This = (CVariant*)this;

	if (m_Type == VAR_TYPE_EMPTY && bCanInit)
		This->m_Type = VAR_TYPE_MAP;
	else if (m_Type != VAR_TYPE_MAP)
		return eErrTypeMismatch;

	if (m_bContainer) 
	{
		if (bExclusive && m_p.Map->Refs > 1)
		{
			TMap* pMap = CloneMap(m_p.Map, true);
			if (!pMap) return eErrAllocFailed;
			This->DetachPayload();
			This->InitFromContainer(pMap, VAR_TYPE_MAP);
		}
		return m_p.Map;
	}

	size_t Size = GetSize();
	if (Size == 0 && !bCanInit)
		return eErrIsEmpty;

	TMap* pMap = AllocMap();
	if (!pMap)
		return eErrAllocFailed;

	if (Size > 0)
	{
		CBuffer Packet(m_pMem, GetData(), Size, true);
		while (Packet.GetSizeLeft())
		{
			bool bOk;
			uint8 Len = Packet.ReadValue<uint8>(&bOk);
			if (!bOk) return eErrBufferShort;
			if (Packet.GetSizeLeft() < Len)
				return eErrInvalidName;
			const char* pName = (char*)Packet.ReadData(Len);
			if (!pName) return eErrInvalidName;

			CVariant Data(m_pMem);
			EResult Err = Data.FromPacket(&Packet, true);
			if (Err) return Err;

			if (!MapInsert(pMap, pName, Len, &Data, true))
				return eErrAllocFailed;
		}
	}
	
	This->InitFromContainer(pMap, VAR_TYPE_MAP);
	return pMap;
}

FW::RetValue<CVariant::TList*, CVariant::EResult> CVariant::GetList(bool bCanInit, bool bExclusive) const
{
	CVariant* This = (CVariant*)this;

	if (m_Type == VAR_TYPE_EMPTY && bCanInit)
		This->m_Type = VAR_TYPE_LIST;
	else if(m_Type != VAR_TYPE_LIST)
		return eErrTypeMismatch;

	if (m_bContainer)
	{
		if (bExclusive && m_p.List->Refs > 1)
		{
			TList* pList = CloneList(m_p.List, true);
			if (!pList) return eErrAllocFailed;
			This->DetachPayload();
			This->InitFromContainer(pList, VAR_TYPE_LIST);
		}
		return m_p.List;
	}

	size_t Size = GetSize();
	if (Size == 0 && !bCanInit)
		return eErrIsEmpty;

	TList* pList = AllocList();
	if (!pList)
		return eErrAllocFailed;

	if (Size > 0)
	{
		CBuffer Packet(m_pMem, GetData(), Size, true);
		while (Packet.GetSizeLeft())
		{
			CVariant Data(m_pMem);
			EResult Err = Data.FromPacket(&Packet, true);
			if(Err) return Err;

			if(!ListAppend(pList, &Data))
				return eErrAllocFailed;
		}
	}

	This->InitFromContainer(pList, VAR_TYPE_LIST);
	return pList;
}

FW::RetValue<CVariant::TIndex*, CVariant::EResult> CVariant::GetIndex(bool bCanInit, bool bExclusive) const
{
	CVariant* This = (CVariant*)this;

	if (m_Type == VAR_TYPE_EMPTY && bCanInit)
		This->m_Type = VAR_TYPE_INDEX;
	else if (m_Type != VAR_TYPE_INDEX)
		return eErrTypeMismatch;

	if (m_bContainer)
	{
		if (bExclusive && m_p.Index->Refs > 1)
		{
			TIndex* pIndex = CloneIndex(m_p.Index, true);
			if (!pIndex) return eErrAllocFailed;
			This->DetachPayload();
			This->InitFromContainer(pIndex, VAR_TYPE_INDEX);
		}
		return m_p.Index;
	}

	size_t Size = GetSize();
	if (Size == 0 && !bCanInit)
		return eErrIsEmpty;

	TIndex* pIndex = AllocIndex();
	if (!pIndex)
		return eErrAllocFailed;

	if (Size > 0)
	{
		CBuffer Packet(m_pMem, GetData(), Size, true);
		while (Packet.GetSizeLeft())
		{
			bool bOk;
			uint32 Id = Packet.ReadValue<uint32>(&bOk);
			if(!bOk) return eErrBufferShort;

			CVariant Data(m_pMem);
			EResult Err = Data.FromPacket(&Packet, true);
			if(Err) return Err;

			if(!IndexInsert(pIndex, Id, &Data, true))
				return eErrAllocFailed;
		}
	}

	This->InitFromContainer(pIndex, VAR_TYPE_INDEX);
	return pIndex;
}

CVariant::EResult CVariant::MkPayload(CBuffer* pBuffer) const
{
	// build the payload if required
	if (!m_bReadOnly && m_bContainer)
	{
		switch(m_Type)
		{
			case VAR_TYPE_MAP:
			{
				auto Ret = GetMap(false, false);
				if (Ret.Error) return Ret.Error == eErrIsEmpty ? eErrNone : Throw(Ret.Error);
				for (auto I = MapBegin(Ret.Value); I != MapEnd(); ++I)
				{
					const FW::StringA& sKey = I.Key();
					ASSERT(sKey.Length() < 0xFF);
					uint8 Len = (uint8)sKey.Length();
					if (!pBuffer->WriteValue<uint8>(Len)) return Throw(eErrBufferWriteFailed);
					if (!pBuffer->WriteData(sKey.ConstData(), Len)) return Throw(eErrBufferWriteFailed);

					EResult Err = I.Value().ToPacket(pBuffer);
					if (Err) return Throw(Err);
				}
				return eErrNone;
			}
			case VAR_TYPE_LIST:
			{
				auto Ret = GetList(false, false);
				if (Ret.Error) return Ret.Error == eErrIsEmpty ? eErrNone : Throw(Ret.Error);
				for (uint32 i = 0; i < Ret.Value->Count; i++)
				{
					EResult Err = ((CVariant*)Ret.Value->Data)[i].ToPacket(pBuffer);
					if (Err) return Throw(Err);
				}
				return eErrNone;
			}
			case VAR_TYPE_INDEX:
			{
				auto Ret = GetIndex(false, false);
				if (Ret.Error) return Ret.Error == eErrIsEmpty ? eErrNone : Throw(Ret.Error);
				for (auto I = IndexBegin(Ret.Value); I != IndexEnd(); ++I)
				{
					if (!pBuffer->WriteValue<uint32>(I.Key().u)) return Throw(eErrBufferWriteFailed);

					EResult Err = I.Value().ToPacket(pBuffer);
					if (Err) return Throw(Err);
				}
				return eErrNone;
			}
		}
	}

	// set buffer by reference if possible - as we currently always use a derived buffer this has no advantage
	//if (m_Type == VAR_TYPE_BYTES && m_bContainer)
	//{
	//	pBuffer->Clear();
	//	pBuffer->u.pContainer = m_p.Buffer;
	//	InterlockedIncrement(&pBuffer->u.pContainer->Refs);
	//	pBuffer->m_uSize = m_uPayloadSize;
	//	return eErrNone;
	//}
	
	// set buffer by value
	if (!pBuffer->SetBuffer((void*)GetData(), GetSize(), true))
		return Throw(eErrBufferWriteFailed);
	return eErrNone; 
}

CVariant::EResult CVariant::Clone(const CVariant* From, CVariant* To, bool Full)
{
	if (!From->m_bContainer)
		return To->InitValue(From->m_Type, From->GetSize(), From->GetData());

	switch (From->m_Type)
	{
		case VAR_TYPE_MAP:
		{
			SMap* pMap = To->CloneMap(From->m_p.Map, Full);
			if (!pMap) return Throw(eErrAllocFailed);
			To->InitFromContainer(pMap, VAR_TYPE_MAP);
			break;
		}
		case VAR_TYPE_LIST:
		{
			SList* pList = To->CloneList(From->m_p.List, Full);
			if (!pList) return Throw(eErrAllocFailed);
			To->InitFromContainer(pList, VAR_TYPE_LIST);
			break;
		}
		case VAR_TYPE_INDEX:
		{
			SIndex* pIndex = To->CloneIndex(From->m_p.Index, Full);
			if (!pIndex) return Throw(eErrAllocFailed);
			To->InitFromContainer(pIndex, VAR_TYPE_INDEX);
			break;
		}

		case VAR_TYPE_BYTES:
		{
			CBuffer::SBuffer* pBuffer = To->AllocBuffer(From->m_uPayloadSize);
			if (!pBuffer) return Throw(eErrAllocFailed);
			MemCopy(pBuffer->Data, From->m_p.Buffer->Data, From->m_uPayloadSize);
			To->InitFromBuffer(pBuffer, From->m_uPayloadSize);
			break;
		}
		case VAR_TYPE_ASCII:
		case VAR_TYPE_UTF8:
		{
			FW::StringA::SStringData* pStrA = To->AllocStringA(From->m_p.StrA->Length);
			if (!pStrA) return Throw(eErrAllocFailed);
			MemCopy(pStrA->Data, From->m_p.StrA->Data, From->m_p.StrA->Length + 1);
			To->InitFromStringA(pStrA);
			break;
		}
		case VAR_TYPE_UNICODE:
		{
			FW::StringW::SStringData* pStrW = To->AllocStringW(From->m_p.StrW->Length);
			if (!pStrW) return Throw(eErrAllocFailed);
			MemCopy(pStrW->Data, From->m_p.StrW->Data, (From->m_p.StrW->Length  + 1) * sizeof(wchar_t));
			To->InitFromStringW(pStrW);
			break;
		}

		default:
			ASSERT(0);
			To->InitValue(VAR_TYPE_EMPTY, 0, nullptr);
	}
	return eErrNone;
}

/////////////////////////////////////////////////////////////////////////
// Containers

CVariant::TMap* CVariant::AllocMap(uint32 BucketCount) const
{
	TMap* pMap = (TMap*)MemAlloc(sizeof(TMap) + sizeof(TMap::Table::SBucketEntry) * BucketCount);
	if (pMap) {
		pMap->Refs = 0;
		pMap->pRawPayload = nullptr;
		pMap->Count = 0;
		pMap->BucketCount = BucketCount;
		MemSet(pMap->Buckets, 0, sizeof(TMap::Table::SBucketEntry*) * BucketCount);
	}
	return pMap;
}

CVariant::TMap* CVariant::CloneMap(const TMap* pSrcMap, bool Full) const
{
	TMap* pMap = AllocMap(pSrcMap->BucketCount);
	if (!pMap) return nullptr;
	for (auto I = MapBegin(pSrcMap); I != MapEnd(); ++I) {
		CVariant Value(m_pMem);
		if(Full) Clone(&I.Value(), &Value, true);
		else Value.InitAssign(I.Value());
		MapInsert(pMap, I.Key().ConstData(), I.Key().Length(), &Value, true);
	}
	return pMap;
}

CVariant* CVariant::MapInsert(TMap*& pMap, const char* Name, size_t Len, const CVariant* pVariant, bool bUnsafe) const
{
	ASSERT(pMap->Refs <= 1);

	FW::StringA NameA(m_pMem, Name, Len);
	if (!bUnsafe)
	{
		TMap::Table::SBucketEntry* pEntry = TMap::Table::FindEntry(pMap->Buckets, pMap->BucketCount, NameA);
		if (pEntry) {
			if (pVariant) pEntry->Value = *pVariant;
			else pEntry->Value.Clear();
			return &pEntry->Value;
		}
	}

	if ((pMap->Count >> 0) >= pMap->BucketCount && (pMap->BucketCount & 0x80000000) == 0)
	{
		uint32 BucketCount = (pMap->BucketCount > 0) ? (pMap->BucketCount << 1) : 1;
		TMap* ptr = AllocMap(BucketCount);

		TMap::Table::MoveTable(ptr->Buckets, ptr->BucketCount, pMap->Buckets, pMap->BucketCount);
		ptr->Count = pMap->Count;
		ptr->pRawPayload = pMap->pRawPayload;
		ptr->Refs = pMap->Refs; // 0 or 1
		FreeMap(pMap);
		pMap = ptr;
	}

	TMap::Table::SBucketEntry* pEntry = (TMap::Table::SBucketEntry*)MemAlloc(sizeof(TMap::Table::SBucketEntry));
	if (!pEntry) return nullptr;
	if (pVariant) new (pEntry) TMap::Table::SBucketEntry(NameA, *pVariant);
	else new (pEntry) TMap::Table::SBucketEntry(NameA, CVariant(m_pMem));
	pMap->Count++;
	TMap::Table::InsertEntry(pMap->Buckets, pMap->BucketCount, pEntry);
	return &pEntry->Value;
}

void CVariant::MapRemove(TMap* pMap, const char* Name) const
{
	uint8 PoolMem[320]; // 256 name + 64 spare
	FW::StackedMem Pool(PoolMem, sizeof(PoolMem));
	FW::StringA NameA(&Pool, Name);
	pMap->Count -= TMap::Table::RemoveImpl(m_pMem, pMap->Buckets, pMap->BucketCount, nullptr, nullptr, NameA, false);
}

CVariant::TMap::Iterator CVariant::MapFind(TMap* pMap, const char* Name)
{
	uint8 PoolMem[320]; // 256 name + 64 spare
	FW::StackedMem Pool(PoolMem, sizeof(PoolMem));
	FW::StringA NameA(&Pool, Name);
	return TMap::Iterator(NameA, pMap->Buckets, pMap->BucketCount);
}

void CVariant::FreeMap(TMap* pMap) const
{
	if (!pMap) return;
	for(uint32 i = 0; i < pMap->BucketCount; i++) {
		for (TMap::Table::SBucketEntry* Curr = pMap->Buckets[i]; Curr; ) {
			TMap::Table::SBucketEntry* pEntry = Curr;
			Curr = Curr->Next;
			pEntry->~SBucketEntry();
			MemFree(pEntry);
		}
	}
	MemFree(pMap);
}

CVariant::TList* CVariant::AllocList(uint32 Capacity) const
{
	TList* pList = (TList*)MemAlloc(sizeof(TList) + sizeof(CVariant) * Capacity);
	if (pList) {
		pList->Refs = 0;
		pList->pRawPayload = nullptr;
		pList->Count = 0;
		pList->Capacity = Capacity;
	}
	return pList;
}

CVariant::TList* CVariant::CloneList(const TList* pSrcList, bool Full) const
{
	TList* pList = AllocList(pSrcList->Count);
	if (!pList) return nullptr;
	for (uint32 i = 0; i < pSrcList->Count; i++) {
		CVariant Value(m_pMem);
		if(Full) Clone(&((CVariant*)pSrcList->Data)[i], &Value, true);
		else Value.InitAssign(((CVariant*)pSrcList->Data)[i]);
		ListAppend(pList, &Value);
	}
	return pList;
}

CVariant* CVariant::ListAppend(TList*& pList, const CVariant* pVariant) const
{
	ASSERT(pList->Refs <= 1);

	if (pList->Count >= pList->Capacity)
	{
		uint32 uCapacity = pList->Capacity * 3 / 2; // Note: min/start size is 4
		TList* ptr = AllocList(uCapacity);

		for (uint32 i = 0; i < pList->Count; i++)
			new (&((CVariant*)ptr->Data)[i]) CVariant(((CVariant*)pList->Data)[i]);
		ptr->Count = pList->Count;
		ptr->pRawPayload = pList->pRawPayload;
		ptr->Refs = pList->Refs; // 0 or 1
		FreeList(pList);
		pList = ptr;
	}

	CVariant* pEntry = &((CVariant*)pList->Data)[pList->Count];
	if(pVariant) new (pEntry) CVariant(*pVariant);
	else new (pEntry) CVariant(m_pMem);
	pList->Count++;
	return pEntry;
}

void CVariant::ListRemove(TList* pList, uint32 Index) const
{
	if(Index >= pList->Count)
		return;
	for (uint32 i = Index; i < pList->Count - 1; i++)
		((CVariant*)pList->Data)[i] = ((CVariant*)pList->Data)[i + 1];
	((CVariant*)pList->Data)[pList->Count].~CVariant();
}

void CVariant::FreeList(TList* pList) const
{
	if (!pList) return;
	for(size_t i = 0; i < pList->Count; i++)
		((CVariant*)pList->Data)[i].~CVariant();
	MemFree(pList);
}

CVariant::TIndex* CVariant::AllocIndex(uint32 BucketCount) const
{
	TIndex* pIndex = (TIndex*)MemAlloc(sizeof(TIndex) + sizeof(TIndex::Table::SBucketEntry) * BucketCount);
	if (pIndex) {
		pIndex->Refs = 0;
		pIndex->pRawPayload = nullptr;
		pIndex->Count = 0;
		pIndex->BucketCount = BucketCount;
		MemSet(pIndex->Buckets, 0, sizeof(TMap::Table::SBucketEntry*) * BucketCount);
	}
	return pIndex;
}

CVariant::TIndex* CVariant::CloneIndex(const TIndex* pSrcIndex, bool Full) const
{
	TIndex* pIndex = AllocIndex(pSrcIndex->BucketCount);
	if (!pIndex) return nullptr;

	for (auto I = IndexBegin(pSrcIndex); I != IndexEnd(); ++I) {
		CVariant Value(m_pMem);
		if(Full) Clone(&I.Value(), &Value, true);
		else Value.InitAssign(I.Value());
		IndexInsert(pIndex, I.Key().u, &Value, true);
	}
	return pIndex;
}

CVariant* CVariant::IndexInsert(TIndex*& pIndex, uint32 Index, const CVariant* pVariant, bool bUnsafe) const
{
	ASSERT(pIndex->Refs <= 1);

	if (!bUnsafe)
	{
		TIndex::Table::SBucketEntry* pEntry = TIndex::Table::FindEntry(pIndex->Buckets, pIndex->BucketCount, TKey{ Index });
		if (pEntry) {
			if (pVariant) pEntry->Value = *pVariant;
			else pEntry->Value.Clear();
			return &pEntry->Value;
		}
	}

	if ((pIndex->Count >> 0) >= pIndex->BucketCount && (pIndex->BucketCount & 0x80000000) == 0)
	{
		uint32 BucketCount = (pIndex->BucketCount > 0) ? (pIndex->BucketCount << 1) : 1;
		TIndex* ptr = AllocIndex(BucketCount);

		TIndex::Table::MoveTable(ptr->Buckets, ptr->BucketCount, pIndex->Buckets, pIndex->BucketCount);
		ptr->Count = pIndex->Count;
		ptr->pRawPayload = pIndex->pRawPayload;
		ptr->Refs = pIndex->Refs; // 0 or 1
		FreeIndex(pIndex);
		pIndex = ptr;
	}

	TIndex::Table::SBucketEntry* pEntry = (TIndex::Table::SBucketEntry*) MemAlloc(sizeof(TIndex::Table::SBucketEntry));
	if (!pEntry) return nullptr;
	if(pVariant) new (pEntry) TIndex::Table::SBucketEntry(TKey{ Index }, *pVariant);
	else new (pEntry) TIndex::Table::SBucketEntry(TKey{ Index }, CVariant(m_pMem));
	pIndex->Count++;
	TIndex::Table::InsertEntry(pIndex->Buckets, pIndex->BucketCount, pEntry);
	return &pEntry->Value;
}

void CVariant::IndexRemove(TIndex* pIndex, uint32 Index) const
{
	pIndex->Count -= TIndex::Table::RemoveImpl(m_pMem, pIndex->Buckets, pIndex->BucketCount, nullptr, nullptr, TKey{ Index }, false);
}

CVariant::TIndex::Iterator CVariant::IndexFind(const TIndex* pIndex, uint32 Index)
{
	return TIndex::Iterator(TKey{ Index }, ((TIndex*)pIndex)->Buckets, pIndex->BucketCount);
}

void CVariant::FreeIndex(TIndex* pIndex) const
{
	if (!pIndex) return;
	for(uint32 i = 0; i < pIndex->BucketCount; i++) {
		for (TIndex::Table::SBucketEntry* Curr = pIndex->Buckets[i]; Curr; ) {
			TIndex::Table::SBucketEntry* pEntry = Curr;
			Curr = Curr->Next;
			pEntry->~SBucketEntry();
			MemFree(pEntry);
		}
	}
	MemFree(pIndex);
}

CBuffer::SBuffer* CVariant::AllocBuffer(size_t uSize) const
{
	CBuffer::SBuffer* Buffer = (CBuffer::SBuffer*)MemAlloc(sizeof(CBuffer::SBuffer) + uSize);
	if(!Buffer) 
		return nullptr;
	Buffer->Refs = 0;
	Buffer->uCapacity = uSize;
	return Buffer; 
}

FW::StringA::SStringData* CVariant::AllocStringA(size_t uSize) const
{
	FW::StringA::SStringData* StrA = (FW::StringA::SStringData*)MemAlloc(sizeof(FW::StringA::SStringData) + uSize);
	if (!StrA)
		return nullptr;
	StrA->Refs = 0;
	StrA->Length = uSize;
	StrA->Capacity = uSize;
	return StrA;
}

FW::StringW::SStringData* CVariant::AllocStringW(size_t uSize) const
{
	FW::StringW::SStringData* StrW = (FW::StringW::SStringData*)MemAlloc(sizeof(FW::StringA::SStringData) + uSize);
	if (!StrW)
		return nullptr;
	StrW->Refs = 0;
	StrW->Length = uSize / sizeof(wchar_t);
	StrW->Capacity = uSize / sizeof(wchar_t);
	return StrW;
}

FW_NAMESPACE_END