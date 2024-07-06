#include "pch.h"
#include "AccessRule.h"
#include "../../Library/API/PrivacyAPI.h"

CAccessRule::CAccessRule(const CProgramID& ID)
	: CGenericRule(ID)
{

}

CAccessRule::~CAccessRule()
{

}

std::shared_ptr<CAccessRule> CAccessRule::Clone() const
{
	std::shared_ptr<CAccessRule> pRule = std::make_shared<CAccessRule>(m_ProgramID);
	CopyTo(pRule.get());

	std::shared_lock Lock(m_Mutex); 
	pRule->m_Type = m_Type;
	pRule->m_AccessPath = m_AccessPath;
	pRule->m_ProgramPath = m_ProgramPath;

	return pRule;
}

void CAccessRule::Update(const std::shared_ptr<CAccessRule>& Rule)
{
	CGenericRule::Update(Rule);

	std::unique_lock Lock(m_Mutex); 
	m_Type = Rule->m_Type;
	m_AccessPath = Rule->m_AccessPath;
	m_ProgramPath = Rule->m_ProgramPath;
}