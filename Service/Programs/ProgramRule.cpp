#include "pch.h"
#include "ProgramRule.h"
#include "../../Library/API/PrivacyAPI.h"

CProgramRule::CProgramRule(const CProgramID& ID)
	: CGenericRule(ID)
{

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
	pRule->m_SignatureLevel = m_SignatureLevel;
	pRule->m_OnTrustedSpawn = m_OnTrustedSpawn;
	pRule->m_OnSpawn = m_OnSpawn;
	pRule->m_ImageLoadProtection = m_ImageLoadProtection;

	return pRule;
}

void CProgramRule::Update(const std::shared_ptr<CProgramRule>& Rule)
{
	CGenericRule::Update(Rule);

	std::unique_lock Lock(m_Mutex); 
	m_Type = Rule->m_Type;
	m_ProgramPath = Rule->m_ProgramPath;
	m_SignatureLevel = Rule->m_SignatureLevel;
	m_OnTrustedSpawn = Rule->m_OnTrustedSpawn;
	m_OnSpawn = Rule->m_OnSpawn;
	m_ImageLoadProtection = Rule->m_ImageLoadProtection;
}

