#pragma once
#include "../Library/Common/Address.h"
#include "DnsHostName.h"

class CDnsProcLog
{
	TRACK_OBJECT(CDnsProcLog)
public:
    CDnsProcLog();
	//~CDnsProcLog();
	
	void Update();

	bool ResolveHost(const CAddress& Address, const CHostNamePtr& pHostName);

	void OnEtwDnsEvent(const struct SEtwDnsEvent* pEvent);

protected:

	mutable std::shared_mutex		m_Mutex;
	
	std::multimap<CAddress, CHostNameRef> m_ResolveList;

	struct SAddrInfo
	{
		SAddrInfo(const CAddress& Addresses) : Addresses(Addresses) {}
		const CAddress Addresses;
		uint64 TimeStamp = 0;
		uint32 HitCount = 0;
	};

	typedef std::shared_ptr<SAddrInfo> SAddrInfoPtr;

	struct SHostNameInfo
	{
		SHostNameInfo(const std::wstring& HostName) : HostName(HostName) {}
		const std::wstring HostName;
		std::map<CAddress, SAddrInfoPtr> Addresses;
		uint64 TimeStamp = 0;
		uint32 HitCount = 0;
	};

	typedef std::shared_ptr<SHostNameInfo> SHostNameInfoPtr;
	typedef std::weak_ptr<SHostNameInfo> SHostNameInfoRef;

	std::map<std::wstring, SHostNameInfoPtr> m_DnsByHost;
	std::multimap<CAddress, SHostNameInfoRef> m_DnsByAddr;

	SHostNameInfoPtr GetHostForAddress(const CAddress& Address);

};