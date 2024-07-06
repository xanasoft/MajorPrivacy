//#include "pch.h"

// machine types
#include "../Types.h"

#include "../../Framework/Header.h"

#ifdef VAR_NO_STL_STR
void* __cdecl operator new(size_t size, void* ptr);
/*{
	return ptr;
}*/
#endif

#include "Variant.h"
//#include "../../zlib/zlib.h"
#include "../../Framework/Memory.h"
#ifndef VAR_NO_EXCEPTIONS
#include "Exception.h"
#endif
#include "../../Framework/UniquePtr.h"

#ifdef VAR_USE_POOL_ALLOCATOR
#include "PoolAllocator.h"
#include <mutex>

static std::recursive_mutex g_Var_AllocatorMutex;
static PoolAllocator<sizeof(CVariant::SVariant)> g_Var_Allocator;
#endif

FW::DefaultMemPool g_VariantDefaultMemPool;


CVariant::CVariant(FW::AbstractMemPool* pMemPool, EType Type)
	: FW::AbstractContainer(pMemPool)
{
	if(Type != VAR_TYPE_EMPTY)
		InitValue(Type, 0, NULL);
	else
		m_Variant = NULL;
}

CVariant::CVariant(const CVariant& Variant)
	: FW::AbstractContainer(Variant)
{
	m_Variant = NULL;
	EResult Err = Assign(Variant);
#ifndef VAR_NO_EXCEPTIONS
	if (Err) throw CException(ErrorString(Err));
#else
	UNREFERENCED_PARAMETER(Err);
#endif
}

CVariant::~CVariant()
{
	Clear();
}

