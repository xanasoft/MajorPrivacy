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
		// todo other:
	}
	return "Unknown";
}

QString CProgramRule::GetSignatureLevelStr(KPH_VERIFY_AUTHORITY SignAuthority)
{
	switch (SignAuthority) {
	case KphDevAuthority:	return tr("Developer Signed");			// Part of Majror Privacy
	case KphUserAuthority:	return tr("User Signed");				// Signed by the User Himself
	case KphMsAuthority:	return tr("Microsoft");					// Signed by Microsoft
	case KphAvAuthority:	return tr("Microsoft/AV");				// Signed by Microsoft or Antivirus
	case KphMsCodeAuthority:return tr("Microsoft Trusted");			// Signed by Microsoft or Code Root
	case KphUnkAuthority:	return tr("Unknown Root");
	case KphNoAuthority:	return tr("None");
	case KphUntestedAuthority: return tr("Undetermined");
	}
	return "Unknown";
}

QString CProgramRule::GetOnSpawnStr(EProgramOnSpawn OnSpawn)
{
	switch (OnSpawn) {
	case EProgramOnSpawn::eAllow:	return tr("Allow");
	case EProgramOnSpawn::eBlock:	return tr("Block");
	case EProgramOnSpawn::eEject:	return tr("Eject");
	}
	return "Unknown";
}