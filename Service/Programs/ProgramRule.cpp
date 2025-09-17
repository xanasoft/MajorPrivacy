#include "pch.h"
#include "ProgramRule.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../ServiceCore.h"

CProgramRule::CProgramRule(const CProgramID& ID)
	: CGenericRule(ID)
{
	m_ProgramPath = ID.GetFilePath();
}

CProgramRule::~CProgramRule()
{

}

std::shared_ptr<CProgramRule> CProgramRule::Clone(bool CloneGuid) const
{
	std::shared_ptr<CProgramRule> pRule = std::make_shared<CProgramRule>(m_ProgramID);
	CopyTo(pRule.get(), CloneGuid);

	std::shared_lock Lock(m_Mutex); 

	pRule->m_Type = m_Type;
	pRule->m_ProgramPath = m_ProgramPath;

	pRule->m_bUseScript = m_bUseScript;
	pRule->m_Script = m_Script;
	pRule->UpdateScript_NoLock();

	pRule->m_AllowedSignatures.Value = m_AllowedSignatures.Value;
	pRule->m_AllowedCollections = m_AllowedCollections;
	pRule->m_ImageLoadProtection = m_ImageLoadProtection;

	return pRule;
}

void CProgramRule::Update(const std::shared_ptr<CProgramRule>& Rule)
{
	CGenericRule::Update(Rule);

	std::unique_lock Lock(m_Mutex); 

	m_Type = Rule->m_Type;
	m_ProgramPath = Rule->m_ProgramPath;

	m_bUseScript = Rule->m_bUseScript;
	m_Script = Rule->m_Script;
	UpdateScript_NoLock();

	m_AllowedSignatures = Rule->m_AllowedSignatures;
	m_AllowedCollections = Rule->m_AllowedCollections;
	m_ImageLoadProtection = Rule->m_ImageLoadProtection;
}

NTSTATUS CProgramRule::FromVariant(const StVariant& Rule)
{
	NTSTATUS status = CGenericRule::FromVariant(Rule);
	std::unique_lock Lock(m_Mutex);
	UpdateScript_NoLock();
	return status;
}

void CProgramRule::UpdateScript_NoLock()
{
	m_pScript.reset();

	if (!m_Script.empty()) 
	{
		m_pScript = std::make_shared<CJSEngine>(m_Script, m_Guid, EScriptTypes::eExecRule);
		m_pScript->RunScript();
	}
}

bool CProgramRule::HasScript() const
{
	std::shared_lock Lock(m_Mutex);

	return m_bUseScript && !!m_pScript.get();
}

CJSEnginePtr CProgramRule::GetScriptEngine() const 
{ 
	std::shared_lock Lock(m_Mutex); 

	return m_pScript; 
}

