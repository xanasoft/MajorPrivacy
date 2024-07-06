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
	if(!m_ProgramPath.isEmpty()) 
		m_ProgramPath = QString::fromStdWString(NtPathToDosPath(m_ProgramPath.toStdWString()));
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
	}
	return "Unknown";
}