CVariant::EResult CVariant::Throw(EResult Error)
{
#ifdef KERNEL_MODE
	//DbgPrintEx(DPFLTR_DEFAULT_ID, 0xFFFFFFFF, "CVariant::Throw: %s\n", ErrorString(Error));
#endif
#ifndef VAR_NO_EXCEPTIONS
	throw CException(ErrorString(Error));
#endif
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

CVariant::SVariant* CVariant::AllocData() const
{
	FW::AbstractMemPool* pMem = m_pMem ? m_pMem : &g_VariantDefaultMemPool;
#ifdef VAR_USE_POOL_ALLOCATOR
	std::unique_lock Lock(g_Var_AllocatorMutex);
	CVariant::SVariant* ptr = (CVariant::SVariant*)g_Var_Allocator.allocate(sizeof(SVariant));
#else
	CVariant::SVariant* ptr = (CVariant::SVariant*)pMem->Alloc(sizeof(SVariant));
#endif
	if (!ptr) return NULL;
	new (ptr) SVariant;
	ptr->pMem = pMem;
	return ptr;
}

void CVariant::FreeData(SVariant* ptr) const
{
	if (!ptr) return;
	FW::AbstractMemPool* pMem = ptr->pMem ? ptr->pMem : &g_VariantDefaultMemPool;
	ptr->~SVariant();
#ifdef VAR_USE_POOL_ALLOCATOR
	std::unique_lock Lock(g_Var_AllocatorMutex);
	g_Var_Allocator.free(ptr);
#else
	pMem->Free(ptr);
#endif
}

CVariant::EResult CVariant::Assign(const CVariant& Other) noexcept
{
	auto pOther = Other.Val(true);
	if (pOther.Error == eErrIsEmpty) 
	{
		Clear();
		return eErrNone;
	}
	if(pOther.Error) return pOther.Error;

	if (m_Variant && m_Variant->Access != eReadWrite)
		return eErrNotWritable;

	if(pOther.Value->Access == eDerived)
	{
		// Note: We can not asign buffer derivations, as the original variant may be removed at any time,
		//	those we must copy the buffer on accessey by value
		SVariant* VariantCopy = AllocData();
		if(!VariantCopy) return eErrAllocFailed;
		VariantCopy->Type = pOther.Value->Type;
		VariantCopy->Size = pOther.Value->Size;
		if (!VariantCopy->AllocPayload()) {
			FreeData(VariantCopy);
			return eErrAllocFailed;
		}
		MemCopy(VariantCopy->Payload,pOther.Value->Payload, VariantCopy->Size);
		VariantCopy->Access = eReadOnly;
		Attach(VariantCopy);
	}
	else
		Attach(Other.m_Variant);
	return eErrNone;
}

void CVariant::Clear() noexcept
{
	if (!m_Variant)
		return;
#ifndef VAR_NO_STL
	if(m_Variant->Refs.fetch_sub(1) == 1)
#else
	if(InterlockedDecrement(&m_Variant->Refs) == 0)
#endif
		FreeData(m_Variant);
	m_Variant = NULL;
}

CVariant CVariant::Clone(bool Full) const
{
	auto Variant = Val();

	CVariant NewVariant(m_pMem);
	if (!Variant.Error) {
		FW::UniquePtr pClone(CVariant::AllocData(), [this](SVariant* ptr) { FreeData(ptr); });
		if (pClone) {
			EResult Err = Variant.Value->Clone(pClone, Full);
			if (!Err) 
				NewVariant.Attach(pClone.Detach());
#ifndef VAR_NO_EXCEPTIONS
			else throw CException(ErrorString(Err));
#endif
		}
#ifndef VAR_NO_EXCEPTIONS
		else throw CException(ErrorString(eErrAllocFailed));
#endif
	}
	return NewVariant;
}

void CVariant::Attach(SVariant* Variant) noexcept
{
	Clear();

	if (m_pMem != Variant->pMem) {
		m_pMem = Variant->pMem; // todo
	}

	if (!Variant) 
		return;
	m_Variant = Variant;
#ifndef VAR_NO_STL
	m_Variant->Refs.fetch_add(1);
#else
	InterlockedIncrement(&m_Variant->Refs);
#endif
}

bool CVariant::InitValue(EType Type, size_t Size, const void* Value, bool bTake)
{
	ASSERT(m_Variant == NULL);

	m_Variant = AllocData();
	if (!m_Variant) {
#ifndef VAR_NO_EXCEPTIONS
		throw CException(ErrorString(eErrAllocFailed));
#endif
		return false;
	}
#ifndef VAR_NO_STL
	m_Variant->Refs.fetch_add(1);
#else
	m_Variant->Refs = 1;
#endif

	if (!m_Variant->Init(Type, Size, Value, bTake)) {
#ifndef VAR_NO_EXCEPTIONS
		throw CException(ErrorString(eErrAllocFailed));
#endif
		return false;
	}
	return true;
}

FW::RetValue<CVariant::SVariant*, CVariant::EResult> CVariant::Val(bool NoAlloc) const
{
	if (!m_Variant) 
	{
		if (NoAlloc)
			return eErrIsEmpty;
		if(!((CVariant*)this)->InitValue(VAR_TYPE_EMPTY, 0, NULL))
			return eErrAllocFailed;
	}
	return m_Variant;
}

FW::RetValue<CVariant::SVariant*, CVariant::EResult> CVariant::Val()
{
	if (!m_Variant) 
	{
		if(!InitValue(VAR_TYPE_EMPTY, 0, NULL))
			return eErrAllocFailed;
	}
	else if (m_Variant->Refs > 1) 
	{
		FW::UniquePtr pClone(CVariant::AllocData(), [this](SVariant* ptr) { FreeData(ptr); });
		if (!pClone) return Throw(eErrAllocFailed);
		EResult Err = m_Variant->Clone(pClone);
		if (Err) return Throw(Err);
		Attach(pClone.Detach());
	}
	return m_Variant;
}

CVariant::EResult CVariant::Freeze()
{
	auto pVal = Val();
	if (pVal.Error) return Throw(pVal.Error);
	if (pVal.Value->Access == eReadOnly)
		return Throw(eErrNotWritable);

	// Note: we must flly serialize and deserialize the variant in order to not store maps and std::list content multiple times in memory
	//	The CPU overhead should be howeever minimal as we imply parse on read for the dictionaries, 
	//	and we usually dont read the variant afterwards but put in on the socket ot save it to a file
	CBuffer Packet(m_pMem);
	EResult Err = ToPacket(&Packet);
	if(Err) return Throw(Err);
	Packet.SetPosition(0);
	return FromPacket(&Packet);
}

CVariant::EResult CVariant::Unfreeze()
{
	auto pVal = Val();
	if (pVal.Error) return Throw(pVal.Error);
	if (pVal.Value->Access == eReadOnly)
		return Throw(eErrNotWritable);

	FW::UniquePtr pClone(CVariant::AllocData(), [this](SVariant* ptr) { FreeData(ptr); });
	if (!pClone) return Throw(eErrAllocFailed);
	EResult Err = pVal.Value->Clone(pClone);
	if (Err) return Throw(Err);
	Attach(pClone.Detach());
	return eErrNone;
}

bool CVariant::IsFrozen() const
{
	auto pVal = Val(true);
	if(pVal.Error) return false;
	return pVal.Value->Access != eReadWrite;
}

CVariant::EResult CVariant::FromPacket(const CBuffer* pPacket, bool bDerived)
{
	Clear();
	auto pVal = Val();
	if (pVal.Error) return Throw(pVal.Error);

	pVal.Value->Size = ReadHeader(pPacket, &pVal.Value->Type);
	if (pVal.Value->Size == 0xFFFFFFFF) return Throw(eErrInvalidHeader);
	
	if(bDerived)
	{
		pVal.Value->Access = eDerived;
		pVal.Value->Payload = pPacket->ReadData(pVal.Value->Size);
		if(!pVal.Value->Payload) return Throw(eErrBufferShort);
	}
	else
	{
		pVal.Value->Access = eReadOnly;
//#ifdef HAS_ZLIB // deprecated
//		if((Type & VAR_LEN32) == 0) // is it packed
//		{
//			byte* pData = pPacket->ReadData(pVal.Value->Size);
//
//			uLongf newsize = pVal.Value->Size*10+300;
//			uLongf unpackedsize = 0;
//			int result = 0;
//			CBuffer Buffer(m_pMem);
//			do
//			{
//				Buffer.AllocBuffer(newsize, true);
//				unpackedsize = newsize;
//				result = uncompress(Buffer.GetBuffer(),&unpackedsize,pData,pVal.Value->Size);
//				newsize *= 2; // size for the next try if needed
//			}
//			while (result == Z_BUF_ERROR && newsize < Max(MB2B(16), pVal.Value->Size*100));	// do not allow the unzip buffer to grow infinetly,
//																					// assume that no packetcould be originaly larger than the UnpackLimit nd those it must be damaged
//			if (result == Z_OK)
//			{
//				pVal.Value->Size = unpackedsize;
//				pVal.Value->AllocPayload();
//				MemCopy(pVal.Value->Payload, Buffer.GetBuffer(), pVal.Value->Size);
//			}
//			else
//				return;
//		}
//		else
//#endif
		{
			if (!pVal.Value->AllocPayload())
				return Throw(eErrAllocFailed);
			MemCopy(pVal.Value->Payload, pPacket->ReadData(pVal.Value->Size), pVal.Value->Size);
		}
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

CVariant::EResult CVariant::ToPacket(CBuffer* pPacket/*, bool bPack*/) const
{
	auto pVal = Val();
	if (pVal.Error) return Throw(pVal.Error);

	CBuffer Payload(m_pMem);
	EResult Err = pVal.Value->MkPayload(&Payload);
	if(Err) return Throw(Err);

//#ifdef HAS_ZLIB // deprecated
//	if(bPack)
//	{
//		ASSERT(Payload.GetSize() + 300 < 0xFFFFFFFF);
//		uLongf newsize = (uLongf)(Payload.GetSize() + 300);
//		CBuffer Buffer(m_pMem, newsize);
//		int result = compress2(Buffer.GetBuffer(),&newsize,Payload.GetBuffer(),(uLongf)Payload.GetSize(),Z_BEST_COMPRESSION);
//
//		if (result == Z_OK && Payload.GetSize() > newsize) // does the compression helped?
//		{
//			ASSERT(pVal.Value->Type != 0xFF);
//			pPacket->WriteValue<uint8>(pVal.Value->Type | VAR_LEN64);
//			pPacket->WriteValue<uint32>(newsize);
//			pPacket->WriteData(Buffer.GetBuffer(), newsize);
//			return;
//		}
//	}
//#endif
	
	return ToPacket(pPacket, pVal.Value->Type, Payload.GetSize(), Payload.GetBuffer());
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
	auto pVal = Val(true);
	if(pVal.Error) return 0;
	return pVal.Value->Count();
}

bool CVariant::IsMap() const // Map
{
	auto pVal = Val(true);
	if(pVal.Error) return false;
	return pVal.Value->Type == VAR_TYPE_MAP;
}

const char* CVariant::Key(uint32 Index) const // Map
{
	auto pVal = Val(true);
	if(pVal.Error) return NULL;
	return pVal.Value->Key(Index);
}

#ifndef VAR_NO_STL_STR
std::wstring CVariant::WKey(uint32 Index) const // Map
{
	std::wstring Name; 
	const char* pName = Key(Index);
	if (pName) AsciiToWStr(Name, pName);
	return Name;
}
#endif

FW::RetValue<const CVariant*, CVariant::EResult> CVariant::PtrAt(const char* Name) const // Map
{
	auto pVal = Val(true);
	if (pVal.Error) return eErrNotFound;

	auto pValue = pVal.Value->Get(Name);
	if(!pValue.Value) return eErrNotFound;
	return pValue.Value;
}

FW::RetValue<CVariant*, CVariant::EResult> CVariant::PtrAt(const char* Name, bool bCanAdd) // Map
{
	auto pVal = Val(!bCanAdd);
	if (pVal.Error) return bCanAdd ? pVal.Error : eErrNotFound;

	auto pValue = pVal.Value->Get(Name);
	if (!pValue.Value)
	{
		if (!bCanAdd) return eErrNotFound;
		if (pVal.Value->Access) return eErrNotWritable;

		if (pVal.Value->Type != VAR_TYPE_MAP) {
			if (!pVal.Value->Init(VAR_TYPE_MAP))
				return eErrAllocFailed;
		}

		pValue = pVal.Value->Insert(Name, NULL, true);
	}
	return pValue;
}

CVariant CVariant::Get(const char* Name) const // Map
{
	if(!Has(Name))
		return CVariant(m_pMem);
	return At(Name);
}

bool CVariant::Has(const char* Name) const // Map
{
	auto pVal = Val(true);
	if(pVal.Error) return false;
	return pVal.Value->Has(Name);
}

CVariant::EResult CVariant::Remove(const char* Name) // Map
{
	auto pVal = Val();
	if (pVal.Error) return Throw(pVal.Error);
	if (pVal.Value->Access != eReadWrite)
		return Throw(eErrNotWritable);
	return pVal.Value->Remove(Name);
}

CVariant::EResult CVariant::Insert(const char* Name, const CVariant& variant) // Map
{
	auto pVal = Val();
	if (pVal.Error) return Throw(pVal.Error);
	if (pVal.Value->Access != eReadWrite)
		return Throw(eErrNotWritable);
	if (pVal.Value->Type != VAR_TYPE_MAP) {
		if(!pVal.Value->Init(VAR_TYPE_MAP))
			return Throw(eErrAllocFailed);
	}
	return pVal.Value->Insert(Name, &variant).Error;
}

bool CVariant::IsList() const // List
{
	auto pVal = Val(true);
	if(pVal.Error) return false;
	return pVal.Value->Type == VAR_TYPE_LIST;
}

bool CVariant::IsIndex() const // Index
{
	auto pVal = Val(true);
	if(pVal.Error) return false;
	return pVal.Value->Type == VAR_TYPE_INDEX;
}

uint32 CVariant::Id(uint32 Index) const // Index
{
	auto pVal = Val(true);
	if(pVal.Error) return 0xFFFFFFFF;
	return pVal.Value->Id(Index);
}

FW::RetValue<const CVariant*, CVariant::EResult> CVariant::PtrAt(uint32 Index) const // Map
{
	auto pVal = Val(true);
	if (pVal.Error) return eErrNotFound;

	auto pValue = pVal.Value->Get(Index);
	if(!pValue.Value) return eErrNotFound;
	return pValue.Value;
}

FW::RetValue<CVariant*, CVariant::EResult> CVariant::PtrAt(uint32 Index, bool bCanAdd) // List or Index
{
	auto pVal = Val(!bCanAdd);
	if (pVal.Error) return bCanAdd ? pVal.Error : eErrNotFound;

	auto pValue = pVal.Value->Get(Index);
	if (!pValue.Value)
	{
		if (!bCanAdd) return eErrNotFound;
		if (pVal.Value->Access) return eErrNotWritable;

		if (pVal.Value->Type != VAR_TYPE_INDEX) {
			if (!pVal.Value->Init(VAR_TYPE_INDEX))
				return eErrAllocFailed;
		}

		pValue = pVal.Value->Insert(Index, NULL, true);
	}
	return pValue;
}

CVariant CVariant::Get(uint32 Index) const // List or Index
{
	if(!Has(Index))
		return CVariant(m_pMem);
	return At(Index);
}

bool CVariant::Has(uint32 Index) const // List or Index
{
	auto pVal = Val(true);
	if(pVal.Error) return false;
	return pVal.Value->Has(Index);
}

CVariant::EResult CVariant::Remove(uint32 Index) // List or Index
{
	auto pVal = Val();
	if (pVal.Error) return Throw(pVal.Error);
	if (pVal.Value->Access != eReadWrite)
		return Throw(eErrNotWritable);
	return pVal.Value->Remove(Index);
}

CVariant::EResult CVariant::Append(const CVariant& variant) // List
{
	auto pVal = Val();
	if (pVal.Value->Access != eReadWrite)
		return Throw(eErrNotWritable);
	if (pVal.Value->Type != VAR_TYPE_LIST) {
		if (!pVal.Value->Init(VAR_TYPE_LIST))
			return Throw(eErrAllocFailed);
	}
	return pVal.Value->Append(&variant);
}

CVariant::EResult CVariant::Insert(uint32 Index, const CVariant& variant) // Index
{
	auto pVal = Val();
	if (pVal.Error) return Throw(pVal.Error);
	if (pVal.Value->Access != eReadWrite)
		return Throw(eErrNotWritable);
	if (pVal.Value->Type != VAR_TYPE_INDEX) {
		if(!pVal.Value->Init(VAR_TYPE_INDEX))
			return Throw(eErrAllocFailed);
	}
	return pVal.Value->Insert(Index, &variant).Error;
}

CVariant::EResult CVariant::Merge(const CVariant& Variant)
{
	if (!IsValid()) // if this valiant is void we can merge with anything
	{
		EResult Err = Assign(Variant);
		if (Err) return Throw(Err);
	}
	else if(Variant.IsList() && IsList())
	{
		auto Ret = Variant.m_Variant->List();
		if (Ret.Error) return Throw(Ret.Error);
		for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I) {
			EResult Err = Append(*I);
			if (Err) return Throw(Err);
		}
	}
	else if(Variant.IsMap() && IsMap())
	{
		auto Ret = Variant.m_Variant->Map();
		if (Ret.Error) return Throw(Ret.Error);
		for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I) {
#ifndef VAR_NO_STL
			EResult Err = Insert(I->first.c_str(), I->second);
#else
			EResult Err = Insert(I.Key().ConstData(), I.Value());
#endif
			if (Err) return Throw(Err);
		}
	}
	else if (Variant.IsIndex() && IsIndex())
	{
		auto Ret = Variant.m_Variant->IMap();
		if (Ret.Error) return Throw(Ret.Error);
		for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I) {
#ifndef VAR_NO_STL
			EResult Err = Insert(I->first.u, I->second);
#else
			EResult Err = Insert(I.Key().u, I.Value());
#endif
			if (Err) return Throw(Err);
		}
	}
	else
		return Throw(eErrTypeMismatch);
	return eErrNone;
}

