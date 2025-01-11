#include "pch.h"
#include "AccessRule.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"

CAccessRule::CAccessRule(QObject* parent)
	: CGenericRule(parent)
{
}

CAccessRule::CAccessRule(const CProgramID& ID, QObject* parent)
	: CGenericRule(ID, parent)
{
	m_ProgramPath = ID.GetFilePath();
}

bool CAccessRule::IsHidden() const
{
	return !m_Data.Get(API_V_RULE_REF_GUID).AsQStr().isEmpty();
}

CAccessRule* CAccessRule::Clone() const
{
    CAccessRule* pRule = new CAccessRule(m_ProgramID);
	CopyTo(pRule);

	pRule->m_Type = m_Type;
	pRule->m_AccessPath = m_AccessPath;
	pRule->m_ProgramPath = m_ProgramPath;
	pRule->m_Data = m_Data;

	return pRule;
}

QString CAccessRule::GetTypeStr() const 
{
	switch (m_Type)
	{
	case EAccessRuleType::eAllow:		return tr("Allow");
	case EAccessRuleType::eAllowRO:		return tr("Allow Read-Only");
	case EAccessRuleType::eEnum:		return tr("Allow Listing");
	case EAccessRuleType::eBlock:		return tr("Block");
	case EAccessRuleType::eProtect:		return tr("Protect");
	case EAccessRuleType::eIgnore:		return tr("Don't Log");
	}
	return "Unknown";
}

