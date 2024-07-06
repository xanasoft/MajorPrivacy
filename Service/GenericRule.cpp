#include "pch.h"
#include "GenericRule.h"
#include "../Library/API/PrivacyAPI.h"
#include <Rpc.h>

CGenericRule::CGenericRule(const CProgramID& ID)
{
    m_ProgramID = ID;
}

void CGenericRule::CopyTo(CGenericRule* Rule) const
{
    std::shared_lock Lock(m_Mutex); 

    GUID myGuid;
    RPC_STATUS status = UuidCreate(&myGuid);
    if (status == RPC_S_OK || status == RPC_S_UUID_LOCAL_ONLY) {
        RPC_WSTR guidString = NULL;
        status = UuidToStringW(&myGuid, &guidString);
        if (status == RPC_S_OK) {
            Rule->m_Guid = std::wstring((wchar_t*)guidString, wcslen((wchar_t*)guidString));
            RpcStringFreeW(&guidString);
        }
    }

	Rule->m_Name = m_Name;

	Rule->m_bEnabled = m_bEnabled;
	Rule->m_bTemporary = m_bTemporary;

	Rule->m_Data = m_Data.Clone();
}

void CGenericRule::Update(const std::shared_ptr<CGenericRule>& Rule)
{
    std::unique_lock Lock(m_Mutex);

	m_Guid = Rule->m_Guid;
	m_Name = Rule->m_Name;

	m_bEnabled = Rule->m_bEnabled;
    m_bTemporary = Rule->m_bTemporary;

    m_Data = Rule->m_Data;
}

CVariant CGenericRule::ToVariant(const SVarWriteOpt& Opts) const
{
    std::shared_lock Lock(m_Mutex);

    CVariant Rule;
    if (Opts.Format == SVarWriteOpt::eIndex) {
        Rule.BeginIMap();
        WriteIVariant(Rule, Opts);
    } else {  
        Rule.BeginMap();
        WriteMVariant(Rule, Opts);
    }
    Rule.Finish();
	return Rule;
}

NTSTATUS CGenericRule::FromVariant(const class CVariant& Rule)
{
    std::unique_lock Lock(m_Mutex);

    if (Rule.GetType() == VAR_TYPE_MAP)         Rule.ReadRawMap([&](const SVarName& Name, const CVariant& Data) { ReadMValue(Name, Data); });
    else if (Rule.GetType() == VAR_TYPE_INDEX)  Rule.ReadRawIMap([&](uint32 Index, const CVariant& Data)        { ReadIValue(Index, Data); });
    else
        return STATUS_UNKNOWN_REVISION;
    return STATUS_SUCCESS;
}
