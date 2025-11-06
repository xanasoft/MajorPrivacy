//#include "pch.h"

#include "../Core/Header.h"
#include "VariantRW.h"

#ifndef NO_CRT
#include <memory>
#endif

FW_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////////
// Read

bool CVariantReader::Begin()
{
	if(m) return false;
	
	if (m_Variant.m_Type == VAR_TYPE_EMPTY)
		return false;

	if (!m_Variant.m_bContainer)
		m = new SRawReader((CVariant*)&m_Variant);
	else
	{
		switch (m_Variant.GetType())
		{
		case VAR_TYPE_MAP:		m = new SMapReader((CVariant*)&m_Variant); break;
		case VAR_TYPE_LIST:		m = new SListReader((CVariant*)&m_Variant); break;
		case VAR_TYPE_INDEX:	m = new SIndexReader((CVariant*)&m_Variant); break;
		default: return false;
		}
	}
	return true;
}

bool CVariantReader::SRawReader::Load()
{
	if (Buffer.GetSizeLeft() == 0)
		return false;
	
	switch (pVariant->m_Type)
	{
		case VAR_TYPE_MAP:
		{
			bool bOk;
			uint8 Len = Buffer.ReadValue<uint8>(&bOk);
			if(!bOk) return false;
			if(Buffer.GetSizeLeft() < Len) return false;

			Name.Buf = (char*)Buffer.ReadData(Len);
			if(!Name.Buf) false;
			Name.Len = Len;
			break;
		}
		case VAR_TYPE_LIST:
			break;
		case VAR_TYPE_INDEX:
		{
			bool bOk;
			Index = Buffer.ReadValue<uint32>(&bOk);
			if(!bOk) return false;
			break;
		}
		default: // not a container type
			return false;
	}

	EResult Err = Value.FromPacket(&Buffer, true);
	if(Err) return false;
	return true;
}

bool CVariantReader::SMapReader::Load()
{
	if (AtEnd())
		return false;

	Name.Buf = I.Key().ConstData();
	Name.Len = I.Key().Length();
	Value = I.Value();
	return true;
}

bool CVariantReader::SListReader::Load()
{
	if (AtEnd())
		return false;

	Index++;
	Value = ((CVariant*)pVariant->m_p.List->Data)[i];
	return true;
}

bool CVariantReader::SIndexReader::Load()
{
	if (AtEnd())
		return false;

	Index = I.Key().u;
	Value = I.Value();
	return true;
}

