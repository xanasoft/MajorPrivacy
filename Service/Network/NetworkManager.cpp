#include "pch.h"

#include <objbase.h>
#include <Netlistmgr.h>
//#include <comdef.h>
#include <WS2tcpip.h>
#include <IPHlpApi.h>

#include "NetworkManager.h"
#include "Firewall/Firewall.h"
#include "SocketList.h"
#include "Dns/DnsInspector.h"


CNetworkManager::CNetworkManager()
{
	m_NicKeyWatcher.AddKey(L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces");
	m_NicKeyWatcher.AddKey(L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\Tcpip6\\Parameters\\Interfaces");
	m_NicKeyWatcher.AddKey(L"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkList\\Profiles"); // firewall profile configuration

	m_NicKeyWatcher.RegisterHandler([&](const std::wstring&){
		m_UpdateAdapterInfo = true;
	});

	m_NicKeyWatcher.Start();

	m_pFirewall = new CFirewall();
	m_pSocketList = new CSocketList();
    m_pDnsInspector = new CDnsInspector();
}

CNetworkManager::~CNetworkManager()
{
	m_NicKeyWatcher.Stop();

	delete m_pFirewall;
	delete m_pSocketList;
    delete m_pDnsInspector;
}

STATUS CNetworkManager::Init()
{
	UpdateAdapterInfo();

    m_pDnsInspector->Init();
	m_pFirewall->Init();
	m_pSocketList->Init();

	return OK;
}

void CNetworkManager::Update()
{
    if (m_UpdateAdapterInfo)
        UpdateAdapterInfo();

    m_pDnsInspector->Update();
    m_pSocketList->Update();
	m_pFirewall->Update();
}

void CNetworkManager::UpdateAdapterInfo()
{
    HRESULT hr = S_OK;

    std::map<std::string, EFwProfiles> NetworkProfiles;

    auto NetCatToProfile = [](DWORD nlmCategory) {
        switch (nlmCategory) {
        case NLM_NETWORK_CATEGORY_PRIVATE:              return EFwProfiles::Private; 
        case NLM_NETWORK_CATEGORY_PUBLIC:               return EFwProfiles::Public; 
        case NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED: return EFwProfiles::Domain; 
        default:                                        return EFwProfiles::Invalid;
        }
    };

    //hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    //if (FAILED(hr))
    //    return;

    INetworkListManager* pNetworkListManager;
    hr = CoCreateInstance(CLSID_NetworkListManager, NULL, CLSCTX_ALL, IID_INetworkListManager, (LPVOID*)&pNetworkListManager);
    if (SUCCEEDED(hr)) 
    {
        IEnumNetworks* pEnumNetworks;
        hr = pNetworkListManager->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, &pEnumNetworks);
        if (SUCCEEDED(hr)) 
        {
            INetwork* pNetwork;
            ULONG fetched;

            while (pEnumNetworks->Next(1, &pNetwork, &fetched) == S_OK) 
            {
                NLM_NETWORK_CATEGORY nlmCategory;
                pNetwork->GetCategory(&nlmCategory);

                EFwProfiles FirewallProfile = NetCatToProfile(nlmCategory);
                if(FirewallProfile == EFwProfiles::Invalid){
                    pNetwork->Release();
                    continue;
                }

                IEnumNetworkConnections* pEnumNetworkConnections;
                hr = pNetwork->GetNetworkConnections(&pEnumNetworkConnections);
                if (SUCCEEDED(hr)) 
                {
                    INetworkConnection* pNetworkConnection;
                    while (pEnumNetworkConnections->Next(1, &pNetworkConnection, &fetched) == S_OK) 
                    {
                        GUID adapterId;
                        pNetworkConnection->GetAdapterId(&adapterId);

                        WCHAR guidString[40] = {0};
                        StringFromGUID2(adapterId, guidString, 40);

                        std::wstring guid(guidString);
#pragma warning(push)
#pragma warning(disable : 4244)
                        NetworkProfiles[std::string(guid.begin(), guid.end())] = FirewallProfile;
#pragma warning(pop)

                        pNetworkConnection->Release();
                    }
                    pEnumNetworkConnections->Release();
                }

                pNetwork->Release();
            }

            pEnumNetworks->Release();
        }

        pNetworkListManager->Release();
    }

    //CoUninitialize();

    // Get default behavioure for unidentified networks
    DWORD DefaultNetCategory = RegQueryDWord(HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\\Policies\\Microsoft\\Windows NT\\CurrentVersion\\NetworkList\\Signatures\\"
        L"010103000F0000F0010000000F0000F0C967A3643C3AD745950DA7859209176EF5B87C875FA20DF21951640E807D7C24",
        L"Category", 0);
    EFwProfiles DefaultProfile = NetCatToProfile(DefaultNetCategory);

    std::map<CAddress, SAdapterInfoPtr> AdapterInfoByIP;

	//MIB_IF_TABLE2* pIfTable = NULL;
	//if (GetIfTable2(&pIfTable) == NO_ERROR)
	//{
	//	uint64 uRecvTotal = 0;
	//	uint64 uRecvCount = 0;
	//	uint64 uSentTotal = 0;
	//	uint64 uSentCount = 0;
	//	for (int i = 0; i < pIfTable->NumEntries; i++)
	//	{
	//		MIB_IF_ROW2* pIfRow = (MIB_IF_ROW2*)&pIfTable->Table[i];
    //
	//		if (pIfRow->InterfaceAndOperStatusFlags.FilterInterface)
	//			continue;
    //
    //      // todo
	//	}
	//}
	//FreeMibTable(pIfTable);

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_WINS_INFO | GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_ALL_INTERFACES;
    ULONG bufferLength = 0x1000;
    std::vector<char> buffer;
    for (ULONG ret = ERROR_BUFFER_OVERFLOW; ret == ERROR_BUFFER_OVERFLOW && bufferLength > buffer.size(); )
    {
        buffer.resize(bufferLength, 0);
        ret = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, (PIP_ADAPTER_ADDRESSES)buffer.data(), &bufferLength);
        if (ret != ERROR_SUCCESS)
            continue;
        
        for (PIP_ADAPTER_ADDRESSES itf = (PIP_ADAPTER_ADDRESSES)buffer.data(); itf; itf = itf->Next)
        {
            if (itf->OperStatus == IfOperStatusNotPresent || (!itf->Ipv4Enabled && !itf->Ipv6Enabled))
                continue;
            //DbgPrint("name: %s (%S) %d\n", itf->AdapterName, itf->FriendlyName, itf->OperStatus);
            
            SAdapterInfoPtr Info = std::make_shared<SAdapterInfo>();

            auto F = NetworkProfiles.find(itf->AdapterName);
            if (F != NetworkProfiles.end())
                Info->Profile = F->second;
            else
                Info->Profile = DefaultProfile;

            switch (itf->IfType)
            {
                case IF_TYPE_ETHERNET_CSMACD:
                case IF_TYPE_GIGABITETHERNET:
                case IF_TYPE_FASTETHER:
                case IF_TYPE_FASTETHER_FX:
                case IF_TYPE_ETHERNET_3MBIT:
                case IF_TYPE_ISO88025_TOKENRING:    
                                                    Info->Type = EFwInterfaces::Lan; break;
                case IF_TYPE_IEEE80211:             Info->Type = EFwInterfaces::Wireless; break;
                case IF_TYPE_PPP:                   Info->Type = EFwInterfaces::RemoteAccess; break;
                default:                            Info->Type = EFwInterfaces::All; break;
            }

            for (PIP_ADAPTER_UNICAST_ADDRESS_LH addr = itf->FirstUnicastAddress; addr; addr = addr->Next) {
                CUAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                IP.OnLinkPrefixLength = addr->OnLinkPrefixLength;
                IP.PrefixOrigin = addr->PrefixOrigin;
                IP.SuffixOrigin = addr->SuffixOrigin;
                Info->UnicastAddresses.push_back(IP);
            }
            for (PIP_ADAPTER_ANYCAST_ADDRESS_XP addr = itf->FirstAnycastAddress; addr; addr = addr->Next) {
                CAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                Info->AnycastAddresses.push_back(IP);
            }
            for (PIP_ADAPTER_MULTICAST_ADDRESS_XP addr = itf->FirstMulticastAddress; addr; addr = addr->Next) {
                CAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                Info->MulticastAddresses.push_back(IP);
            }

            for (PIP_ADAPTER_GATEWAY_ADDRESS_LH addr = itf->FirstGatewayAddress; addr; addr = addr->Next) {
                CAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                Info->GatewayAddresses.push_back(IP);
            }

            for(PIP_ADAPTER_DNS_SERVER_ADDRESS_XP addr = itf->FirstDnsServerAddress; addr; addr = addr->Next) {
                CAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                Info->DnsAddresses.push_back(IP);
            }
            for(PIP_ADAPTER_WINS_SERVER_ADDRESS_LH addr = itf->FirstWinsServerAddress; addr; addr = addr->Next) {
                CAddress IP;
                IP.FromSA(addr->Address.lpSockaddr, addr->Address.iSockaddrLength);
                Info->WinsServersAddresses.push_back(IP);
            }

            if (itf->Dhcpv4Server.iSockaddrLength) {
                CAddress IP;
                IP.FromSA(itf->Dhcpv4Server.lpSockaddr, itf->Dhcpv4Server.iSockaddrLength);
                Info->DhcpServerAddresses.push_back(IP);
            }
            if (itf->Dhcpv6Server.iSockaddrLength){
                CAddress IP;
                IP.FromSA(itf->Dhcpv6Server.lpSockaddr, itf->Dhcpv6Server.iSockaddrLength);
                Info->DhcpServerAddresses.push_back(IP);
            }

            for (auto IpInfo : Info->UnicastAddresses)
                AdapterInfoByIP[IpInfo] = Info;
        }
    }

    std::unique_lock Lock(m_AdapterInfoMutex);
    m_AdapterInfoByIP = AdapterInfoByIP;
    m_DefaultProfile = DefaultProfile;
	m_UpdateAdapterInfo = false;
}

SAdapterInfoPtr CNetworkManager::GetAdapterInfoByIP(const CAddress& IP)
{
	std::shared_lock Lock(m_AdapterInfoMutex);
    auto F = m_AdapterInfoByIP.find(IP);
    if (F != m_AdapterInfoByIP.end())
        return F->second;
    return SAdapterInfoPtr();
}