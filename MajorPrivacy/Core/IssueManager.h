#pragma once
#include "../Library/Status.h"
#include "../Core/Network/NetworkManager.h"
#include "../Core/Tweaks/TweakManager.h"

/////////////////////////////////////////////////////////////////////////////
// CIssue
//

class CIssue : public QObject
{
	Q_OBJECT
public:
	CIssue(QObject* parent = nullptr) : QObject(parent) {}

	virtual QIcon GetIcon() const = 0;

	QString GetDescription() const		{ return m_Description; }

	enum ESeverity
	{
		eUndefined = 0,
		eLow,
		eMedium,
		eHigh,
		eCritical
	};

	ESeverity GetSeverity() const		{ return m_Severity; }
	QString GetSeverityStr() const;

	enum EIssueType
	{
		eNone = 0,
		eNoUserKey,
		eFwNotApproved,
		eFwRuleAltered,
		eTweakFailed,
	};

	virtual EIssueType GetType() = 0;

	enum EFixMode
	{
		eDefault = 0,
		eAccept,
		eReject,
	};

	virtual STATUS FixIssue(EFixMode Mode) = 0;

protected:
	QString m_Description;
	ESeverity m_Severity = eUndefined;
};

typedef QSharedPointer<CIssue> CIssuePtr;

/////////////////////////////////////////////////////////////////////////////
// CIssueManager
//

class CIssueManager : public QObject
{
	Q_OBJECT
public:
	CIssueManager(QObject* parent = nullptr);
	
	void DetectIssues();

	QMap<QString, CIssuePtr> GetIssues() const { return m_Issues; }

	STATUS FixIssue(const QString& Id, CIssue::EFixMode Mode);
	void HideIssue(const QString& Id);

	int GetUnapprovedFwRuleCount() const { return m_UnapprovedFwRuleCount; }
	int GetAlteredFwRuleCount() const { return m_AlteredFwRuleCount; }
	int GetMissingFwRuleCount() const { return m_MissingFwRuleCount; }

protected:
	void AddIssueImpl(const QString& Id, CIssuePtr pIssue);

	QMap<QString, CIssuePtr> m_Issues;

	int m_UnapprovedFwRuleCount = 0;
	int m_AlteredFwRuleCount = 0;
	int m_MissingFwRuleCount = 0;
};


/////////////////////////////////////////////////////////////////////////////
// CCustomIssue
//

class CCustomIssue : public CIssue
{
	Q_OBJECT
public:

	CCustomIssue(EIssueType Type, QObject* parent = nullptr);

	EIssueType GetType() override { return m_eType; }

	QIcon GetIcon() const override;

	STATUS FixIssue(EFixMode Mode) override;

protected:

	EIssueType m_eType = eNone;
};

/////////////////////////////////////////////////////////////////////////////
// CFirewallIssue
//

class CFirewallIssue : public CIssue
{
	Q_OBJECT
public:
	CFirewallIssue(const CFwRulePtr& pFwRule, QObject* parent = nullptr);
	CFirewallIssue(const CFwRulePtr& pFwRule, const CFwRulePtr& pNewFwRule, QObject* parent = nullptr);

	EIssueType GetType() override { return eFwRuleAltered; }

	QIcon GetIcon() const override;

	STATUS FixIssue(EFixMode Mode) override;

protected:
	QString m_RuleID;
};

/////////////////////////////////////////////////////////////////////////////
// CTweakIssue
//

class CTweakIssue : public CIssue
{
	Q_OBJECT
public:
	CTweakIssue(const CTweakPtr& pTweak, QObject* parent = nullptr);

	EIssueType GetType() override { return eTweakFailed; }

	QIcon GetIcon() const override;

	STATUS FixIssue(EFixMode Mode) override;

protected:
	QString m_TweakID;
};
