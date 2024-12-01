#pragma once
#include "../Library/Status.h"
#include "../Library/Helpers/RegUtil.h"
#include "../Library/Common/Address.h"
#include "Firewall/FirewallDefs.h"

class CUAddress : public CAddress
{
public:
	CUAddress() {}

protected:
	friend class CNetworkManager;

	uint8 OnLinkPrefixLength;
	uint32 PrefixOrigin;
	uint32 SuffixOrigin;
};

struct SAdapterInfo
{
	EFwProfiles Profile = EFwProfiles::Invalid;
	EFwInterfaces Type = EFwInterfaces::All;

	std::list<CUAddress>UnicastAddresses;
	std::list<CAddress> AnycastAddresses;
	std::list<CAddress> MulticastAddresses;

	std::list<CAddress> GatewayAddresses;

	std::list<CAddress> DnsServersAddresses;
	std::list<CAddress> WinsServersAddresses;

	std::list<CAddress> DnsAddresses;
	std::list<CAddress> DhcpServerAddresses;
};

typedef std::shared_ptr<SAdapterInfo> SAdapterInfoPtr;

class CNetworkManager
{
	TRACK_OBJECT(CNetworkManager)
public:
	CNetworkManager();
	~CNetworkManager();

	STATUS Init();

	STATUS Load();
	STATUS Store();
	STATUS StoreAsync();

	void Update();

	class CFirewall* Firewall()			{ return m_pFirewall; }
	class CSocketList* SocketList()		{ return m_pSocketList; }
	class CDnsInspector* DnsInspector()	{ return m_pDnsInspector; }

	SAdapterInfoPtr GetAdapterInfoByIP(const CAddress& IP);
	EFwProfiles GetDefaultProfile()		{ return m_DefaultProfile; }

protected:

	void UpdateAdapterInfo();

	class CFirewall*		m_pFirewall = NULL;
	class CSocketList*		m_pSocketList = NULL;
	class CDnsInspector*	m_pDnsInspector = NULL;

	CRegWatcher				m_NicKeyWatcher;
	bool					m_UpdateAdapterInfo = true;
	std::shared_mutex		m_AdapterInfoMutex;
	std::map<CAddress, SAdapterInfoPtr> m_AdapterInfoByIP;
	EFwProfiles				m_DefaultProfile = EFwProfiles::Invalid;

	friend DWORD CALLBACK CNetworkManager__LoadProc(LPVOID lpThreadParameter);
	friend DWORD CALLBACK CNetworkManager__StoreProc(LPVOID lpThreadParameter);
	HANDLE					m_hStoreThread = NULL;
};