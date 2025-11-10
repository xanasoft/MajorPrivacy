#include "pch.h"
#include "Enclave.h"
#include "../Library/API/PrivacyAPI.h"
#include "../ServiceCore.h"

CEnclave::CEnclave()
{
}

CEnclave::~CEnclave()
{
}

void CEnclave::Update(const std::shared_ptr<CEnclave>& pEnclave)
{
	std::unique_lock Lock(m_Mutex);

	m_bEnabled = pEnclave->m_bEnabled;
	m_Name = pEnclave->m_Name;
	m_Description = pEnclave->m_Description;

	m_VolumeGuid = pEnclave->m_VolumeGuid;

	m_bUseScript = pEnclave->m_bUseScript;
	m_Script = pEnclave->m_Script;
	UpdateScript_NoLock();

	m_AllowedSignatures = pEnclave->m_AllowedSignatures;
	m_AllowedCollections = pEnclave->m_AllowedCollections;
	m_ImageLoadProtection = pEnclave->m_ImageLoadProtection;
	m_ImageCoherencyChecking = pEnclave->m_ImageCoherencyChecking;

	m_OnTrustedSpawn = pEnclave->m_OnTrustedSpawn;
	m_OnSpawn = pEnclave->m_OnSpawn;

	m_IntegrityLevel = pEnclave->m_IntegrityLevel;
	m_AllowDebugging = pEnclave->m_AllowDebugging;
	m_KeepAlive = pEnclave->m_KeepAlive;
}

StVariant CEnclave::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
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

NTSTATUS CEnclave::FromVariant(const StVariant& Enclave)
{
	std::unique_lock Lock(m_Mutex);

    if (Enclave.GetType() == VAR_TYPE_MAP)         FW::CVariantReader(Enclave).ReadRawMap([&](const SVarName& Name, const CVariant& Data)   { ReadMValue(Name, Data); });
    else if (Enclave.GetType() == VAR_TYPE_INDEX)  FW::CVariantReader(Enclave).ReadRawIndex([&](uint32 Index, const CVariant& Data)         { ReadIValue(Index, Data); });
    else
        return STATUS_UNKNOWN_REVISION;
	UpdateScript_NoLock();
    return STATUS_SUCCESS;
}

void CEnclave::UpdateScript_NoLock()
{
	m_pScript.reset();

	if (!m_Script.empty()) 
	{
		m_pScript = std::make_shared<CJSEngine>(m_Script, m_Guid, EItemType::eEnclave);
		m_pScript->RunScript();
	}
}

bool CEnclave::HasScript() const
{
	std::shared_lock Lock(m_Mutex);

	return m_bUseScript && !!m_pScript.get();
}

CJSEnginePtr CEnclave::GetScriptEngine() const 
{ 
	std::shared_lock Lock(m_Mutex); 

	return m_pScript; 
}

