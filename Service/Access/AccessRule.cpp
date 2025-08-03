#include "pch.h"
#include "AccessRule.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../ServiceCore.h"

CAccessRule::CAccessRule(const CProgramID& ID)
	: CGenericRule(ID)
{
	m_ProgramPath = ID.GetFilePath();
}

CAccessRule::~CAccessRule()
{

}

std::shared_ptr<CAccessRule> CAccessRule::Clone(bool CloneGuid) const
{
	std::shared_ptr<CAccessRule> pRule = std::make_shared<CAccessRule>(m_ProgramID);
	CopyTo(pRule.get(), CloneGuid);

	std::shared_lock Lock(m_Mutex); 
	pRule->m_Type = m_Type;
	pRule->m_bUseScript = m_bUseScript;
	pRule->m_Script = m_Script;
	pRule->m_bInteractive = m_bInteractive;
	pRule->m_AccessPath = m_AccessPath;
	pRule->m_ProgramPath = m_ProgramPath;
	pRule->UpdateScript();

	return pRule;
}

void CAccessRule::Update(const std::shared_ptr<CAccessRule>& Rule)
{
	CGenericRule::Update(Rule);

	std::unique_lock Lock(m_Mutex); 
	m_Type = Rule->m_Type;
	m_bUseScript = Rule->m_bUseScript;
	m_Script = Rule->m_Script;
	m_bInteractive = Rule->m_bInteractive;
	m_AccessPath = Rule->m_AccessPath;
	m_ProgramPath = Rule->m_ProgramPath;
	UpdateScript();
}

NTSTATUS CAccessRule::FromVariant(const StVariant& Rule)
{
	NTSTATUS status = CGenericRule::FromVariant(Rule);
	UpdateScript();
	return status;
}

void CAccessRule::UpdateScript()
{
	m_pScript.reset();

	if (!m_pScript.get() && !m_Script.empty()) 
	{
		m_pScript = std::make_shared<CJSEngine>(m_Script);
		m_pScript->RunScript();
	}
}

bool CAccessRule::HasScript() const
{
	std::shared_lock Lock(m_Mutex);

	return m_bUseScript && !!m_pScript.get();
}

EAccessRuleType CAccessRule::RunScript(const std::wstring& NtPath, uint64 ActorPid, const std::wstring& ActorServiceTag, uint32 AccessMask) const
{
	StVariant Event;
	Event["NtPath"] = NtPath;
	Event["DosPath"] = theCore->NormalizePath(NtPath);
	Event["ActorPid"] = ActorPid;
	Event["ActorServiceTag"] = ActorServiceTag;
	Event["AccessMask"] = AccessMask;

	auto Ret = m_pScript->CallFunc("TestAccess", 1, Event);
	if(Ret.IsError())
		return EAccessRuleType::eNone;
	EAccessRuleType Action = (EAccessRuleType)Ret.GetValue().To<int>();
	return Action;
}