CVariant::EResult CVariant::GetInt(size_t Size, void* Value) const
{
	auto pVal = Val(true); // only error can be eErrIsEmpty
	if(pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
	{
		MemZero(Value, Size);
		return eErrNone;
	}

	if(pVal.Value->Type != VAR_TYPE_SINT && pVal.Value->Type != VAR_TYPE_UINT)
		return Throw(eErrTypeMismatch);
	if (Size < pVal.Value->Size)
	{
		for (size_t i = Size; i < pVal.Value->Size; i++) {
			if (pVal.Value->Payload[i] != 0)
				return Throw(eErrIntegerOverflow);
		}
	}

	if (Size <= pVal.Value->Size)
		MemCopy(Value, pVal.Value->Payload, Size);
	else {
		MemCopy(Value, pVal.Value->Payload, pVal.Value->Size);
		MemZero((byte*)Value + pVal.Value->Size, Size - pVal.Value->Size);
	}
	return eErrNone;
}

#ifndef VAR_NO_FPU
CVariant::EResult CVariant::GetDouble(size_t Size, void* Value) const
{
	auto pVal = Val(true); // only error can be eErrIsEmpty
	if(pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
	{
		MemZero(Value, Size);
		return eErrNone;
	}

	if(pVal.Value->Type != VAR_TYPE_DOUBLE)
		return Throw(eErrTypeMismatch);
	if(Size != pVal.Value->Size)
		return Throw(eErrIntegerOverflow); // technicaly double but who cares

	MemCopy(Value, pVal.Value->Payload, Size);
	return eErrNone;
}
#endif

#ifndef VAR_NO_STL_STR
std::string CVariant::ToString() const
{
	std::string str;
	auto pVal = Val(true); // only error can be eErrIsEmpty
	if(pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
		return str;

	if(pVal.Value->Type == VAR_TYPE_ASCII || pVal.Value->Type == VAR_TYPE_BYTES)
		str = std::string((char*)pVal.Value->Payload, pVal.Value->Size);
	else if(pVal.Value->Type == VAR_TYPE_UTF8 || pVal.Value->Type == VAR_TYPE_UNICODE)
	{
		std::wstring wstr = ToWString();
		WStrToAscii(str, wstr);
	}
#ifndef VAR_NO_EXCEPTIONS
	else
		throw CException(ErrorString(eErrTypeMismatch));
#endif
	return str;
}

std::wstring CVariant::ToWString() const
{
	std::wstring wstr;
	auto pVal = Val(true); // only error can be eErrIsEmpty
	if(pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
		return wstr;

	if(pVal.Value->Type == VAR_TYPE_UNICODE)
		wstr = std::wstring((wchar_t*)pVal.Value->Payload, pVal.Value->Size/sizeof(wchar_t));
	else if(pVal.Value->Type == VAR_TYPE_ASCII || pVal.Value->Type == VAR_TYPE_BYTES)
	{
		std::string str = std::string((char*)pVal.Value->Payload, pVal.Value->Size);
		AsciiToWStr(wstr, str);
	}
	else if(pVal.Value->Type == VAR_TYPE_UTF8)
	{
		std::string str = std::string((char*)pVal.Value->Payload, pVal.Value->Size);
		Utf8ToWStr(wstr, str);
	}
#ifndef VAR_NO_EXCEPTIONS
	else
		throw CException(ErrorString(eErrTypeMismatch));
#endif
	return wstr;
}
#endif

FW::StringA CVariant::ToStringA() const
{
	FW::StringA str;
	auto pVal = Val(true); // only error can be eErrIsEmpty
	if(pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
		return str;

	if(pVal.Value->Type == VAR_TYPE_ASCII || pVal.Value->Type == VAR_TYPE_BYTES)
		str = FW::StringA(Allocator(), (char*)pVal.Value->Payload, pVal.Value->Size);
	else if(/*pVal.Value->Type == VAR_TYPE_UTF8 ||*/ pVal.Value->Type == VAR_TYPE_UNICODE)
		str = ToStringW();
#ifndef VAR_NO_EXCEPTIONS
	else
		throw CException(ErrorString(eErrTypeMismatch));
#endif
	return str;
}

FW::StringW CVariant::ToStringW() const
{
	FW::StringW wstr;
	auto pVal = Val(true); // only error can be eErrIsEmpty
	if(pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
		return wstr;

	if(pVal.Value->Type == VAR_TYPE_UNICODE)
		wstr = FW::StringW(Allocator(), (wchar_t*)pVal.Value->Payload, pVal.Value->Size/sizeof(wchar_t));
	else if(pVal.Value->Type == VAR_TYPE_ASCII || pVal.Value->Type == VAR_TYPE_BYTES)
		wstr = FW::StringA(Allocator(), (char*)pVal.Value->Payload, pVal.Value->Size);
	/*else if(pVal.Value->Type == VAR_TYPE_UTF8) // todo
	{
		std::string str = std::string((char*)pVal.Value->Payload, pVal.Value->Size);
		Utf8ToWStr(wstr, str);
	}*/
#ifndef VAR_NO_EXCEPTIONS
	else
		throw CException(ErrorString(eErrTypeMismatch));
#endif
	return wstr;
}

CVariant::EType CVariant::GetType() const
{
	auto pVal = Val(true);
	if(pVal.Error) return VAR_TYPE_EMPTY;
	return pVal.Value->Type;
}

int	CVariant::Compare(const CVariant &R) const
{
	auto pL = Val(true);
	auto pR = R.Val(true);
	if (pL.Error || pR.Error)
	{
		if(pL.Error && pR.Error) return 0;
		else if(pL.Error) return -1;
		else return 1;
	}
	// we compare only actual payload, content not the declared type or auxyliary informations
	CBuffer l(m_pMem);
	pL.Value->MkPayload(&l); // for non containers this is fast, CBuffer only gets a pointer
	CBuffer r(m_pMem);
	pR.Value->MkPayload(&r); // same here
	return l.Compare(r); 
}

///////////////////////////////////////////////////////////////////////////////////////////
// Internal Stuff

CVariant::SVariant::SVariant()
{
	Type = VAR_TYPE_EMPTY;
	Size = 0;
	Payload = NULL;
	Access = eReadWrite;
	Refs = 0;
	Container.Void = NULL;
}

CVariant::SVariant::~SVariant()
{
	FreeContainer();
	if (Access != eDerived)
		FreePayload();
}

bool CVariant::SVariant::AllocPayload()
{
	ASSERT(Payload == NULL);
	if (Size <= sizeof(Buffer)) {
		Payload = Buffer;
		return true;
	}
	Payload = (byte*)pMem->Alloc(Size);
	return Payload != NULL;
}

void CVariant::SVariant::FreePayload()
{
	if (Payload != Buffer)
		pMem->Free(Payload);
	Payload = NULL;
}

bool CVariant::SVariant::AllocContainer()
{
	if (Access == eWriteOnly)
	{
		Container.Void = pMem->Alloc(sizeof(CBuffer));
		if (!Container.Void) return false;
		// todo mem pool for buffer
		new (Container.Buffer) CBuffer(pMem);
	}
	else 
	{
		switch (Type)
		{
#ifndef VAR_NO_STL
			case VAR_TYPE_MAP:
				Container.Void = pMem->Alloc(sizeof(std::map<std::string, CVariant>));
				if (!Container.Void) return false;
				new (Container.Map) std::map<std::string, CVariant>;
				break;
			case VAR_TYPE_LIST:
				Container.Void = pMem->Alloc(sizeof(std::vector<CVariant>));
				if (!Container.Void) return false;
				new (Container.List) std::vector<CVariant>;	
				break;
			case VAR_TYPE_INDEX:
				Container.Void = pMem->Alloc(sizeof(std::map<TIndex, CVariant>));
				if (!Container.Void) return false;
				new (Container.Index) std::map<TIndex, CVariant>;
				break;
#else
			case VAR_TYPE_MAP:
				Container.Void = pMem->Alloc(sizeof(FW::Map<FW::StringA, CVariant>));
				if (!Container.Void) return false;
				new (Container.Map) FW::Map<FW::StringA, CVariant>(pMem);
				break;
			case VAR_TYPE_LIST:
				Container.Void = pMem->Alloc(sizeof(FW::Array<CVariant>));
				if (!Container.Void) return false;
				new (Container.List) FW::Array<CVariant>(pMem);	
				break;
			case VAR_TYPE_INDEX:
				Container.Void = pMem->Alloc(sizeof(FW::Map<TIndex, CVariant>));
				if (!Container.Void) return false;
				new (Container.Index) FW::Map<TIndex, CVariant>(pMem);
				break;
#endif
		}
	}
	return true;
}

void CVariant::SVariant::FreeContainer()
{
	if (!Container.Void) 
		return;
	if (Access == eWriteOnly)
	{
		Container.Buffer->~CBuffer();
	}
	else 
	{
		switch (Type)
		{
#ifndef VAR_NO_STL
			case VAR_TYPE_MAP:		Container.Map->~map(); break;
			case VAR_TYPE_LIST:		Container.List->~vector(); break;
			case VAR_TYPE_INDEX:	Container.Index->~map(); break;
#else
			case VAR_TYPE_MAP:		Container.Map->~Map(); break;
			case VAR_TYPE_LIST:		Container.List->~Array(); break;
			case VAR_TYPE_INDEX:	Container.Index->~Map(); break;
#endif
		}
	}
	pMem->Free(Container.Void);
	Container.Void = NULL;
}

bool CVariant::SVariant::Init(EType type, size_t size, const void* payload, bool bTake)
{
	ASSERT(Access == eReadWrite);
	ASSERT(Type == VAR_TYPE_EMPTY);
	ASSERT(size < 0xFFFFFFFF); // Note: one single variant can not be bigegr than 4 GB

	Type = type;
	Size = (uint32)size;
	if(bTake)
		Payload = (byte*)payload;
	else if(Size)
	{
		if(!AllocPayload()) 
			return false;
		if(payload)
			MemCopy(Payload, payload, size);
		else
			MemZero(Payload, size);
	}
	else
		Payload = NULL;

	if(!AllocContainer()) 
		return false;

	return true;
}

#ifndef VAR_NO_STL
FW::RetValue<std::map<std::string, CVariant>*, CVariant::EResult> CVariant::SVariant::Map() const
#else
FW::RetValue<FW::Map<FW::StringA, CVariant>*, CVariant::EResult> CVariant::SVariant::Map() const
#endif
{
	ASSERT(Type == VAR_TYPE_MAP);

	if (Access == eWriteOnly)
		return Throw(eErrWriteInProgrees);
	if(Container.Map == NULL)
	{
		CBuffer Packet(pMem, Payload, Size, true);
		((CVariant::SVariant*)this)->AllocContainer();
		for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Size; )
		{
			bool bOk;
			uint8 Len = Packet.ReadValue<uint8>(&bOk);
			if(!bOk) return Throw(eErrBufferShort);
			if(Packet.GetSizeLeft() < Len)
				return Throw(eErrInvalidName);
			const char* pName = (char*)Packet.ReadData(Len);
			if(!pName) return eErrInvalidName;

			CVariant Data(pMem);
			EResult Err = Data.FromPacket(&Packet, true);
			if(Err) return Throw(Err);

#ifndef VAR_NO_STL
			CVariant* pValue = &Container.Map[std::string(pName, Len)];
#else
			CVariant* pValue = Container.Map->AddValuePtr(FW::StringA(Container.Map->Allocator(), pName, Len), nullptr);
			if(!pValue)
				return Throw(eErrAllocFailed);
#endif
			pValue->Attach(Data.m_Variant);
		}
	}
	return Container.Map;
}

#ifndef VAR_NO_STL
FW::RetValue<std::vector<CVariant>*, CVariant::EResult> CVariant::SVariant::List() const
#else
FW::RetValue<FW::Array<CVariant>*, CVariant::EResult> CVariant::SVariant::List() const
#endif
{
	ASSERT(Type == VAR_TYPE_LIST);

	if (Access == eWriteOnly) 
		return Throw(eErrWriteInProgrees);
	if(Container.List == NULL)
	{
		CBuffer Packet(pMem, Payload, Size, true);
		((CVariant::SVariant*)this)->AllocContainer();
		for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Size; )
		{
			CVariant Data(pMem);
			EResult Err = Data.FromPacket(&Packet, true);
			if(Err) return Throw(Err);

#ifndef VAR_NO_STL
			Container.List->push_back(CVariant(pMem));
			CVariant* pValue = &Container.List.back();
#else
			if(!Container.List->Append(CVariant(pMem)))
				return Throw(eErrAllocFailed);
			CVariant* pValue = Container.List->LastPtr();
#endif
			pValue->Attach(Data.m_Variant);

		}
	}
	return Container.List;
}

#ifndef VAR_NO_STL
FW::RetValue<std::map<CVariant::SVariant::TIndex, CVariant>*, CVariant::EResult> CVariant::SVariant::IMap() const
#else
FW::RetValue<FW::Map<CVariant::SVariant::TIndex, CVariant>*, CVariant::EResult> CVariant::SVariant::IMap() const
#endif
{
	ASSERT(Type == VAR_TYPE_INDEX);

	if (Access == eWriteOnly) 
		return Throw(eErrWriteInProgrees);
	if(Container.Index == NULL)
	{
		CBuffer Packet(pMem, Payload, Size, true);
		((CVariant::SVariant*)this)->AllocContainer();
		for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Size; )
		{
			bool bOk;
			uint32 Id = Packet.ReadValue<uint32>(&bOk);
			if(!bOk) return Throw(eErrBufferShort);
			
			CVariant Data(pMem);
			EResult Err = Data.FromPacket(&Packet, true);
			if(Err) return Throw(Err);

#ifndef VAR_NO_STL
			CVariant* pValue = &Container.Index[TIndex{ Id }];
#else
			CVariant* pValue = Container.Index->AddValuePtr(TIndex{ Id }, nullptr);
			if(!pValue)
				return Throw(eErrAllocFailed);
#endif
			pValue->Attach(Data.m_Variant);
		}
	}
	return Container.Index;
}

CVariant::EResult CVariant::SVariant::MkPayload(CBuffer* pBuffer) const
{
	if(Access == eReadWrite) // write the packet
	{
		switch(Type)
		{
			case VAR_TYPE_MAP:
			{
				auto Ret = Map();
				if (Ret.Error) return Throw(Ret.Error);
				for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I)
				{
#ifndef VAR_NO_STL
					const std::string& sKey = I->first;
					ASSERT(sKey.length() < 0xFF);
					uint8 Len = (uint8)sKey.length();
					if (!pBuffer->WriteValue<uint8>(Len)) return Throw(eErrBufferWriteFailed);
					if (!pBuffer->WriteData(sKey.c_str(), Len)) return Throw(eErrBufferWriteFailed);

					EResult Err = I->second.ToPacket(&Buffer);
#else
					const FW::StringA& sKey = I.Key();
					ASSERT(sKey.Length() < 0xFF);
					uint8 Len = (uint8)sKey.Length();
					if (!pBuffer->WriteValue<uint8>(Len)) return Throw(eErrBufferWriteFailed);
					if (!pBuffer->WriteData(sKey.ConstData(), Len)) return Throw(eErrBufferWriteFailed);

					EResult Err = I.Value().ToPacket(pBuffer);
#endif
					if (Err) return Throw(Err);
				}
				return eErrNone;
			}
			case VAR_TYPE_LIST:
			{
				auto Ret = List();
				if (Ret.Error) return Throw(Ret.Error);
				for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I)
				{
					EResult Err = I->ToPacket(pBuffer);
					if (Err) return Throw(Err);
				}
				return eErrNone;
			}
			case VAR_TYPE_INDEX:
			{
				auto Ret = IMap();
				if (Ret.Error) return Throw(Ret.Error);
				for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I)
				{
#ifndef VAR_NO_STL
					if (!pBuffer->WriteValue<uint32>(I->first.u)) return Throw(eErrBufferWriteFailed);

					EResult Err = I->second.ToPacket(&Buffer);
#else
					if (!pBuffer->WriteValue<uint32>(I.Key().u)) return Throw(eErrBufferWriteFailed);

					EResult Err = I.Value().ToPacket(pBuffer);
#endif
					if (Err) return Throw(Err);
				}
				return eErrNone;
			}
		}
	}
	
	if (!pBuffer->SetBuffer(Payload, Size, true))
		return Throw(eErrBufferWriteFailed);
	return eErrNone; 
}


