#include "pch.h"
#include "GenericRule.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/MiscHelpers.h"
#include "../Library/Helpers/NtUtil.h"

CGenericRule::CGenericRule(const CProgramID& ID)
{
    m_ProgramID = ID;
}

bool CGenericRule::IsExpired() const
{
    std::shared_lock Lock(m_Mutex); 
    if(m_uTimeOut == -1)
        return false;
    uint64 CurrentTime = FILETIME2time(GetCurrentTimeAsFileTime());
    if (m_uTimeOut < CurrentTime) {
//#ifdef _DEBUG
//        DbgPrint("Rule %S has expired\r\n", m_Name.c_str());
//#endif
        return true;
    }
//#ifdef _DEBUG
//    DbgPrint("Rule %S will expire in %llu seconds\r\n", m_Name.c_str(), m_uTimeOut - CurrentTime);
//#endif
    return false;
}

void CGenericRule::CopyTo(CGenericRule* Rule, bool CloneGuid) const
{
    std::shared_lock Lock(m_Mutex); 

    if(CloneGuid)
        Rule->m_Guid = m_Guid;
	else
		Rule->m_Guid.FromWString(MkGuid());
    Rule->m_bEnabled = m_bEnabled;

    Rule->m_Enclave = m_Enclave;
	
	Rule->m_bTemporary = m_bTemporary;

    Rule->m_Name = m_Name;
    //Rule->m_Grouping = v;
	Rule->m_Description = m_Description;

	Rule->m_Data = m_Data.Clone();
}

void CGenericRule::Update(const std::shared_ptr<CGenericRule>& Rule)
{
    std::unique_lock Lock(m_Mutex);

	m_Guid = Rule->m_Guid;
	m_bEnabled = Rule->m_bEnabled;

	m_Enclave = Rule->m_Enclave;

    m_bTemporary = Rule->m_bTemporary;

	m_Name = Rule->m_Name;
	//m_Grouping = Rule->m_Grouping;
	m_Description = Rule->m_Description;

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
