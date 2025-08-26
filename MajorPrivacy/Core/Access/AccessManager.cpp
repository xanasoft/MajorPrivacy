#include "pch.h"
#include "AccessManager.h"
#include "../PrivacyCore.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "./Common/QtVariant.h"
#include "./MiscHelpers/Common/Common.h"
#include "../Programs/ProgramManager.h"

CAccessManager::CAccessManager(QObject* parent)
	: QObject(parent)
{

}

STATUS CAccessManager::Update()
{
	//////////////////////////////////////////////////////////
	// WARING This is called from a differnt thread
	//////////////////////////////////////////////////////////


	return OK;
}

/////////////////////////////////////////////////////////////////////////////
// Rules

bool CAccessManager::UpdateAllAccessRules()
{
	auto Ret = theCore->GetAllAccessRules();
	if (Ret.IsError())
		return false;

	QtVariant& Rules = Ret.GetValue();

	QMap<QFlexGuid, CAccessRulePtr> OldRules = m_AccessRules;

	for (int i = 0; i < Rules.Count(); i++)
	{
		const QtVariant& Rule = Rules[i];

		QFlexGuid Guid;
		Guid.FromVariant(Rule[API_V_GUID]);

		QString ProgramPath = Rule[API_V_FILE_PATH].AsQStr();
		CProgramID ID;
		ID.SetPath(theCore->NormalizePath(ProgramPath, true));

		CAccessRulePtr pRule = OldRules.value(Guid);
		if (pRule) {
			if(ID == pRule->GetProgramID())
				OldRules.remove(Guid);
			else // on ID change compeltely re add the rule as if it would be new
			{
				RemoveAccessRule(pRule);
				pRule.clear();
			}
		}

		bool bAdd = false;
		if (!pRule) {
			pRule = CAccessRulePtr(new CAccessRule(ID));
			bAdd = true;
		} 

		pRule->FromVariant(Rule);

		if(bAdd)
			AddAccessRule(pRule);
	}

	foreach(const QFlexGuid& Guid, OldRules.keys())
		RemoveAccessRule(OldRules.take(Guid));

	return true;
}

bool CAccessManager::UpdateAccessRule(const QFlexGuid& Guid)
{
	auto Ret = theCore->GetAccessRule(Guid);
	if (Ret.IsError())
		return false;

	QtVariant& Rule = Ret.GetValue();

	QFlexGuid Guid2;
	Guid2.FromVariant(Rule[API_V_GUID]);
	if (Guid2 != Guid)
		return false;

	QString ProgramPath = Rule[API_V_FILE_PATH].AsQStr();
	CProgramID ID;
	ID.SetPath(theCore->NormalizePath(ProgramPath, true));

	CAccessRulePtr pRule = m_AccessRules.value(Guid);
	if (pRule) {
		if(ID != pRule->GetProgramID()) // on ID change compeltely re add the rule as if it would be new
		{
			RemoveAccessRule(pRule);
			pRule.clear();
		}
	}

	bool bAdd = false;
	if (!pRule) {
		pRule = CAccessRulePtr(new CAccessRule(ID));
		bAdd = true;
	} 

	pRule->FromVariant(Rule);

	if(bAdd)
		AddAccessRule(pRule);

	return true;
}

void CAccessManager::RemoveAccessRule(const QFlexGuid& Guid)
{
	CAccessRulePtr pRule = m_AccessRules.take(Guid);
	if(pRule)
		RemoveAccessRule(pRule);
}

QSet<QFlexGuid> CAccessManager::GetAccessRuleIDs() const
{
	return ListToSet(m_AccessRules.keys()); 
}

QList<CAccessRulePtr> CAccessManager::GetAccessRules(const QSet<QFlexGuid>& AccessRuleIDs)
{
	QList<CAccessRulePtr> List;
	foreach(const QFlexGuid& Guid, AccessRuleIDs) {
		CAccessRulePtr pAccessRulePtr = m_AccessRules.value(Guid);
		if (pAccessRulePtr) List.append(pAccessRulePtr);
	}
	return List;
}

STATUS CAccessManager::SetAccessRule(const CAccessRulePtr& pRule)
{
	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

	if (!pRule->ValidateUserSID())
		return ERR(STATUS_INVALID_PARAMETER, tr("Invalid User Name in access rule").toStdWString());

	return theCore->SetAccessRule(pRule->ToVariant(Opts));
}

RESULT(CAccessRulePtr) CAccessManager::GetAccessRule(const QFlexGuid& Guid)
{
	auto Ret = theCore->GetAccessRule(Guid);
	if (Ret.IsError())
		return Ret;

	QtVariant& Rule = Ret.GetValue();

	QString ProgramPath = Rule[API_V_FILE_PATH].AsQStr();
	CProgramID ID;
	ID.SetPath(theCore->NormalizePath(ProgramPath, true));
	
	CAccessRulePtr pRule = CAccessRulePtr(new CAccessRule(ID));
	pRule->FromVariant(Rule);

	RETURN(pRule);
}

STATUS CAccessManager::DelAccessRule(const CAccessRulePtr& pRule)
{
	STATUS Status = theCore->DelAccessRule(pRule->GetGuid());
	if(Status)
		RemoveAccessRule(pRule);
	return Status;
}

void CAccessManager::AddAccessRule(const CAccessRulePtr& pRule)
{
	m_AccessRules.insert(pRule->GetGuid(), pRule);
}

void CAccessManager::RemoveAccessRule(const CAccessRulePtr& pRule)
{
	m_AccessRules.remove(pRule->GetGuid());
}