CVariant::EResult CVariant::SVariant::Clone(SVariant* pClone, bool Full) const
{
	pClone->Type = Type;
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			if (Container.Map == NULL) break;
			pClone->AllocContainer();
			auto Ret = Map();
			if (Ret.Error) return Throw(Ret.Error);
			for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I) {
				CVariant Value = Full ? I.Value().Clone() : I.Value();
#ifndef VAR_NO_STL
				pClone->Insert(I->first.c_str(), &Value); // todo pass err
#else
				pClone->Insert(I.Key().ConstData(), &Value); // todo pass err
#endif
			}
			return eErrNone;
		}
		case VAR_TYPE_LIST:
		{
			if (Container.List == NULL) break;
			pClone->AllocContainer();
			auto Ret = List();
			if (Ret.Error) return Throw(Ret.Error);
			for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I) {
				CVariant Value = Full ? I->Clone() : *I;
				pClone->Append(&Value); // todo pass err
			}
			return eErrNone;
		}
		case VAR_TYPE_INDEX:
		{
			if (Container.Index == NULL) break;
			pClone->AllocContainer();
			auto Ret = IMap();
			if (Ret.Error) return Throw(Ret.Error);
			for (auto I = Ret.Value->begin(); I != Ret.Value->end(); ++I) {
				CVariant Value = Full ? I.Value().Clone() : I.Value();
#ifndef VAR_NO_STL
				pClone->Insert(I->first.u, &Value); // todo pass err
#else
				pClone->Insert(I.Key().u, &Value); // todo pass err
#endif
		}
			return eErrNone;
		}
	}
	if(Size > 0)
	{
		pClone->Size = Size;
		if(!pClone->AllocPayload())
			return Throw(eErrAllocFailed);
		MemCopy(pClone->Payload, Payload, pClone->Size);
	}
	return eErrNone;
}

