#include "pch.h"
#include "GenericRule.h"
#include "../../Library/API/PrivacyAPI.h"
#include "./Library/Helpers/NtUtil.h"

CGenericRule::CGenericRule(QObject* parent)
	: QObject(parent)
{
}

CGenericRule::CGenericRule(const CProgramID& ID, QObject* parent)
	: QObject(parent)
{
	m_ProgramID = ID;
}

bool CGenericRule::IsPathValid(QString FilePath)
{
	FilePath = FilePath.toLower();

	if (FilePath.length() >= 7 && FilePath.mid(0, 4) == "\\??\\" && FilePath[5] == ':' && FilePath[6] == '\\')  // \??\X:\...
		FilePath = FilePath.mid(4);
	else if (FilePath.length() > 4 && FilePath.mid(0, 4) == "\\\\.\\") // \\.\... alias for \Device\...
		FilePath = "\\Device\\" + FilePath.mid(4);

	if (FilePath.length() >= 3 && (FilePath[0] >= 'a' && FilePath[0] <= 'z' && FilePath[1] == ':' && FilePath[2] == '\\')) { // X:\... Dos Path
		return true;
	}
	else if (FilePath.length() > 8 && FilePath.mid(0, 8) == "\\device\\") { // \Device\... Nt Device Path
		size_t uWCPos = FilePath.indexOf('*', 8);
		if(uWCPos == -1)
			return true; // no wild card
		else {
			size_t uBSPos = FilePath.indexOf('\\', 8);
			if(uBSPos != -1 && uWCPos > uBSPos)
				return true; // wildcard but after the backslash
		}
	}
	else if (FilePath.length() > 12 && FilePath.mid(0, 12) == "\\systemroot\\") { // WinDir
		return true;
	}
	else if (FilePath.length() >= 3 && (FilePath[0] == '\\' && FilePath[1] == '\\')) { // \\... Network Path
		return true;
	}

	return false;
}

QString CGenericRule::GetNextPath(const QString& Path, size_t& uPos)
{
	if (uPos == -1)
		return QString();
	size_t uStart = uPos + 1; // skip \

	uPos = Path.indexOf('\\', uStart);
	QString SubPath = Path.mid(uStart, uPos == -1 ? -1 : (uPos - uStart));

	size_t uAPos = SubPath.indexOf('*');
	size_t uQPos = SubPath.indexOf('?');

	if (uAPos == -1 && uQPos == -1)
		return SubPath;

	return SubPath.mid(0, uQPos < uAPos ? uQPos : uAPos);
}

bool CGenericRule::WildEnd(const QString& Path, size_t uPos)
{
	if(uPos == -1)
		return true;
	// uPos pointrs to '\\'
	if(Path.at(uPos + 1) == '*')
		return true;
	return false;
}

void CGenericRule::CopyTo(CGenericRule* pRule, bool CloneGuid) const
{
	if(CloneGuid)
		pRule->m_Guid = m_Guid;
	else
		pRule->m_Guid = QUuid::createUuid().toString();
		
	pRule->m_Name = m_Name;
	if(!CloneGuid)
		pRule->m_Name += tr(" (duplicate)");
	//pRule->m_Grouping = m_Grouping;
	pRule->m_Description = m_Description;

	pRule->m_bEnabled = m_bEnabled;

	//pRule->m_ProgramID = m_ProgramID;
}

bool CGenericRule::ValidateUserSID()
{
	if(m_SidValid)
		return true;

	if(m_User.isEmpty())
	{
		m_UserSid.Clear();
		m_SidValid = true;
		return true;
	}

	BYTE sidBuffer[SECURITY_MAX_SID_SIZE] = { 0 };
	DWORD sidSize = sizeof(sidBuffer);
	SID_NAME_USE sidType;
	WCHAR domain[256];
	DWORD domainSize = _countof(domain);

	QtVariant SID;
	if (LookupAccountNameW(nullptr, (wchar_t*)m_User.utf16(), sidBuffer, &sidSize, domain, &domainSize, &sidType))
	{
		CBuffer SidBuff;
		SidBuff.AllocBuffer(72);
		SidBuff.WriteData(sidBuffer, sizeof(sidBuffer));
		SidBuff.WriteData(L"\0\0\0\0", 4);
		ASSERT(SidBuff.GetSize() == 72);
		m_UserSid = SidBuff;
	}

	m_SidValid = true;
	return true;
}

QtVariant CGenericRule::ToVariant(const SVarWriteOpt& Opts) const
{
	QtVariantWriter Rule;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Rule.BeginIndex();
		WriteIVariant(Rule, Opts);
	} else {  
		Rule.BeginMap();
		WriteMVariant(Rule, Opts);
	}
	return Rule.Finish();
}

void CGenericRule::FromVariant(const class QtVariant& Rule)
{
	if (Rule.GetType() == VAR_TYPE_MAP)			QtVariantReader(Rule).ReadRawMap([&](const SVarName& Name, const QtVariant& Data)	{ ReadMValue(Name, Data); });
	else if (Rule.GetType() == VAR_TYPE_INDEX)  QtVariantReader(Rule).ReadRawIndex([&](uint32 Index, const QtVariant& Data)		{ ReadIValue(Index, Data); });
	// todo err
}
