#pragma once
#include "../Library/Status.h"
#include "DnsCacheEntry.h"
#include "..\Library\Common\Address.h"
#include "..\Library\Common\StVariant.h"
#include "DnsHostName.h"

class CDnsInspector
{
	TRACK_OBJECT(CDnsInspector)
public:
	CDnsInspector();
	~CDnsInspector();

	STATUS Init();

	void Update();

	std::multimap<std::wstring, CDnsCacheEntryPtr> GetDnsCache() { std::shared_lock Lock(m_Mutex); return m_DnsCache; }

	bool ResolveHost(const CAddress& Address, const CHostNamePtr& pHostName);

	StVariant DumpDnsCache();

	void FlushDnsCache();

protected:
	void OnEtwDnsEvent(const struct SEtwDnsEvent* pEvent);
	void OnDnsFilterEvent(const struct SDnsFilterEvent* pEvent);

	void ProcessJobList();

	std::wstring GetHostNameSmart(const CAddress& Address, const std::vector<std::wstring>& RevHostNames);
	std::wstring GetHostNamesSmart(const std::wstring& HostName, int Limit = 10);

	std::shared_mutex m_Mutex;

	std::multimap<std::wstring, CDnsCacheEntryPtr>		m_DnsCache; // HostName -> Entry
	uint64												m_LastCacheTraverse = 0;
	std::multimap<CAddress, CDnsCacheEntryPtr>			m_AddressCache;
	std::multimap<std::wstring, CDnsCacheEntryPtr>		m_RedirectionCache;

	std::map<CAddress, uint64>							m_NoResultBlackList;

	std::mutex											m_JobMutex;
	std::multimap<CAddress, CHostNamePtr>				m_JobList;
	bool												m_bRunning = false;
	std::shared_ptr<std::thread>						m_Resolver;
private:
	struct SDnsInspector* m;
};