uint32 CVariant::SVariant::Count() const // Map, List or Index
{
	switch(Type)
	{
		case VAR_TYPE_MAP: {
			auto Ret = Map();
			if (Ret.Error) return 0;
#ifndef VAR_NO_STL
			return (uint32)Ret.Value->size();
#else
			return (uint32)Ret.Value->Count();
#endif
		}
		case VAR_TYPE_LIST: {
			auto Ret = List();
			if (Ret.Error) return 0;
#ifndef VAR_NO_STL
			return (uint32)Ret.Value->size();
#else
			return (uint32)Ret.Value->Count();
#endif
		}
		case VAR_TYPE_INDEX: {
			auto Ret = IMap();
			if (Ret.Error) return 0;
#ifndef VAR_NO_STL
			return (uint32)Ret.Value->size();
#else
			return (uint32)Ret.Value->Count();
#endif
		}
		default:		return 0;
	}
}

const char* CVariant::SVariant::Key(uint32 Index) const // Map
{
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			auto Ret = Map();
			if (Ret.Error) return NULL;
#ifndef VAR_NO_STL
			if (Index >= Ret.Value->size())
#else
			if (Index >= Ret.Value->Count())
#endif
				return NULL;

			auto I = Ret.Value->begin();
			while(Index--)
				++I;
#ifndef VAR_NO_STL
			return I->first.c_str();
#else
			return I.Key().ConstData();
#endif
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:
		default: return NULL;
	}
}

