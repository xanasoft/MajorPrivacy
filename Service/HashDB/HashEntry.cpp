#include "pch.h"
#include "HashEntry.h"
#include "../Library/API/PrivacyAPI.h"

CHash::CHash()
{
}

CHash::~CHash()
{
}

void CHash::Update(const std::shared_ptr<CHash>& pHash)
{
	std::unique_lock Lock(m_Mutex);
	std::shared_lock SrcLock(pHash->m_Mutex);

	m_Name = pHash->m_Name;
	m_Description = pHash->m_Description;
	m_bEnabled = pHash->m_bEnabled;
	m_bTemporary = pHash->m_bTemporary;
	m_Type = pHash->m_Type;
	m_Hash = pHash->m_Hash;

	m_Enclaves = pHash->m_Enclaves;
	m_Collections = pHash->m_Collections;

	m_Data = pHash->m_Data;
}

StVariant CHash::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	std::shared_lock Lock(m_Mutex);

	StVariantWriter Enclave(pMemPool);
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Enclave.BeginIndex();
		WriteIVariant(Enclave, Opts);
	} else {  
		Enclave.BeginMap();
		WriteMVariant(Enclave, Opts);
	}
	return Enclave.Finish();
}

NTSTATUS CHash::FromVariant(const StVariant& Enclave)
{
	std::unique_lock Lock(m_Mutex);

	if (Enclave.GetType() == VAR_TYPE_MAP)         FW::CVariantReader(Enclave).ReadRawMap([&](const SVarName& Name, const CVariant& Data)   { ReadMValue(Name, Data); });
	else if (Enclave.GetType() == VAR_TYPE_INDEX)  FW::CVariantReader(Enclave).ReadRawIndex([&](uint32 Index, const CVariant& Data)         { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;
	return STATUS_SUCCESS;
}
