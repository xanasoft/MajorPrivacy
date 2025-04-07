#pragma once
#include "../Library/Status.h"
#include "FwRule.h"
#include "DnsRule.h"
#include "DnsCacheEntry.h"

class CNetworkManager : public QObject
{
    Q_OBJECT

public:
    CNetworkManager(QObject *parent = nullptr);

	// FW
	bool UpdateAllFwRules(bool bReLoad = false);
	bool UpdateFwRule(const QFlexGuid& Guid);
	void RemoveFwRule(const QFlexGuid& Guid);

	QSet<QFlexGuid> GetFwRuleIDs() const;
	QList<CFwRulePtr> GetFwRules() const { return m_FwRules.values(); }
	//QList<CFwRulePtr> GetFwRulesFor(const QList<const class CProgramItem*>& Nodes);
	QList<CFwRulePtr> GetFwRules(const QSet<QFlexGuid> &FwRuleIDs);

	STATUS SetFwRule(const CFwRulePtr& pRule);
	//RESULT(CFwRulePtr) GetFwRule(const QFlexGuid& Guid);
	STATUS DelFwRule(const CFwRulePtr& pRule);

	// dns
	bool UpdateAllDnsRules();
	bool UpdateDnsRule(const QFlexGuid& Guid);
	void RemoveDnsRule(const QFlexGuid& Guid);

	QList<CDnsRulePtr> GetDnsRules() const { return m_DnsRules.values(); }

	STATUS SetDnsRule(const CDnsRulePtr& pRule);
	//RESULT(CDnsRulePtr) GetFwRule(const QFlexGuid& Guid);
	STATUS DelDnsRule(const CDnsRulePtr& pRule);

	void UpdateDnsCache();
	QMultiMap<quint64, CDnsCacheEntryPtr>		GetDnsCache() { return m_DnsCache; }

protected:

	void AddFwRule(const CFwRulePtr& pFwRule);
	void RemoveFwRule(const CFwRulePtr& pFwRule);

	QMap<QFlexGuid, CFwRulePtr> m_FwRules;
	//QMultiMap<QString, CFwRulePtr>	m_FileRules;
	//QMultiMap<QString, CFwRulePtr>	m_SvcRules;
	//QMultiMap<QString, CFwRulePtr>	m_AppRules;

	QMap<QFlexGuid, CDnsRulePtr> m_DnsRules;

	QMap<quint64, CDnsCacheEntryPtr>	m_DnsCache;
};