FW::RetValue<CVariant*, CVariant::EResult> CVariant::SVariant::Get(const char* Name) const // Map
{
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			auto Ret = Map();
			if (Ret.Error) return Ret.Error;
#ifndef VAR_NO_STL
			std::string sName;
			sName.assign(Name/*, Len != -1 ? Len : strlen(Name)*/);
			std::map<std::string, CVariant>::iterator I = Ret.Value->find(sName);
			if (I == Ret.Value->end())
				return NULL; //I = Ret.Value->insert(std::map<std::string, CVariant>::value_type(sName, CVariant(pMem))).first;
			return &I->second;
#else
			FW::StringA NameA(Ret.Value->Allocator(), Name);
			CVariant* pValue = Ret.Value->GetValuePtr(NameA);
			//if (!pValue) {
			//	pValue = Ret.Value->SetValuePtr(NameA, NULL).first;
			//	if(!pValue) return Throw(eErrAllocFailed);
			//}
			return pValue;
#endif
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:
		default: return eErrTypeMismatch;
	}
}

bool CVariant::SVariant::Has(const char* Name) const // Map
{
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			auto Ret = Map();
			if (Ret.Error) return false;
#ifndef VAR_NO_STL
			return Ret.Value->find(Name) != Ret.Value->end();
#else
			return Ret.Value->find(FW::StringA(Ret.Value->Allocator(), Name)) != Ret.Value->end();
#endif
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:
		default: return false;
	}
}

FW::RetValue<CVariant*, CVariant::EResult> CVariant::SVariant::Insert(const char* Name, const CVariant* pVariant, bool bUnsafe) // Map
{
	switch(Type)
	{
	case VAR_TYPE_MAP:
	{
		auto Ret = Map();
		if (Ret.Error) return Ret.Error;
#ifndef VAR_NO_STL
		CVariant* pValue = &Ret.Value->insert[Name];
		*Value = pVariant ? *pVariant : CVariant(pMem);
#else
		CVariant* pValue; // We use eMulti as we already know that there is no duplicate
		if(bUnsafe) pValue = Ret.Value->AddValuePtr(FW::StringA(Ret.Value->Allocator(), Name), pVariant);
		else        pValue = Ret.Value->SetValuePtr(FW::StringA(Ret.Value->Allocator(), Name), pVariant).first;
		if(!pValue) return Throw(eErrAllocFailed);
#endif
		return pValue;
	}
	case VAR_TYPE_LIST:
	case VAR_TYPE_INDEX:
	default: return eErrTypeMismatch;
	}
}

CVariant::EResult CVariant::SVariant::Remove(const char* Name) // Map
{
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			auto Ret = Map();
			if (Ret.Error) return Ret.Error;
#ifndef VAR_NO_STL
			std::map<std::string, CVariant>::iterator I = Ret.Value->find(Name);
#else
			auto I = Ret.Value->find(FW::StringA(Ret.Value->Allocator(), Name));
#endif
			if(I != Ret.Value->end())
				Ret.Value->erase(I);
			return eErrNone;
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:
		default: return Throw(eErrTypeMismatch);
	}
}

uint32 CVariant::SVariant::Id(uint32 Index) const // Index
{
	switch(Type)
	{
	case VAR_TYPE_INDEX:
	{
		auto Ret = IMap();
		if (Ret.Error) return Ret.Error;
#ifndef VAR_NO_STL
		if (Index >= Ret.Value->size())
#else
		if (Index >= Ret.Value->Count())
#endif
			return 0xFFFFFFFF;

		auto I = Ret.Value->begin();
		while(Index--)
			++I;
#ifndef VAR_NO_STL
		return I->first.u;
#else
		return I.Key().u;
#endif
	}
	case VAR_TYPE_LIST:
	case VAR_TYPE_MAP:
	default: return Throw(eErrTypeMismatch);
	}
}

FW::RetValue<CVariant*, CVariant::EResult> CVariant::SVariant::Get(uint32 Index) const // List or Index
{
	switch(Type)
	{
	case VAR_TYPE_LIST:
	{
		auto Ret = List();
		if (Ret.Error) return Ret.Error;
#ifndef VAR_NO_STL
		if(Index >= Ret.Value->size())
#else
		if(Index >= Ret.Value->Count())
#endif
			return Throw(eErrIndexOutOfBounds);
		return &(*Ret.Value)[Index];
	}
	case VAR_TYPE_INDEX:
	{
		auto Ret = IMap();
		if (Ret.Error) return Ret.Error;
#ifndef VAR_NO_STL
		std::map<TIndex, CVariant>::iterator I = Ret.Value->find(TIndex{ Index });
		if (I == Ret.Value->end())
			return NULL; // I = Ret.Value->insert(std::map<TIndex, CVariant>::value_type(TIndex{ Index }, CVariant())).first;
		return &I->second;
#else
		CVariant* pValue = Ret.Value->GetValuePtr(TIndex{ Index });
		//if (!pValue) {
		//	pValue = Ret.Value->SetValuePtr(TIndex{ Index }, NULL).first;
		//	if(!pValue) return Throw(eErrAllocFailed);
		//}
		return pValue;
#endif
	}
	case VAR_TYPE_MAP:
	default: return eErrTypeMismatch;
	}
}

bool CVariant::SVariant::Has(uint32 Index) const // List or Index
{
	switch(Type)
	{
	case VAR_TYPE_INDEX:
	{
		auto Ret = IMap();
		if (Ret.Error) return false;
		return Ret.Value->find(TIndex{ Index }) != Ret.Value->end();
	}
	case VAR_TYPE_LIST:
	{
		auto Ret = List();
		if (Ret.Error) return false;
#ifndef VAR_NO_STL
		return Index < Ret.Value->size();
#else
		return Index < Ret.Value->Count();
#endif
	}
	case VAR_TYPE_MAP:
	default: return false;
	}
}

CVariant::EResult CVariant::SVariant::Append(const CVariant* pVariant) // List
{
	switch(Type)
	{
		case VAR_TYPE_LIST:
		{
			auto Ret = List();
			if (Ret.Error) return Ret.Error;
#ifndef VAR_NO_STL
			Ret.Value->push_back(pVariant ? *pVariant : CVariant(pMem));
#else
			if(!Ret.Value->Append(pVariant ? *pVariant : CVariant(pMem)))
				return Throw(eErrAllocFailed);
#endif
			return eErrNone;
		}
		case VAR_TYPE_MAP:
		case VAR_TYPE_INDEX:
		default: return Throw(eErrTypeMismatch);
	}
}

