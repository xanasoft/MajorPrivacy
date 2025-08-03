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
