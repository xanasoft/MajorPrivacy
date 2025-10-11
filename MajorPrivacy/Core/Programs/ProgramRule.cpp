#include "pch.h"
#include "ProgramRule.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../PrivacyCore.h"
#include <QUuid>

CProgramRule::CProgramRule(QObject* parent)
	: CGenericRule(parent)
{
}

CProgramRule::CProgramRule(const CProgramID& ID, QObject* parent)
	: CGenericRule(ID, parent)
{
}

bool CProgramRule::IsUnsafePath(const QString& Path)
{
	QString SystemVolume = theCore->GetWinDir();
	if (Path.length() < SystemVolume.length() || SystemVolume.compare(Path.left(SystemVolume.length()), Qt::CaseInsensitive) != 0)
		return false;

	// we are on C:\

	size_t uPos = SystemVolume.length();
	QString SubPath = GetNextPath(Path, uPos);
	if (SubPath.isEmpty())
		return true; // Volume without sub path

	//DbgPrintEx(DPFLTR_DEFAULT_ID, 0xFFFFFFFF, "BAM TEST: %S\n", SubPath.ConstData());

	if (SubPath.compare("Windows", Qt::CaseInsensitive) == 0)
	{
		if(WildEnd(Path, uPos)) return true; // no file or sub folder

		SubPath = GetNextPath(Path, uPos);

		if (SubPath.compare("System32", Qt::CaseInsensitive) == 0 || SubPath.compare("SysWOW64", Qt::CaseInsensitive) == 0)
		{
			if(WildEnd(Path, uPos)) return true; // no file or sub folder
		}

		if (SubPath.compare("SystemApps", Qt::CaseInsensitive) == 0)
		{
			if(WildEnd(Path, uPos)) return true; // no file or sub folder
		}
	}

	return false;
}

CProgramRule* CProgramRule::Clone() const
{
    CProgramRule* pRule = new CProgramRule(m_ProgramID);
	CopyTo(pRule);

	pRule->m_Type = m_Type;
	pRule->m_ProgramPath = m_ProgramPath;
	pRule->m_Enclave = m_Enclave;

	pRule->m_AllowedSignatures = m_AllowedSignatures;
	pRule->m_AllowedCollections = m_AllowedCollections;

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