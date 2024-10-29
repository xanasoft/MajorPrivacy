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
		
	pRule->m_Name = m_Name + tr(" (duplicate)");
	//pRule->m_Grouping = m_Grouping;
	pRule->m_Description = m_Description;

	pRule->m_bEnabled = m_bEnabled;

	//pRule->m_ProgramID = m_ProgramID;
}

XVariant CGenericRule::ToVariant(const SVarWriteOpt& Opts) const
{
	XVariant Rule;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Rule.BeginIMap();
		WriteIVariant(Rule, Opts);
	} else {  
		Rule.BeginMap();
		WriteMVariant(Rule, Opts);
	}
	Rule.Finish();
	return Rule;
}

void CGenericRule::FromVariant(const class XVariant& Rule)
{
	if (Rule.GetType() == VAR_TYPE_MAP)			Rule.ReadRawMap([&](const SVarName& Name, const CVariant& Data)	{ ReadMValue(Name, Data); });
	else if (Rule.GetType() == VAR_TYPE_INDEX)  Rule.ReadRawIMap([&](uint32 Index, const CVariant& Data)		{ ReadIValue(Index, Data); });
	// todo err
}
