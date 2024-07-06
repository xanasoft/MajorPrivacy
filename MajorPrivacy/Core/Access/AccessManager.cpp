#include "pch.h"
#include "AccessManager.h"
#include "../PrivacyCore.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Common/XVariant.h"
#include "./MiscHelpers/Common/Common.h"
#include "../Programs/ProgramManager.h"

CAccessManager::CAccessManager(QObject* parent)
	: QObject(parent)
{

}

STATUS CAccessManager::Update()
{
	return OK;
}

/////////////////////////////////////////////////////////////////////////////
// Rules

bool CAccessManager::UpdateAllAccessRules()
{
	auto Ret = theCore->GetAllAccessRules();
	if (Ret.IsError())
		return false;

	XVariant& Rules = Ret.GetValue();

	QMap<QString, CAccessRulePtr> OldRules = m_AccessRules;

	for (int i = 0; i < Rules.Count(); i++)
	{
		const XVariant& Rule = Rules[i];

		QString Guid = Rule[API_V_RULE_GUID].AsQStr();

		QString ProgramPath = QString::fromStdWString(NormalizeFilePath(Rule[API_V_FILE_PATH].AsStr()));

		CProgramID ID;
		ID.SetPath(ProgramPath);

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

	foreach(const QString& Guid, OldRules.keys())
		RemoveAccessRule(OldRules.take(Guid));

	return true;
}

bool CAccessManager::UpdateAccessRule(const QString& RuleId)
{
	auto Ret = theCore->GetAccessRule(RuleId);
	if (Ret.IsError())
		return false;

	XVariant& Rule = Ret.GetValue();

	QString RuleID = Rule[API_V_RULE_GUID].AsQStr();

	QString ProgramPath = QString::fromStdWString(NormalizeFilePath(Rule[API_V_FILE_PATH].AsStr()));

	CProgramID ID;
	ID.SetPath(ProgramPath);

	CAccessRulePtr pRule = m_AccessRules.value(RuleID);
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

void CAccessManager::RemoveAccessRule(const QString& RuleId)
{
	CAccessRulePtr pRule = m_AccessRules.take(RuleId);
	if(pRule)
		RemoveAccessRule(pRule);
}

QSet<QString> CAccessManager::GetAccessRuleIDs() const
{
	return ListToSet(m_AccessRules.keys()); 
}

QList<CAccessRulePtr> CAccessManager::GetAccessRules(const QSet<QString>& AccessRuleIDs)
{
	QList<CAccessRulePtr> List;
	foreach(const QString& RuleName, AccessRuleIDs) {
		CAccessRulePtr pAccessRulePtr = m_AccessRules.value(RuleName);
		if (pAccessRulePtr) List.append(pAccessRulePtr);
	}
	return List;
}

STATUS CAccessManager::SetAccessRule(const CAccessRulePtr& pRule)
{
	return theCore->SetAccessRule(pRule->ToVariant(SVarWriteOpt()));
}

RESULT(CAccessRulePtr) CAccessManager::GetAccessRule(QString Guid)
{
	auto Ret = theCore->GetAccessRule(Guid);
	if (Ret.IsError())
		return Ret;

	XVariant& Rule = Ret.GetValue();

	QString ProgramPath = QString::fromStdWString(NormalizeFilePath(Rule[API_V_FILE_PATH].AsStr()));

	CProgramID ID;
	ID.SetPath(ProgramPath);
	
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