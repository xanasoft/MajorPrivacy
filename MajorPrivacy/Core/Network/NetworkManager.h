#pragma once
#include "../Library/Status.h"
#include "FwRule.h"
#include "DnsCacheEntry.h"

class CNetworkManager : public QObject
{
    Q_OBJECT

public:
    CNetworkManager(QObject *parent = nullptr);

	bool UpdateAllFwRules();
	bool UpdateFwRule(const QString& RuleId);
	void RemoveFwRule(const QString& RuleId);

	QSet<QString> GetFwRuleIDs() const;
	QList<CFwRulePtr> GetFwRules() const { return m_FwRules.values(); }
	//QList<CFwRulePtr> GetFwRulesFor(const QList<const class CProgramItem*>& Nodes);
	QList<CFwRulePtr> GetFwRules(const QSet<QString> &FwRuleIDs);

	STATUS SetFwRule(const CFwRulePtr& pRule);
	RESULT(CFwRulePtr) GetRule(QString Guid);
	STATUS DelRule(QString Guid);

	void UpdateDnsCache();
	QMultiMap<quint64, CDnsCacheEntryPtr>		GetDnsCache() { return m_DnsCache; }

protected:

	void AddFwRule(const CFwRulePtr& pFwRule);
	void RemoveFwRule(const CFwRulePtr& pFwRule);

	QMap<QString, CFwRulePtr> m_FwRules;

	//QMultiMap<QString, CFwRulePtr>	m_FileRules;
	//QMultiMap<QString, CFwRulePtr>	m_SvcRules;
	//QMultiMap<QString, CFwRulePtr>	m_AppRules;


	QMap<quint64, CDnsCacheEntryPtr>	m_DnsCache;
};