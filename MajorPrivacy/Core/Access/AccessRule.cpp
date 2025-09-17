#include "pch.h"
#include "AccessRule.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../PrivacyCore.h"

CAccessRule::CAccessRule(QObject* parent)
	: CGenericRule(parent)
{
}

CAccessRule::CAccessRule(const CProgramID& ID, QObject* parent)
	: CGenericRule(ID, parent)
{
	m_ProgramPath = ID.GetFilePath();
}

bool CAccessRule::IsUnsafePath(const QString& Path)
{
	QString SystemVolume = theCore->GetWinDir();
	if (Path.length() < SystemVolume.length() || SystemVolume.compare(Path.left(SystemVolume.length()), Qt::CaseInsensitive) == 0)
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

			if (SubPath.compare("config", Qt::CaseInsensitive) == 0)
				return true;
		}

		if (SubPath.compare("SystemApps", Qt::CaseInsensitive) == 0)
		{
			if(WildEnd(Path, uPos)) return true; // no file or sub folder
		}

		if (SubPath.compare("WinSxS", Qt::CaseInsensitive) == 0)
		{
			if(WildEnd(Path, uPos)) return true; // no file or sub folder
		}
	}
	else if (SubPath.compare("Users", Qt::CaseInsensitive) == 0)
	{
		if(WildEnd(Path, uPos)) return true; // no user name

		SubPath = GetNextPath(Path, uPos); // user name 
		if (WildEnd(Path, uPos))
			return true; // no followup path

		SubPath = GetNextPath(Path, uPos);
		if (SubPath.left(10).toUpper() == "NTUSER.DAT" && WildEnd(Path, uPos))
			return true; // User registry hive

		//if (SubPath.compare("AppData", Qt::CaseInsensitive) != 0)
		//	return false; // other paths are ok
		//if(WildEnd(Path, uPos))
		//	return true; // app data only
		//
		//SubPath = GetNextPath(Path, uPos);
		//
		//if (SubPath.compare("Local", Qt::CaseInsensitive) != 0 && SubPath.compare("LocalLow", Qt::CaseInsensitive) != 0 && SubPath.compare("Roaming", Qt::CaseInsensitive) != 0)
		//	return false; // none of the thress
		//if(WildEnd(Path, uPos))
		//	return true; // no follow up path
	}
	else if (SubPath.compare("Program Files", Qt::CaseInsensitive) == 0 || SubPath.compare("Program Files (x86)", Qt::CaseInsensitive) == 0)
	{
		if (WildEnd(Path, uPos)) return true; // no user name
	}

	return false;
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

bool CAccessRule::IsNotMounted() const
{
	return m_AccessNtPath.isEmpty();
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