FW::RetValue<CVariant*, CVariant::EResult> CVariant::SVariant::Insert(uint32 Index, const CVariant* pVariant, bool bUnsafe) // Index
{
	switch(Type)
	{
	case VAR_TYPE_INDEX:
	{
		auto Ret = IMap();
		if (Ret.Error) return Ret.Error;
#ifndef VAR_NO_STL
		CVariant* pValue = &Ret.Value->insert[TIndex{ Index }];
		*Value = pVariant ? *pVariant : CVariant(pMem);
#else
		CVariant* pValue; // We use eMulti as we already know that there is no duplicate
		if(bUnsafe) pValue = Ret.Value->AddValuePtr(TIndex{ Index }, pVariant);
		else pValue = Ret.Value->SetValuePtr(TIndex{ Index }, pVariant).first;
		if(!pValue) return Throw(eErrAllocFailed);
#endif
		return pValue;
	}
	case VAR_TYPE_LIST:
	case VAR_TYPE_MAP:
	default: return Throw(eErrTypeMismatch);
	}
}

CVariant::EResult CVariant::SVariant::Remove(uint32 Index) // List and Index
{
	switch(Type)
	{
		case VAR_TYPE_LIST:
		{
			auto Ret = List();
			if (Ret.Error) return Ret.Error;
#ifndef VAR_NO_STL
			if(Index >= Ret.Value->size())
#else
			if(Index >= Ret.Value->Count())
#endif
				return Throw(eErrIndexOutOfBounds);
			auto I = Ret.Value->begin();
			I += Index;
			Ret.Value->erase(I);
			return eErrNone;
		}
		case VAR_TYPE_INDEX:
		{
			auto Ret = IMap();
			if (Ret.Error) return Ret.Error;
			auto I = Ret.Value->find(TIndex{ Index });
			if(I != Ret.Value->end())
				Ret.Value->erase(I);
			return eErrNone;
		}
		case VAR_TYPE_MAP:
		default: return Throw(eErrTypeMismatch);
	}
}



//////////////////////////////////////////////////////////////////////////////////////////////
// Variant Read/Write

CVariant::EResult CVariant::BeginWrite(EType Type)
{
	Clear();

	if(!InitValue(VAR_TYPE_EMPTY, 0, NULL)) 
		return eErrAllocFailed;
	m_Variant->Access = eWriteOnly;
	m_Variant->Type = Type;
	m_Variant->AllocContainer();
	if (!m_Variant->Container.Buffer) return Throw(eErrAllocFailed);
	return eErrNone;
}

