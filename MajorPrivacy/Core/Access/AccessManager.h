#pragma once

#include "../Library/Status.h"
#include "AccessRule.h"

class CAccessManager: public QObject
{
	Q_OBJECT
public:
	CAccessManager(QObject* parent = nullptr);

	STATUS Update();

	bool UpdateAllAccessRules();
	bool UpdateAccessRule(const QString& RuleId);
	void RemoveAccessRule(const QString& RuleId);

	QSet<QString> GetAccessRuleIDs() const;
	QList<CAccessRulePtr> GetAccessRules() const { return m_AccessRules.values(); }
	//QList<CAccessRulePtr> GetAccessRulesFor(const QList<const class CAccessItem*>& Nodes);
	QList<CAccessRulePtr> GetAccessRules(const QSet<QString> &AccessRuleIDs);

	STATUS SetAccessRule(const CAccessRulePtr& pRule);
	RESULT(CAccessRulePtr) GetAccessRule(QString Guid);
	STATUS DelAccessRule(const CAccessRulePtr& pRule);

protected:

	void AddAccessRule(const CAccessRulePtr& pFwRule);
	void RemoveAccessRule(const CAccessRulePtr& pFwRule);

	QMap<QString, CAccessRulePtr>			m_AccessRules;
};

