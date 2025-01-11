#include "pch.h"
#include "ProgramRule.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include <QUuid>

CProgramRule::CProgramRule(QObject* parent)
	: CGenericRule(parent)
{
}

CProgramRule::CProgramRule(const CProgramID& ID, QObject* parent)
	: CGenericRule(ID, parent)
{
}

CProgramRule* CProgramRule::Clone() const
{
    CProgramRule* pRule = new CProgramRule(m_ProgramID);
	CopyTo(pRule);

	pRule->m_Type = m_Type;
	pRule->m_ProgramPath = m_ProgramPath;
	pRule->m_Enclave = m_Enclave;
	pRule->m_SignatureLevel = m_SignatureLevel;
	pRule->m_Data = m_Data;

	return pRule;
}

QString CProgramRule::GetTypeStr() const 
{
	switch (m_Type)
	{
	case EExecRuleType::eAllow:		return tr("Allow");
	case EExecRuleType::eBlock:		return tr("Block");
	case EExecRuleType::eProtect:	return tr("Protect");
	case EExecRuleType::eIsolate:	return tr("Isolate");
	case EExecRuleType::eAudit:		return tr("Audit");
		// todo other:
	}
	return "Unknown";
}