void CVariantReader::End()
{
	delete m;
	m = nullptr;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Read

CVariant::EResult CVariantReader::ReadRawMap(const CVariant& Variant, void(*cb)(const SVarName& Name, const CVariant& Data, void* Param), void* Param)
{
	if (Variant.m_Type == VAR_TYPE_EMPTY)
		return CVariant::eErrNone;
	if (Variant.m_Type != VAR_TYPE_MAP) 
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	if (Variant.m_bContainer) { // in case this variant has already been parsed
		for (auto I = CVariant::MapBegin(Variant.m_p.Map); I != CVariant::MapEnd(); ++I)
			cb(SVarName{ I.Key().ConstData(), I.Key().Length() }, I.Value(), Param);
		return CVariant::eErrNone;
	}

	CBuffer Packet(Variant.m_pMem, Variant.GetData(), Variant.GetSize(), true);
	while (Packet.GetSizeLeft() > 0)
	{
		bool bOk;
		uint8 Len = Packet.ReadValue<uint8>(&bOk);
		if(!bOk) return CVariant::Throw(CVariant::eErrBufferShort);
		if(Packet.GetSizeLeft() < Len)
			return CVariant::Throw(CVariant::eErrInvalidName);

		SVarName Name;
		Name.Buf = (char*)Packet.ReadData(Len);
		if(!Name.Buf) return CVariant::Throw(CVariant::eErrInvalidName);
		Name.Len = Len;

		CVariant Data(Variant.m_pMem);
		EResult Err = Data.FromPacket(&Packet, true);
		if(Err) return Err;

		cb(Name, Data, Param);
	}
	return CVariant::eErrNone;
}

CVariant::EResult CVariantReader::Find(const CVariant& Variant, const char* sName, CVariant& Data)
{
	if (Variant.m_Type == VAR_TYPE_EMPTY)
		return CVariant::eErrNone;
	if (Variant.m_Type != VAR_TYPE_MAP) 
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	if (Variant.m_bContainer) { // in case this variant has already been parsed
		auto Ret = Variant.PtrAt(sName);
		if (Ret.Error || !Ret.Value) 
			return CVariant::eErrNotFound;
		Data = *Ret.Value;
		return CVariant::eErrNone;
	}

	size_t uLen = strlen(sName);

	CBuffer Packet(Variant.m_pMem, Variant.GetData(), Variant.GetSize(), true);
	while (Packet.GetSizeLeft() > 0)
	{
		bool bOk;
		uint8 Len = Packet.ReadValue<uint8>(&bOk);
		if(!bOk) return CVariant::Throw(CVariant::eErrBufferShort);
		if(Packet.GetSizeLeft() < Len)
			return CVariant::Throw(CVariant::eErrInvalidName);

		SVarName Name;
		Name.Buf = (char*)Packet.ReadData(Len);
		if(!Name.Buf) return CVariant::eErrInvalidName;
		Name.Len = Len;
		if (Name.Len == uLen && MemCmp(sName, Name.Buf, Name.Len) == 0) 
			return Data.FromPacket(&Packet, true);

		uint32 Size = Variant.ReadHeader(&Packet);
		if(Size == 0xFFFFFFFF) return CVariant::Throw(CVariant::eErrInvalidHeader);
		if (!Packet.ReadData(Size)) return CVariant::Throw(CVariant::eErrBufferShort);
	}
	return CVariant::eErrNotFound;
}

CVariant::EResult CVariantReader::ReadRawList(const CVariant& Variant, void(*cb)(const CVariant& Data, void* Param), void* Param)
{
	if (Variant.m_Type == VAR_TYPE_EMPTY)
		return CVariant::eErrNone;
	if (Variant.m_Type != VAR_TYPE_LIST) 
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	if (Variant.m_bContainer) { // in case this variant has already been parsed
		for (uint32 i = 0; i < Variant.m_p.List->Count; i++)
			cb(((CVariant*)Variant.m_p.List->Data)[i], Param);
		return CVariant::eErrNone;
	}

	CBuffer Packet(Variant.m_pMem, Variant.GetData(), Variant.GetSize(), true);
	while (Packet.GetSizeLeft() > 0)
	{
		CVariant Data(Variant.m_pMem);
		EResult Err = Data.FromPacket(&Packet, true);
		if(Err) return Err;

		cb(Data, Param);
	}
	return CVariant::eErrNone;
}

CVariant::EResult CVariantReader::ReadRawIndex(const CVariant& Variant, void(*cb)(uint32 Index, const CVariant& Data, void* Param), void* Param)
{
	if (Variant.m_Type == VAR_TYPE_EMPTY)
		return CVariant::eErrNone;
	if (Variant.m_Type != VAR_TYPE_INDEX) 
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	if (Variant.m_bContainer) { // in case this variant has already been parsed
		for (auto I = CVariant::IndexBegin(Variant.m_p.Index); I != CVariant::IndexEnd(); ++I)
			cb(I.Key().u, I.Value(), Param);
		return CVariant::eErrNone;
	}

	CBuffer Packet(Variant.m_pMem, Variant.GetData(), Variant.GetSize(), true);
	while (Packet.GetSizeLeft() > 0)
	{
		bool bOk;
		uint32 Index = Packet.ReadValue<uint32>(&bOk);
		if(!bOk) return CVariant::Throw(CVariant::eErrBufferShort);

		CVariant Data(Variant.m_pMem);
		EResult Err = Data.FromPacket(&Packet, true);
		if(Err) return Err;

		cb(Index, Data, Param);
	}
	return CVariant::eErrNone;
}

CVariant::EResult CVariantReader::Find(const CVariant& Variant, uint32 uIndex, CVariant& Data)
{
	if (Variant.m_Type == VAR_TYPE_EMPTY)
		return CVariant::eErrNone;
	if (Variant.m_Type != VAR_TYPE_INDEX) 
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	if (Variant.m_bContainer) { // in case this variant has already been parsed
		auto Ret = Variant.PtrAt(uIndex);
		if (Ret.Error || !Ret.Value) 
			return CVariant::eErrNotFound;
		Data = *Ret.Value;
	}

	CBuffer Packet(Variant.m_pMem, Variant.GetData(), Variant.GetSize(), true);
	while (Packet.GetSizeLeft() > 0)
	{
		bool bOk;
		uint32 Index = Packet.ReadValue<uint32>(&bOk);
		if(!bOk) return CVariant::eErrBufferShort;

		if (Index == uIndex) 
			return Data.FromPacket(&Packet, true);

		uint32 Size = Variant.ReadHeader(&Packet);
		if(Size == 0xFFFFFFFF) return CVariant::Throw(CVariant::eErrInvalidHeader);
		if (!Packet.ReadData(Size)) return CVariant::Throw(CVariant::eErrBufferShort);
	}
	return CVariant::eErrNotFound;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Write

CVariant::EResult CVariantWriter::BeginWrite(EType Type)
{
	m_Buffer.AllocBuffer(128, false, false, true);
	m_Type = Type;
	return CVariant::eErrNone;
}

CVariant::EResult CVariantWriter::WriteValue(const char* Name, EType Type, size_t Size, const void* Value)
{
	if (m_Type != VAR_TYPE_MAP)
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	ASSERT(strlen(Name) < 0xFF);
	uint8 Len = (uint8)strlen(Name);
	if (!m_Buffer.WriteValue<uint8>(Len)) return CVariant::Throw(CVariant::eErrBufferWriteFailed);
	if (!m_Buffer.WriteData(Name, Len)) return CVariant::Throw(CVariant::eErrBufferWriteFailed);

	return CVariant::ToPacket(&m_Buffer, Type, Size, Value);
}

CVariant::EResult CVariantWriter::WriteVariant(const char* Name, const CVariant& Variant)
{
	if (m_Type != VAR_TYPE_MAP)
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	ASSERT(strlen(Name) < 0xFF);
	uint8 Len = (uint8)strlen(Name);
	if (!m_Buffer.WriteValue<uint8>(Len)) return CVariant::Throw(CVariant::eErrBufferWriteFailed);
	if (!m_Buffer.WriteData(Name, Len)) return CVariant::Throw(CVariant::eErrBufferWriteFailed);

	return Variant.ToPacket(&m_Buffer);
}

CVariant::EResult CVariantWriter::WriteValue(EType Type, size_t Size, const void* Value)
{
	if (m_Type != VAR_TYPE_LIST)
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	return CVariant::ToPacket(&m_Buffer, Type, Size, Value);
}

CVariant::EResult CVariantWriter::WriteVariant(const CVariant& Variant)
{
	if (m_Type != VAR_TYPE_LIST)
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	return Variant.ToPacket(&m_Buffer);
}

CVariant::EResult CVariantWriter::WriteValue(uint32 Index, EType Type, size_t Size, const void* Value)
{
	if (m_Type != VAR_TYPE_INDEX)
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	if(!m_Buffer.WriteValue<uint32>(Index)) return CVariant::Throw(CVariant::eErrBufferWriteFailed);

	return CVariant::ToPacket(&m_Buffer, Type, Size, Value);
}

CVariant::EResult CVariantWriter::WriteVariant(uint32 Index, const CVariant& Variant)
{
	if (m_Type != VAR_TYPE_INDEX)
		return CVariant::Throw(CVariant::eErrTypeMismatch);

	if(!m_Buffer.WriteValue<uint32>(Index)) return CVariant::Throw(CVariant::eErrBufferWriteFailed);

	return Variant.ToPacket(&m_Buffer);
}

CVariant CVariantWriter::Finish()
{
	CVariant Variant(m_Buffer.Allocator());

	if (m_Type == VAR_TYPE_EMPTY) {
		CVariant::Throw(CVariant::eErrWriteNotReady);
		return Variant;
	}
	
	uint32 uSize = (uint32)m_Buffer.GetSize();
	CVariant::EResult Err = Variant.InitValue(m_Type, uSize, m_Buffer.GetBuffer(true), true);
	if (Err != CVariant::eErrNone) {
		CVariant::Throw(Err);
		return Variant;
	}
	Variant.m_bReadOnly = true;
	return Variant;
}

FW_NAMESPACE_END