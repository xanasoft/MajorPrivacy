#include "pch.h"
#include "IssueManager.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"

/////////////////////////////////////////////////////////////////////////////
// CIssueManager
//

CIssueManager::CIssueManager(QObject* parent)
	: QObject(parent)
{

}

void CIssueManager::DetectIssues()
{
	QMap<QString, CIssuePtr> Old = m_Issues;

	//
	// Status
	//

	if (theCore->Driver()->IsConnected())
	{
		auto Ret = theCore->Driver()->GetUserKey();
		if (Ret.IsError())
		{
			QString Id = "no_user_key";
			if (!Old.take(Id))
			{
				CIssuePtr pIssue(new CCustomIssue(CCustomIssue::eNoUserKey));
				AddIssueImpl(Id, pIssue);
			}
		}
	}

	//
	// Firewall rules
	//

	auto FwRules = theCore->NetworkManager()->GetFwRules();

	QMap<QFlexGuid, CFwRulePtr> UnapprovedRules;
	QMap<QFlexGuid, CFwRulePtr> AlteredRules;
	foreach(auto pFwRule, FwRules)
	{
		if (pFwRule->IsUnapproved())
			UnapprovedRules.insert(pFwRule->GetGuid(), pFwRule);
		if (pFwRule->IsBackup())
			AlteredRules.insert(pFwRule->GetGuid(), pFwRule);
	}

	m_AlteredFwRuleCount = 0;
	m_MissingFwRuleCount = 0;

	foreach(auto pFwRule, AlteredRules)
	{
		QString Id = "backup_fw_rule_" + pFwRule->GetGuid().ToQS();
		
		CFwRulePtr pNewFwRule = FwRules.value(pFwRule->GetOriginalGuid());
		if (pNewFwRule)
		{
			if (!Old.take(Id))
			{
				CIssuePtr pIssue(new CFirewallIssue(pFwRule, pNewFwRule));
				AddIssueImpl(Id, pIssue);
			}
			UnapprovedRules.remove(pNewFwRule->GetGuid());
			m_AlteredFwRuleCount++;
		}
		else
		{
			if (!Old.take(Id))
			{
				CIssuePtr pIssue(new CFirewallIssue(pFwRule));
				AddIssueImpl(Id, pIssue);
			}
			m_MissingFwRuleCount++;
		}
	}

	m_UnapprovedFwRuleCount = UnapprovedRules.count();

	if(m_UnapprovedFwRuleCount > FwRules.count() * 8 / 10) // almost no approved rules
	{
		QString Id = "unapproved_fw_rules";
		if (!Old.take(Id))
		{
			CIssuePtr pIssue(new CCustomIssue(CCustomIssue::eFwNotApproved));
			AddIssueImpl(Id, pIssue);
		}
	}
	else
	{
		foreach(auto pFwRule, UnapprovedRules)
		{
			QString Id = "unapproved_fw_rule_" + pFwRule->GetGuid().ToQS();
			if (!Old.take(Id))
			{
				CIssuePtr  pIssue(new CFirewallIssue(pFwRule));
				AddIssueImpl(Id, pIssue);
			}
		}
	}

	//
	// Tweaks
	//

	foreach(auto pTweak, theCore->TweakManager()->GetTweaks())
	{
		switch (pTweak->GetType()) {
		case ETweakType::eGroup:
		case ETweakType::eSet:
			continue;
		}

		if (pTweak->GetStatus() == ETweakStatus::eMissing || pTweak->GetStatus() == ETweakStatus::eIneffective)
		{
			QString Id = "missing_tweak_" + pTweak->GetId();
			if (!Old.take(Id))
			{
				CIssuePtr pIssue(new CTweakIssue(pTweak));
				AddIssueImpl(Id, pIssue);
			}
		}
	}

	foreach(const QString& Id, Old.keys())
		m_Issues.remove(Id);
}

void CIssueManager::AddIssueImpl(const QString& Id, CIssuePtr pIssue)
{
	if(theConf->GetValue(HIDDEN_ISSUES_GROUP "/" + Id).toBool())
		return;
	m_Issues.insert(Id, pIssue);
}

STATUS CIssueManager::FixIssue(const QString& Id, CIssue::EFixMode Mode)
{
	CIssuePtr pIssue = m_Issues.value(Id);
	if(!pIssue)
		return ERR(STATUS_NOT_FOUND);

	STATUS Status = pIssue->FixIssue(Mode);
	if(Status)
		m_Issues.remove(Id);
	return Status;
}

void CIssueManager::HideIssue(const QString& Id)
{
	theConf->SetValue(HIDDEN_ISSUES_GROUP "/" + Id, true);
}

/////////////////////////////////////////////////////////////////////////////
// CCustomIssue
//

QString CIssue::GetSeverityStr() const
{
	switch (m_Severity)
	{
	case eLow:      return tr("Low");
	case eMedium:   return tr("Medium");
	case eHigh:     return tr("High");
	case eCritical: return tr("Critical");
	default:        return tr("Undefined");
	}
}


CCustomIssue::CCustomIssue(EIssueType Type, QObject* parent) 
	: CIssue(parent) 
{ 
	m_eType = Type; 
	switch (m_eType)
	{
	case eNoUserKey:
		m_Description = tr("No user key is set, some features may not work!");
		m_Severity = eCritical;
		break;
	case eFwNotApproved:
		m_Description = tr("Existing Windows Firewall rules have not been approved yet!");
		m_Severity = eMedium;
		break;
	};
}

QIcon CCustomIssue::GetIcon() const
{
	switch (m_eType)
	{
	case eNoUserKey:
		return QIcon(":/Icons/AddKey.png");
	case eFwNotApproved:
		return QIcon(":/Icons/Wall2.png");
	default:
		return QIcon(":/Icons/Tweaks.png");
	};
}