CVariant::EResult CVariant::Finish()
{
	if (m_Variant->Access != eWriteOnly) 
		return Throw(eErrWriteNotReady);

	m_Variant->Size = (uint32)m_Variant->Container.Buffer->GetSize();
	m_Variant->Payload = m_Variant->Container.Buffer->GetBuffer(true);
	m_Variant->FreeContainer();
	m_Variant->Access = eReadOnly;
	return eErrNone;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Map Read/Write

CVariant::EResult CVariant::ReadRawMap(void(*cb)(const SVarName& Name, const CVariant& Data, void* Param), void* Param) const
{
	auto pVal = Val(true);
	if (pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
		return eErrNone;
	if (pVal.Value->Type != VAR_TYPE_MAP) 
		return Throw(eErrTypeMismatch);

	if (pVal.Value->Container.Map) { // in case this variant has already been parsed
		for (auto I = pVal.Value->Container.Map->begin(); I != pVal.Value->Container.Map->end(); ++I)
#ifndef VAR_NO_STL
			cb(SVarName{ I->first.c_str(), I->first.length() }, I->second, Param);
#else
			cb(SVarName{ I.Key().ConstData(), I.Key().Length() }, I.Value(), Param);
#endif
		return eErrNone;
	}

	CBuffer Packet(m_pMem, pVal.Value->Payload, pVal.Value->Size, true);
	for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < pVal.Value->Size; )
	{
		bool bOk;
		uint8 Len = Packet.ReadValue<uint8>(&bOk);
		if(!bOk) return Throw(eErrBufferShort);
		if(Packet.GetSizeLeft() < Len)
			return Throw(eErrInvalidName);

		//std::string Name = std::string((char*)Packet.ReadData(Len), Len);
		SVarName Name;
		Name.Buf = (char*)Packet.GetData(Len);
		if(!Name.Buf) return Throw(eErrInvalidName);
		Name.Len = Len;

		CVariant Data(m_pMem);
		EResult Err = Data.FromPacket(&Packet, true);
		if(Err) return Err;

		cb(Name, Data, Param);
	}
	return eErrNone;
}

CVariant::EResult CVariant::Find(const char* sName, CVariant& Data) const
{
	auto pVal = Val(true);
	if (pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
		return eErrNone;
	if (pVal.Value->Type != VAR_TYPE_MAP) 
		return Throw(eErrTypeMismatch);

	if (pVal.Value->Container.Map) { // in case this variant has already been parsed
		auto Ret = PtrAt(sName);
		if (Ret.Error) 
			return eErrNotFound;
		Data = *Ret.Value;
		return eErrNone;
	}

	size_t uLen = strlen(sName);

	CBuffer Packet(m_pMem, pVal.Value->Payload, pVal.Value->Size, true);
	for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < pVal.Value->Size; )
	{
		bool bOk;
		uint8 Len = Packet.ReadValue<uint8>(&bOk);
		if(!bOk) return Throw(eErrBufferShort);
		if(Packet.GetSizeLeft() < Len)
			return Throw(eErrInvalidName);

		SVarName Name;
		Name.Buf = (char*)Packet.ReadData(Len);
		if(!Name.Buf) return eErrInvalidName;
		Name.Len = Len;
		if (Name.Len == uLen && MemCmp(sName, Name.Buf, Name.Len) == 0) 
			return Data.FromPacket(&Packet, true);

		uint32 Size = ReadHeader(&Packet);
		if(Size == 0xFFFFFFFF) return Throw(eErrInvalidHeader);
		if (!Packet.ReadData(Size)) return Throw(eErrBufferShort);
	}
	return eErrNotFound;
}

CVariant::EResult CVariant::WriteValue(const char* Name, EType Type, size_t Size, const void* Value)
{
	if (m_Variant->Access != eWriteOnly)
		return eErrWriteNotReady;
	if (m_Variant->Type != VAR_TYPE_MAP)
		return Throw(eErrTypeMismatch);

	ASSERT(strlen(Name) < 0xFF);
	uint8 Len = (uint8)strlen(Name);
	if (!m_Variant->Container.Buffer->WriteValue<uint8>(Len)) return Throw(eErrBufferWriteFailed);
	if (!m_Variant->Container.Buffer->WriteData(Name, Len)) return Throw(eErrBufferWriteFailed);

	return ToPacket(m_Variant->Container.Buffer, Type, Size, Value);
}

CVariant::EResult CVariant::WriteVariant(const char* Name, const CVariant& Variant)
{
	if (m_Variant->Access != eWriteOnly) 
		return eErrWriteNotReady;
	if (m_Variant->Type != VAR_TYPE_MAP)
		return Throw(eErrTypeMismatch);

	ASSERT(strlen(Name) < 0xFF);
	uint8 Len = (uint8)strlen(Name);
	if (!m_Variant->Container.Buffer->WriteValue<uint8>(Len)) return Throw(eErrBufferWriteFailed);
	if (!m_Variant->Container.Buffer->WriteData(Name, Len)) return Throw(eErrBufferWriteFailed);

	return Variant.ToPacket(m_Variant->Container.Buffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// List Read/Write

CVariant::EResult CVariant::ReadRawList(void(*cb)(const CVariant& Data, void* Param), void* Param) const
{
	auto pVal = Val(true);
	if (pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
		return eErrNone;
	if (pVal.Value->Type != VAR_TYPE_LIST) 
		return Throw(eErrTypeMismatch);

	if (pVal.Value->Container.List) { // in case this variant has already been parsed
		for (auto I = pVal.Value->Container.List->begin(); I != pVal.Value->Container.List->end(); ++I)
			cb(*I, Param);
		return eErrNone;
	}

	CBuffer Packet(m_pMem, pVal.Value->Payload, pVal.Value->Size, true);
	for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < pVal.Value->Size; )
	{
		CVariant Data(m_pMem);
		EResult Err = Data.FromPacket(&Packet, true);
		if(Err) return Err;

		cb(Data, Param);
	}
	return eErrNone;
}

CVariant::EResult CVariant::WriteValue(EType Type, size_t Size, const void* Value)
{
	if (m_Variant->Access != eWriteOnly)
		return eErrWriteNotReady;
	if (m_Variant->Type != VAR_TYPE_LIST)
		return Throw(eErrTypeMismatch);

	return ToPacket(m_Variant->Container.Buffer, Type, Size, Value);
}
	
CVariant::EResult CVariant::WriteVariant(const CVariant& Variant)
{
	if (m_Variant->Access != eWriteOnly) 
		return eErrWriteNotReady;
	if (m_Variant->Type != VAR_TYPE_LIST)
		return Throw(eErrTypeMismatch);

	return Variant.ToPacket(m_Variant->Container.Buffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IMap Read/Write

CVariant::EResult CVariant::ReadRawIMap(void(*cb)(uint32 Index, const CVariant& Data, void* Param), void* Param) const
{
	auto pVal = Val(true);
	if (pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
		return eErrNone;
	if (pVal.Value->Type != VAR_TYPE_INDEX) 
		return Throw(eErrTypeMismatch);

	if (pVal.Value->Container.Index) { // in case this variant has already been parsed
		for (auto I = pVal.Value->Container.Index->begin(); I != pVal.Value->Container.Index->end(); ++I)
#ifndef VAR_NO_STL
			cb(I->first.u, I->second, Param);
#else
			cb(I.Key().u, I.Value(), Param);
#endif
		return eErrNone;
	}

	CBuffer Packet(m_pMem, pVal.Value->Payload, pVal.Value->Size, true);
	for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < pVal.Value->Size; )
	{
		bool bOk;
		uint32 Index = Packet.ReadValue<uint32>(&bOk);
		if(!bOk) return Throw(eErrBufferShort);
		
		CVariant Data(m_pMem);
		EResult Err = Data.FromPacket(&Packet, true);
		if(Err) return Err;

		cb(Index, Data, Param);
	}
	return eErrNone;
}

CVariant::EResult CVariant::Find(uint32 uIndex, CVariant& Data) const
{
	auto pVal = Val(true);
	if (pVal.Error == eErrIsEmpty || pVal.Value->Type == VAR_TYPE_EMPTY)
		return eErrNone;
	if (pVal.Value->Type != VAR_TYPE_INDEX) 
		return Throw(eErrTypeMismatch);

	if (pVal.Value->Container.Index) { // in case this variant has already been parsed
		auto Ret = PtrAt(uIndex);
		if (Ret.Error) 
			return eErrNotFound;
		Data = *Ret.Value;
	}

	CBuffer Packet(m_pMem, pVal.Value->Payload, pVal.Value->Size, true);
	for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < pVal.Value->Size; )
	{
		bool bOk;
		uint32 Index = Packet.ReadValue<uint32>(&bOk);
		if(!bOk) return eErrBufferShort;

		if (Index == uIndex) 
			return Data.FromPacket(&Packet, true);

		uint32 Size = ReadHeader(&Packet);
		if(Size == 0xFFFFFFFF) return Throw(eErrInvalidHeader);
		if (!Packet.ReadData(Size)) return Throw(eErrBufferShort);
	}
	return eErrNotFound;
}

CVariant::EResult CVariant::WriteValue(uint32 Index, EType Type, size_t Size, const void* Value)
{
	if (m_Variant->Access != eWriteOnly) 
		return eErrWriteNotReady;
	if (m_Variant->Type != VAR_TYPE_INDEX)
		return Throw(eErrTypeMismatch);

	if(!m_Variant->Container.Buffer->WriteValue<uint32>(Index)) return Throw(eErrBufferWriteFailed);

	return ToPacket(m_Variant->Container.Buffer, Type, Size, Value);
}

CVariant::EResult CVariant::WriteVariant(uint32 Index, const CVariant& Variant)
{
	if (m_Variant->Access != eWriteOnly) 
		return eErrWriteNotReady;
	if (m_Variant->Type != VAR_TYPE_INDEX)
		return Throw(eErrTypeMismatch);

	if(!m_Variant->Container.Buffer->WriteValue<uint32>(Index)) return Throw(eErrBufferWriteFailed);

	return Variant.ToPacket(m_Variant->Container.Buffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// 

#ifndef VAR_NO_STL_STR
void WritePacket(const std::string& Name, const CVariant& Packet, CBuffer& Buffer)
{
	ASSERT(Name.length() < 0xFF);
	uint8 Len = (uint8)Name.length();
	ASSERT(Len < 0x7F);
	Buffer.WriteValue<uint8>(Len);							// [uint8 - NameLen]
	Buffer.WriteData((byte*)Name.c_str(), Name.length());	// [bytes - Name]
	Packet.ToPacket(&Buffer);								// [uint8 Type][uint32/uint16/uint8 - PaylaodLen][bytes - Paylaod]
}

void ReadPacket(const CBuffer& Buffer, std::string& Name, CVariant& Packet)
{
	uint8 Len = *((uint8*)Buffer.ReadData(sizeof(uint8)));
	Len &= 0x7F;
	char* pName = (char*)Buffer.ReadData(Len);
	Name = std::string(pName, Len);
	Packet.FromPacket(&Buffer);
}

//bool StreamPacket(CBuffer& Buffer, std::string& Name, CVariant& Packet)
//{
//	// [uint8 - NameLen][bytes - Name][uint8 Type][uint32/uint16/uint8 - PaylaodLen][bytes - Paylaod]([uint32 - CRC CheckSumm])
//
//	Buffer.SetPosition(0);
//
//	uint8 Len = *((uint8*)Buffer.GetData(sizeof(uint8)));
//	Len &= 0x7F;
//	char* pName = (char*)Buffer.GetData(Len);
//	if(!pName)
//		return false; // incomplete header
//
//	uint8* pType = ((uint8*)Buffer.GetData(sizeof(uint8) + Len, sizeof(uint8)));
//	if(pType == NULL)
//		return false; // incomplete header
//
//	void* pSize; // = (uint32*)Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint32));
//	if((*pType & CVariant::ELen32) == CVariant::ELen8)
//		pSize = Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint8));
//	else if((*pType & CVariant::ELen32) == CVariant::ELen16)
//		pSize = Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint16));
//	else
//		pSize = Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint32));
//
//	if(pSize == NULL)
//		return false; // incomplete header
//
//	size_t Size; // = sizeof(uint8) + sizeof(uint32) + *pSize;
//	if((*pType & CVariant::ELen32) == CVariant::ELen8)
//		Size = sizeof(uint8) + sizeof(uint8) + *((uint8*)pSize);
//	else if((*pType & CVariant::ELen32) == CVariant::ELen16)
//		Size = sizeof(uint8) + sizeof(uint16) + *((uint16*)pSize);
//	else
//		Size = sizeof(uint8) + sizeof(uint32) + *((uint32*)pSize);
//
//	if(Buffer.GetSizeLeft() < Size)
//		return false; // incomplete data
//
//	Name = std::string(pName, Len);
//	Packet.FromPacket(&Buffer);
//	Buffer.ShiftData(sizeof(uint8) + Len + Size);
//
//	return true;
//}
#endif