STATUS CCustomIssue::FixIssue(EFixMode Mode)
{
	if(Mode != eDefault)
		return ERR(STATUS_NOT_IMPLEMENTED, tr("This issue can not be fixed in this mode!").toStdWString());

	switch (m_eType)
	{
	case eNoUserKey: {
		return theGUI->MakeKeyPair();
	}
	case eFwNotApproved: {
		auto FwRules = theCore->NetworkManager()->GetFwRules();
		foreach(auto pFwRule, FwRules) {
			if (pFwRule->IsUnapproved()) {
				if(pFwRule->GetState() == EFwRuleState::eUnapprovedDisabled)
					pFwRule->SetEnabled(true);
				pFwRule->SetApproved();
				STATUS Status = theCore->NetworkManager()->SetFwRule(pFwRule);
				if (Status.IsError())
					return Status;
			}
		}
		return OK;
	}
	default:
		return ERR(STATUS_NOT_IMPLEMENTED, tr("This issue can not be fixed!").toStdWString());
	};
}

/////////////////////////////////////////////////////////////////////////////
// CFirewallIssue
//

CFirewallIssue::CFirewallIssue(const CFwRulePtr& pFwRule, QObject* parent) 
	: CIssue(parent) 
{
	m_RuleID = pFwRule->GetGuid().ToQS();
	switch (pFwRule->GetState())
	{
		case EFwRuleState::eUnapprovedDisabled:
		case EFwRuleState::eUnapproved:
			m_Description = tr("Unapproved Windows Firewall rule: %1").arg(CMajorPrivacy::GetResourceStr(pFwRule->GetName()));
			if(pFwRule->IsEnabled())
				m_Severity = eMedium;
			else
				m_Severity = eLow;
			break;
		case EFwRuleState::eBackup:
			m_Description = tr("Missing Windows Firewall rule: %1").arg(CMajorPrivacy::GetResourceStr(pFwRule->GetName()));
			m_Severity = eHigh;
			break;
		default:
			ASSERT(0);
	}
}

CFirewallIssue::CFirewallIssue(const CFwRulePtr& pFwRule, const CFwRulePtr& pNewFwRule, QObject* parent) 
	: CIssue(parent) 
{
	m_RuleID = pFwRule->GetGuid().ToQS();
	ASSERT(pFwRule->GetState() == EFwRuleState::eBackup);
	m_Description = tr("Altered Windows Firewall rule: %1").arg(CMajorPrivacy::GetResourceStr(pFwRule->GetName()));
	m_Severity = eHigh;
}

QIcon CFirewallIssue::GetIcon() const
{
	return QIcon(":/Icons/Wall2.png");
}

STATUS CFirewallIssue::FixIssue(EFixMode Mode)
{
	CFwRulePtr pFwRule = theCore->NetworkManager()->GetFwRules().value(m_RuleID);

	switch (pFwRule->GetState())
	{
	case EFwRuleState::eUnapprovedDisabled:
	case EFwRuleState::eUnapproved:
		if(Mode == eAccept)
		{
			if(pFwRule->GetState() == EFwRuleState::eUnapprovedDisabled)
				pFwRule->SetEnabled(true);
			pFwRule->SetApproved();
			return theCore->NetworkManager()->SetFwRule(pFwRule);
		}
		else if(Mode == eReject)
			return theCore->NetworkManager()->DelFwRule(pFwRule);
		else
			return ERR(STATUS_NOT_IMPLEMENTED, tr("This issue can not be fixed in this mode!").toStdWString());
	case EFwRuleState::eBackup:
		if(Mode == eAccept)
			return theCore->NetworkManager()->DelFwRule(pFwRule);
		else if (Mode == eReject)
		{
			pFwRule->SetApproved();
			return theCore->NetworkManager()->SetFwRule(pFwRule);
		}
		else if(Mode != eDefault)
			return ERR(STATUS_NOT_IMPLEMENTED, tr("This issue can not be fixed in this mode!").toStdWString());
	default:
		return ERR(STATUS_NOT_IMPLEMENTED, tr("This issue can not be fixed!").toStdWString());
	}
}

/////////////////////////////////////////////////////////////////////////////
// CTweakIssue
//

CTweakIssue::CTweakIssue(const CTweakPtr& pTweak, QObject* parent) 
	: CIssue(parent) 
{
	m_TweakID = pTweak->GetId();

	switch (pTweak->GetStatus())
	{
	case ETweakStatus::eMissing:
		m_Description = tr("Missing Tweak: %1").arg(pTweak->GetName());
		break;
	case ETweakStatus::eIneffective:
		m_Description = tr("Ineffective Tweak: %1").arg(pTweak->GetName());
		break;
	}
	m_Severity = eHigh; // todo make it tweak dependant
}

QIcon CTweakIssue::GetIcon() const
{
	return QIcon(":/Icons/Tweaks.png");
}

STATUS CTweakIssue::FixIssue(EFixMode Mode)
{
	CTweakPtr pTweak = theCore->TweakManager()->GetTweaks().value(m_TweakID);

	if(Mode != eDefault)
		return ERR(STATUS_NOT_IMPLEMENTED, tr("This issue can not be fixed in this mode!").toStdWString());

	if(pTweak->GetStatus() == ETweakStatus::eIneffective)
		return ERR(STATUS_UNSUCCESSFUL, tr("When a tweak is inneffective this can not be fixed it means that you windows version does not respect this preset.\n"
			"You can undo the tweak from the tweaks page to remowe the warning.").toStdWString());

	return theCore->TweakManager()->ApplyTweak(pTweak);
}