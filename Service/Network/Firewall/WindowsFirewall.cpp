#include "pch.h"
#include "WindowsFirewallAPI.h"
#include "WindowsFirewall.h"
#include "../Library/Helpers/WinUtil.h"
#include "../Library/Common/DbgHelp.h"
#include "../Library/Common/Strings.h"
#include "../Library/Common/Address.h"
#include "../Library/Helpers/Scoped.h"

#include <combaseapi.h>
#include <WS2tcpip.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////
// SWindowsFirewall

struct SWindowsFirewall
{
    SWindowsFirewall()
    {
        hFirewallAPI = LoadLibraryW(L"FirewallAPI.dll");
        if (!hFirewallAPI)
            return;

        uApiVersion = DetectApiVersion();

    #define FW_API_GET(x) *(PVOID*)&p##x = GetProcAddress(hFirewallAPI, #x);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Firewall Policy
        FW_API_GET(FWOpenPolicyStore)
        FW_API_GET(FWClosePolicyStore)
        FW_API_GET(FWRestoreGPODefaults)
        FW_API_GET(FWStatusMessageFromStatusCode)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Notifications
        FW_API_GET(FWChangeNotificationCreate)
        FW_API_GET(FWChangeNotificationDestroy)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Transactions
        //FW_API_GET(FWChangeTransactionalState)
        //FW_API_GET(FWRevertTransaction)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Firewall Rules
        FW_API_GET(FWEnumFirewallRules)
        FW_API_GET(FWQueryFirewallRules)
        FW_API_GET(FWFreeFirewallRules)
        FW_API_GET(FWDeleteAllFirewallRules)
        FW_API_GET(FWDeleteFirewallRule)
        FW_API_GET(FWAddFirewallRule)
        FW_API_GET(FWSetFirewallRule)
        FW_API_GET(FWVerifyFirewallRule)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Firewall Settings
        FW_API_GET(FWExportPolicy)
        FW_API_GET(FWRestoreDefaults)
        FW_API_GET(FWRestoreGPODefaults)
        FW_API_GET(FWGetGlobalConfig)
        FW_API_GET(FWSetGlobalConfig)
        FW_API_GET(FWGetConfig)
        FW_API_GET(FWSetConfig)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // App Packages
        FW_API_GET(NetworkIsolationEnumAppContainers)
        FW_API_GET(NetworkIsolationFreeAppContainers)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Network Info
        FW_API_GET(FWEnumNetworks)
        FW_API_GET(FWFreeNetworks)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Adapter Info
        FW_API_GET(FWEnumAdapters)
        FW_API_GET(FWFreeAdapters)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Product Info
        FW_API_GET(FWEnumProducts)
        FW_API_GET(FWFreeProducts)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Authentication

        FW_API_GET(FWAddAuthenticationSet)
        FW_API_GET(FWSetAuthenticationSet)
        FW_API_GET(FWDeleteAuthenticationSet)
        FW_API_GET(FWDeleteAllAuthenticationSets)
        FW_API_GET(FWEnumAuthenticationSets)
        FW_API_GET(FWQueryAuthenticationSets)
        //FW_API_GET(FWVerifyAuthenticationSet)
        //FW_API_GET(FWFreeAuthenticationSets)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Connection security rules

        FW_API_GET(FWEnumConnectionSecurityRules)
        FW_API_GET(FWQueryConnectionSecurityRules)
        FW_API_GET(FWAddConnectionSecurityRule)
        FW_API_GET(FWSetConnectionSecurityRule)
        FW_API_GET(FWDeleteConnectionSecurityRule)
        FW_API_GET(FWDeleteAllConnectionSecurityRules)
        //FW_API_GET(FWFreeConnectionSecurityRules)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Crypto

        FW_API_GET(FWEnumCryptoSets)
        FW_API_GET(FWQueryCryptoSets)
        FW_API_GET(FWAddCryptoSet)
        FW_API_GET(FWSetCryptoSet)
        FW_API_GET(FWDeleteCryptoSet)
        FW_API_GET(FWDeleteAllCryptoSets)
        //FW_API_GET(FWVerifyCryptoSet)
        //FW_API_GET(FWFreeCryptoSets)

        FW_API_GET(FWEnumPhase1SAs)
        FW_API_GET(FWDeletePhase1SAs)
        //FW_API_GET(FWFreePhase1SAs)

        FW_API_GET(FWEnumPhase2SAs)
        FW_API_GET(FWDeletePhase2SAs)
        //FW_API_GET(FWFreePhase2SAs)

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 

    #undef FW_API_GET
    }

    USHORT DetectApiVersion();


    std::vector<std::wstring> GetPortsStrings(const FW_PORTS* ports);
    FW_PORT_KEYWORD MakeSpecialPort(std::vector<std::wstring> portList);
    void MakePortsArray(PFW_PORTS ports, const std::vector<std::wstring>& portList, std::list<std::vector<BYTE>>& Buffer);

    FW_ADDRESS_KEYWORD MakeSpecialAddress(const std::vector<std::wstring>& addressList);
    void MakeAddressList(PFW_ADDRESSES addresses, const std::vector<std::wstring>& addressList, std::list<std::vector<BYTE>>& Buffer);
    std::vector<std::wstring> GetSpecialAddress(FW_ADDRESS_KEYWORD addressKeywords);
    std::vector<std::wstring> GetAddressesStrings(const FW_ADDRESSES* addresses);

    std::vector<SWindowsFwRule::IcmpTypeCode> GetIcmpTypesAndCodes(const FW_ICMP_TYPE_CODE_LIST* TypeCodeList);
    void MakeIcmpTypesAndCodes(PFW_ICMP_TYPE_CODE_LIST TypeCodeList, const std::vector<SWindowsFwRule::IcmpTypeCode>& list, std::list<std::vector<BYTE>>& Buffer);

    std::vector<std::wstring> GetPlatformValidity(const FW_OS_PLATFORM_LIST* platformList);
    void MakePlatformValidity(FW_OS_PLATFORM_LIST* platformList, std::vector<std::wstring> list, std::list<std::vector<BYTE>>& Buffer);

    SWindowsFwRulePtr LoadRule(const FW_RULE* fwRule);
    FW_RULE* SaveRule(const SWindowsFwRulePtr& pRule, std::list<std::vector<BYTE>>& Buffer);

    ULONG GetFWConfig(FW_PROFILE_TYPE profile, FW_PROFILE_CONFIG conf);
    ULONG SetFWConfig(FW_PROFILE_TYPE profile, FW_PROFILE_CONFIG conf, ULONG value);

    HMODULE hFirewallAPI = NULL;
    USHORT uApiVersion = 0;

    CRITICAL_SECTION cPolicyLock;
    FW_POLICY_STORE_HANDLE hPolicyHandle = NULL;

#define FW_API_DEF(x)   P_##x p##x = NULL;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Firewall Policy
    FW_API_DEF(FWOpenPolicyStore)
    FW_API_DEF(FWClosePolicyStore)
    FW_API_DEF(FwIsGroupPolicyEnforced)
    FW_API_DEF(FWStatusMessageFromStatusCode)

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Notifications
    FW_API_DEF(FWChangeNotificationCreate)
    FW_API_DEF(FWChangeNotificationDestroy)

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Transactions
    //FW_API_DEF(FWChangeTransactionalState)
    //FW_API_DEF(FWRevertTransaction)

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Firewall Rules
    FW_API_DEF(FWEnumFirewallRules)
    FW_API_DEF(FWQueryFirewallRules)
    FW_API_DEF(FWFreeFirewallRules)
    FW_API_DEF(FWDeleteAllFirewallRules)
    FW_API_DEF(FWDeleteFirewallRule)
    FW_API_DEF(FWAddFirewallRule)
    FW_API_DEF(FWSetFirewallRule)
    FW_API_DEF(FWVerifyFirewallRule)

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Firewall Settings
    FW_API_DEF(FWExportPolicy)
    FW_API_DEF(FWRestoreDefaults)
    FW_API_DEF(FWRestoreGPODefaults)
    FW_API_DEF(FWGetGlobalConfig)
    FW_API_DEF(FWSetGlobalConfig)
    FW_API_DEF(FWGetConfig)
    FW_API_DEF(FWSetConfig)

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // App Packages
    FW_API_DEF(NetworkIsolationEnumAppContainers)
    FW_API_DEF(NetworkIsolationFreeAppContainers)

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Network Info
    FW_API_DEF(FWEnumNetworks)
    FW_API_DEF(FWFreeNetworks)

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Adapter Info
    FW_API_DEF(FWEnumAdapters)
    FW_API_DEF(FWFreeAdapters)

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Product Info
    FW_API_DEF(FWEnumProducts)
    FW_API_DEF(FWFreeProducts)

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Authentication

    FW_API_DEF(FWAddAuthenticationSet)
    FW_API_DEF(FWSetAuthenticationSet)
    FW_API_DEF(FWDeleteAuthenticationSet)
    FW_API_DEF(FWDeleteAllAuthenticationSets)
    FW_API_DEF(FWEnumAuthenticationSets)
    FW_API_DEF(FWQueryAuthenticationSets)
    //FW_API_DEF(FWVerifyAuthenticationSet) // public static extern uint FWVerifyAuthenticationSet(ushort wBinaryVersion, ref FW_AUTH_SET pAuthSet);
    //FW_API_DEF(FWFreeAuthenticationSets) // public static extern uint FWFreeAuthenticationSets(IntPtr pAuthSets);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Connection security rules

    FW_API_DEF(FWEnumConnectionSecurityRules)
    FW_API_DEF(FWQueryConnectionSecurityRules)
    FW_API_DEF(FWAddConnectionSecurityRule)
    FW_API_DEF(FWSetConnectionSecurityRule)
    FW_API_DEF(FWDeleteConnectionSecurityRule)
    FW_API_DEF(FWDeleteAllConnectionSecurityRules)
    //FW_API_DEF(FWFreeConnectionSecurityRules) // public static extern uint FWFreeConnectionSecurityRules(IntPtr pRule);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Crypto

    FW_API_DEF(FWEnumCryptoSets)
    FW_API_DEF(FWQueryCryptoSets)
    FW_API_DEF(FWAddCryptoSet)
    FW_API_DEF(FWSetCryptoSet)
    FW_API_DEF(FWDeleteCryptoSet)
    FW_API_DEF(FWDeleteAllCryptoSets)
    //FW_API_DEF(FWVerifyCryptoSet) // public static extern uint FWVerifyCryptoSet(ushort wBinaryVersion, ref FW_CRYPTO_SET_PHASE1 pCryptoSet);
    //FW_API_DEF(FWFreeCryptoSets) // public static extern uint FWFreeCryptoSets(IntPtr pCryptoSets);

    FW_API_DEF(FWEnumPhase1SAs)
    FW_API_DEF(FWDeletePhase1SAs)
    //FW_API_DEF(FWFreePhase1SAs) // public static extern uint FWFreePhase1SAs(uint dwNumSAs, IntPtr pSAs);

    FW_API_DEF(FWEnumPhase2SAs)
    FW_API_DEF(FWDeletePhase2SAs)
    //FW_API_DEF(FWFreePhase2SAs) // public static extern uint FWFreePhase2SAs(uint dwNumSAs, IntPtr pSAs);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // 

#undef FW_API_DEF
};

USHORT SWindowsFirewall::DetectApiVersion()
{
    if (g_WindowsVersion >= WINDOWS_11_22H1)
        return FW_22H2_BINARY_VERSION;
    else if (g_WindowsVersion >= WINDOWS_11)
        return FW_21H2_BINARY_VERSION;
    else if (g_WindowsVersion >= WINDOWS_10_19H1)
        return FW_19Hx_BINARY_VERSION;
    else if (g_WindowsVersion >= WINDOWS_10_RS4)
        return FW_180x_BINARY_VERSION;
    else if (g_WindowsVersion >= WINDOWS_10_RS2)
        return FW_REDSTONE2_BINARY_VERSION;
    else if (g_WindowsVersion >= WINDOWS_10_RS1)
        return FW_REDSTONE1_BINARY_VERSION;
    else if (g_WindowsVersion >= WINDOWS_10_TH2)
        return FW_THRESHOLD2_BINARY_VERSION;
    else if (g_WindowsVersion >= WINDOWS_10)
        return FW_THRESHOLD_BINARY_VERSION;
    //else if (???)
    //    return FW_WIN10_BINARY_VERSION;
    else if (g_WindowsVersion >= WINDOWS_8_1)
        return FW_WIN8_1_BINARY_VERSION;
    else //if (g_WindowsVersion >= WINDOWS_7)
        return FW_SEVEN_BINARY_VERSION;
}

//static bool contains(const std::vector<std::wstring>& vec, const wchar_t* str) {
//    return std::find(vec.begin(), vec.end(), std::wstring(str)) != vec.end();
//}

std::vector<std::wstring> SWindowsFirewall::GetPortsStrings(const FW_PORTS* ports)
{
    std::vector<std::wstring> portList;
    
#define PORT_CHK_ADD(k, s) if (ports->wPortKeywords & (k)) portList.push_back(s);

    PORT_CHK_ADD(FW_PORT_KEYWORD_DYNAMIC_RPC_PORTS,SWindowsFwRule::PortKeywordRpc)
    PORT_CHK_ADD(FW_PORT_KEYWORD_RPC_EP,SWindowsFwRule::PortKeywordRpcEp)
    PORT_CHK_ADD(FW_PORT_KEYWORD_TEREDO_PORT,SWindowsFwRule::PortKeywordTeredo)
    PORT_CHK_ADD(FW_PORT_KEYWORD_IP_TLS_IN,SWindowsFwRule::PortKeywordIpTlsIn)
    PORT_CHK_ADD(FW_PORT_KEYWORD_IP_TLS_OUT,SWindowsFwRule::PortKeywordIpTlsOut) // outgoing
    
    PORT_CHK_ADD(FW_PORT_KEYWORD_DHCP,SWindowsFwRule::PortKeywordDhcp)
    PORT_CHK_ADD(FW_PORT_KEYWORD_PLAYTO_DISCOVERY,SWindowsFwRule::PortKeywordPly2Disc)
    
    PORT_CHK_ADD(FW_PORT_KEYWORD_MDNS,SWindowsFwRule::PortKeywordMDns)

    PORT_CHK_ADD(FW_PORT_KEYWORD_CORTANA_OUT,SWindowsFwRule::PortKeywordCortan) // outgoing

    PORT_CHK_ADD(FW_PORT_KEYWORD_PROXIMAL_TCP_CDP,SWindowsFwRule::PortKeywordProximalTcpCdn)

#undef PORT_CHK_ADD

    for (ULONG i = 0; i < ports->Ports.dwNumEntries; i++)
    {
        PFW_PORT_RANGE portRange = &ports->Ports.pPorts[i];
        portList.push_back(portRange->wBegin >= portRange->wEnd ? std::to_wstring(portRange->wBegin) : (std::to_wstring(portRange->wBegin) + L"-" + std::to_wstring(portRange->wEnd)));
    }

    return portList;
}

void SWindowsFirewall::MakePortsArray(PFW_PORTS ports, const std::vector<std::wstring>& portList, std::list<std::vector<BYTE>>& Buffer)
{
    ports->Ports.dwNumEntries = 0;
    ports->Ports.pPorts = NULL;
    if (portList.empty())
        return;

    USHORT wPortKeywords = FW_PORT_KEYWORD_NONE;
    std::vector<std::pair<USHORT,USHORT>> portRanges;

    for(std::wstring portStr : portList)
    {

#define PORT_CMP_ADD(s, v, k) if (_wcsicmp(portStr.c_str(), s) == 0) { if (uApiVersion > v) wPortKeywords |= k; continue; }

        PORT_CMP_ADD(SWindowsFwRule::PortKeywordRpc,0,FW_PORT_KEYWORD_DYNAMIC_RPC_PORTS)
        PORT_CMP_ADD(SWindowsFwRule::PortKeywordRpcEp,0,FW_PORT_KEYWORD_RPC_EP)
        PORT_CMP_ADD(SWindowsFwRule::PortKeywordTeredo,0,FW_PORT_KEYWORD_TEREDO_PORT)
        PORT_CMP_ADD(SWindowsFwRule::PortKeywordIpTlsIn,0,FW_PORT_KEYWORD_IP_TLS_IN)
        PORT_CMP_ADD(SWindowsFwRule::PortKeywordIpTlsOut,0,FW_PORT_KEYWORD_IP_TLS_OUT) // outgoing

        PORT_CMP_ADD(SWindowsFwRule::PortKeywordDhcp,FW_WIN8_1_BINARY_VERSION,FW_PORT_KEYWORD_DHCP)
        PORT_CMP_ADD(SWindowsFwRule::PortKeywordPly2Disc,FW_WIN8_1_BINARY_VERSION,FW_PORT_KEYWORD_PLAYTO_DISCOVERY)

        PORT_CMP_ADD(SWindowsFwRule::PortKeywordMDns,FW_WIN10_BINARY_VERSION,FW_PORT_KEYWORD_MDNS)

        PORT_CMP_ADD(SWindowsFwRule::PortKeywordCortan,FW_REDSTONE1_BINARY_VERSION,FW_PORT_KEYWORD_CORTANA_OUT) // outgoing

        PORT_CMP_ADD(SWindowsFwRule::PortKeywordProximalTcpCdn,FW_19Hx_BINARY_VERSION,FW_PORT_KEYWORD_PROXIMAL_TCP_CDP)

#undef PORT_CMP_ADD

        std::vector<std::wstring> portRange = SplitStr(portStr, L"-");
        if (portRange.size() == 2)
            portRanges.push_back(std::make_pair((USHORT)_wtoi(portRange[0].c_str()), (USHORT)_wtoi(portRange[1].c_str())));
        else if (USHORT port = (USHORT)_wtoi(portRange[0].c_str()))
            portRanges.push_back(std::make_pair(port, port));
    }

    ports->wPortKeywords = wPortKeywords;

    if (portRanges.size() > 0)
    {
        Buffer.push_back(std::vector<BYTE>(portRanges.size() * sizeof(FW_PORT_RANGE)));
        ports->Ports.dwNumEntries = (ULONG)portRanges.size();
        ports->Ports.pPorts = (PFW_PORT_RANGE)Buffer.back().data();
        for (SIZE_T i = 0; i < portRanges.size(); i++)
        {
            ports->Ports.pPorts[i].wBegin = portRanges[i].first;
            ports->Ports.pPorts[i].wEnd = portRanges[i].second;
        }
    }
}

std::vector<std::wstring> SWindowsFirewall::GetAddressesStrings(const FW_ADDRESSES* addresses)
{
    std::vector<std::wstring> addressList;

#define ADDR_CHK_ADD(k, s) if((addresses->dwV4AddressKeywords & k) != 0 || (addresses->dwV6AddressKeywords & k) != 0) addressList.push_back(s);

    ADDR_CHK_ADD(FW_ADDRESS_KEYWORD_LOCAL_SUBNET,SWindowsFwRule::AddrKeywordLocalSubnet)
    ADDR_CHK_ADD(FW_ADDRESS_KEYWORD_DNS,SWindowsFwRule::AddrKeywordDNS)
    ADDR_CHK_ADD(FW_ADDRESS_KEYWORD_DHCP,SWindowsFwRule::AddrKeywordDHCP)
    ADDR_CHK_ADD(FW_ADDRESS_KEYWORD_WINS,SWindowsFwRule::AddrKeywordWINS)
    ADDR_CHK_ADD(FW_ADDRESS_KEYWORD_DEFAULT_GATEWAY,SWindowsFwRule::AddrKeywordDefaultGateway)

    ADDR_CHK_ADD(FW_ADDRESS_KEYWORD_INTRANET,SWindowsFwRule::AddrKeywordIntrAnet)
    ADDR_CHK_ADD(FW_ADDRESS_KEYWORD_INTERNET,SWindowsFwRule::AddrKeywordIntErnet)
    ADDR_CHK_ADD(FW_ADDRESS_KEYWORD_PLAYTO_RENDERERS,SWindowsFwRule::AddrKeywordPly2Renders)
    ADDR_CHK_ADD(FW_ADDRESS_KEYWORD_REMOTE_INTRANET,SWindowsFwRule::AddrKeywordRmtIntrAnet)

    ADDR_CHK_ADD(FW_ADDRESS_KEYWORD_CAPTIVE_PORTAL,SWindowsFwRule::AddrKeywordCaptivePortal)
    
#undef ADDR_CHK_ADD

    for (DWORD i = 0; i < addresses->V4SubNets.dwNumEntries; i++)
    {
        PFW_IPV4_SUBNET subNet = &addresses->V4SubNets.pSubNets[i];
        CAddress IP(subNet->dwAddress);
        if (SubNetToInt(subNet->dwSubNetMask) == 32)
            addressList.push_back(IP.ToWString());
        else
            addressList.push_back(IP.ToWString() + L"/" + std::to_wstring(SubNetToInt(subNet->dwSubNetMask)));
    }

    for (DWORD i = 0; i < addresses->V4Ranges.dwNumEntries; i++)
    {
        PFW_IPV4_ADDRESS_RANGE ipRange = &addresses->V4Ranges.pRanges[i];
        CAddress IPb(ipRange->dwBegin);
        CAddress IPe(ipRange->dwEnd);
        if (ipRange->dwBegin == ipRange->dwEnd)
            addressList.push_back(IPb.ToWString());
        else
            addressList.push_back(IPb.ToWString() + L"-" + IPe.ToWString());
    }

    for (DWORD i = 0; i < addresses->V6SubNets.dwNumEntries; i++)
    {
        PFW_IPV6_SUBNET subNet = &addresses->V6SubNets.pSubNets[i];
        CAddress IP(subNet->Address);
        if (subNet->dwNumPrefixBits == 128)
            addressList.push_back(IP.ToWString());
        else
            addressList.push_back(IP.ToWString() + L"/" + std::to_wstring(subNet->dwNumPrefixBits));
    }

    for (DWORD i = 0; i < addresses->V6Ranges.dwNumEntries; i++)
    {
        PFW_IPV6_ADDRESS_RANGE ipRange = &addresses->V6Ranges.pRanges[i];
        CAddress IPb(ipRange->Begin);
        CAddress IPe(ipRange->End);
        if (memcmp(ipRange->Begin, ipRange->End, 16) == 0)
            addressList.push_back(IPb.ToWString());
        else
            addressList.push_back(IPb.ToWString() + L"-" + IPe.ToWString());
    }

    return addressList;
}

void SWindowsFirewall::MakeAddressList(PFW_ADDRESSES addresses, const std::vector<std::wstring>& addressList, std::list<std::vector<BYTE>>& Buffer)
{
    addresses->V4SubNets.dwNumEntries = 0;
    addresses->V4SubNets.pSubNets = NULL;
    addresses->V4Ranges.dwNumEntries = 0;
    addresses->V4Ranges.pRanges = NULL;
    addresses->V6SubNets.dwNumEntries = 0;
    addresses->V6SubNets.pSubNets = NULL;
    addresses->V6Ranges.dwNumEntries = 0;
    addresses->V6Ranges.pRanges = NULL;
    if (addressList.empty())
        return;

    DWORD dwAddressKeywords = FW_ADDRESS_KEYWORD_NONE;

    std::vector<std::pair<IN_ADDR, ULONG>> ipSubNets;
    std::vector<std::pair<IN_ADDR, IN_ADDR>> ipRanges;
    std::vector<std::pair<IN6_ADDR, USHORT>> ipSubNetsV6;
    std::vector<std::pair<IN6_ADDR, IN6_ADDR>> ipRangesV6;

    IN_ADDR ipv4_0, ipv4_1;
    IN6_ADDR ipv6_0, ipv6_1;

    for (std::wstring addressStr : addressList)
    {
#define ADDR_CMP_ADD(s, v, k) if (_wcsicmp(addressStr.c_str(), s) == 0) { if (uApiVersion > v) dwAddressKeywords |= k; continue; }

        ADDR_CMP_ADD(SWindowsFwRule::AddrKeywordLocalSubnet,0,FW_ADDRESS_KEYWORD_LOCAL_SUBNET)
        ADDR_CMP_ADD(SWindowsFwRule::AddrKeywordDNS,0,FW_ADDRESS_KEYWORD_DNS)
        ADDR_CMP_ADD(SWindowsFwRule::AddrKeywordDHCP,0,FW_ADDRESS_KEYWORD_DHCP)
        ADDR_CMP_ADD(SWindowsFwRule::AddrKeywordWINS,0,FW_ADDRESS_KEYWORD_WINS)
        ADDR_CMP_ADD(SWindowsFwRule::AddrKeywordDefaultGateway,0,FW_ADDRESS_KEYWORD_DEFAULT_GATEWAY)

        ADDR_CMP_ADD(SWindowsFwRule::AddrKeywordIntrAnet,FW_WIN8_1_BINARY_VERSION,FW_ADDRESS_KEYWORD_INTRANET)
        ADDR_CMP_ADD(SWindowsFwRule::AddrKeywordIntErnet,FW_WIN8_1_BINARY_VERSION,FW_ADDRESS_KEYWORD_INTERNET)
        ADDR_CMP_ADD(SWindowsFwRule::AddrKeywordPly2Renders,FW_WIN8_1_BINARY_VERSION,FW_ADDRESS_KEYWORD_PLAYTO_RENDERERS)
        ADDR_CMP_ADD(SWindowsFwRule::AddrKeywordRmtIntrAnet,FW_WIN8_1_BINARY_VERSION,FW_ADDRESS_KEYWORD_REMOTE_INTRANET)

        ADDR_CMP_ADD(SWindowsFwRule::AddrKeywordCaptivePortal,FW_19Hx_BINARY_VERSION,FW_ADDRESS_KEYWORD_CAPTIVE_PORTAL)

#undef ADDR_CMP_ADD

        std::vector<std::wstring> addrRange = SplitStr(addressStr, L"-");
        if (addrRange.size() == 2)
        {
            //  v4Range
            if (InetPtonW(AF_INET, addrRange[0].c_str(), &ipv4_0) == 1 && InetPtonW(AF_INET, addrRange[1].c_str(), &ipv4_1) == 1)
            {
                auto ipV4Range = std::make_pair(ipv4_0, ipv4_1);
                if (TestV4RangeOrder(ipV4Range.first, ipV4Range.second))
                    ipRanges.push_back(ipV4Range);
            }

            //  v6Range
            else if (InetPtonW(AF_INET6, addrRange[0].c_str(), &ipv6_0) == 1 && InetPtonW(AF_INET6, addrRange[1].c_str(), &ipv6_1) == 1)
            {
                auto ipV6Range = std::make_pair(ipv6_0, ipv6_1);
                if (TestV6RangeOrder(ipV6Range.first, ipV6Range.second))
                    ipRangesV6.push_back(ipV6Range);
            }
        }
        else
        {
            std::vector<std::wstring> addrSubNet = SplitStr(addressStr, L"/");
            if (addrSubNet.size() == 2)
            {
                // v4SubNet
                if (InetPtonW(AF_INET, addrSubNet[0].c_str(), &ipv4_0) == 1)
                {
                    USHORT subnet = (USHORT)_wtoi(addrSubNet[1].c_str());
                    if (subnet <= 32)
                        ipSubNets.push_back(std::make_pair(ipv4_0, IntSubNet(subnet)));
                }

                // v6SubNet
                else if (InetPtonW(AF_INET6, addrSubNet[0].c_str(), &ipv6_0) == 1)
                {
                    USHORT subnet = (USHORT)_wtoi(addrSubNet[1].c_str());
                    if (subnet <= 128)
                        ipSubNetsV6.push_back(std::make_pair(ipv6_0, subnet));
                }
            }
            else
            {
                // IPv4
                if (InetPtonW(AF_INET, addrSubNet[0].c_str(), &ipv4_0) == 1)
                {
                    ipSubNets.push_back(std::make_pair(ipv4_0, 0xFFFFFFFF));
                }

                // IPv6
                else if (InetPtonW(AF_INET6, addrSubNet[0].c_str(), &ipv6_0) == 1)
                {
                    ipSubNetsV6.push_back(std::make_pair(ipv6_0, 128));
                }
            }
        }
    }

    addresses->dwV4AddressKeywords = addresses->dwV6AddressKeywords = dwAddressKeywords;

    if (ipSubNets.size() > 0)
    {
        Buffer.push_back(std::vector<BYTE>(ipSubNets.size() * sizeof(FW_IPV4_SUBNET)));
        addresses->V4SubNets.dwNumEntries = (ULONG)ipSubNets.size();
        addresses->V4SubNets.pSubNets = (PFW_IPV4_SUBNET)Buffer.back().data();
        for (SIZE_T i = 0; i < ipSubNets.size(); i++)
        {
            addresses->V4SubNets.pSubNets[i].dwAddress = ntohl(ipSubNets[i].first.S_un.S_addr);
            addresses->V4SubNets.pSubNets[i].dwSubNetMask = ipSubNets[i].second;
        }
    }

    if (ipRanges.size() > 0)
    {
        Buffer.push_back(std::vector<BYTE>(ipRanges.size() * sizeof(FW_IPV4_ADDRESS_RANGE)));
        addresses->V4Ranges.dwNumEntries = (ULONG)ipRanges.size();
        addresses->V4Ranges.pRanges = (PFW_IPV4_ADDRESS_RANGE)Buffer.back().data();
        for (SIZE_T i = 0; i < ipRanges.size(); i++)
        {
            addresses->V4Ranges.pRanges[i].dwBegin = ntohl(ipRanges[i].first.S_un.S_addr);
            addresses->V4Ranges.pRanges[i].dwBegin = ntohl(ipRanges[i].second.S_un.S_addr);
        }
    }

    if (ipSubNetsV6.size() > 0)
    {
        Buffer.push_back(std::vector<BYTE>(ipSubNetsV6.size() * sizeof(FW_IPV6_SUBNET)));
        addresses->V6SubNets.dwNumEntries = (ULONG)ipSubNetsV6.size();
        addresses->V6SubNets.pSubNets = (PFW_IPV6_SUBNET)Buffer.back().data();
        for (SIZE_T i = 0; i < ipSubNetsV6.size(); i++)
        {
            memcpy(addresses->V6SubNets.pSubNets[i].Address, ipSubNetsV6[i].first.u.Byte, 16);
            addresses->V6SubNets.pSubNets[i].dwNumPrefixBits = ipSubNetsV6[i].second;
        }
    }

    if (ipRangesV6.size() > 0)
    {
        Buffer.push_back(std::vector<BYTE>(ipRangesV6.size() * sizeof(FW_IPV6_ADDRESS_RANGE)));
        addresses->V6Ranges.dwNumEntries = (ULONG)ipRangesV6.size();
        addresses->V6Ranges.pRanges = (PFW_IPV6_ADDRESS_RANGE)Buffer.back().data();
        for (SIZE_T i = 0; i < ipRangesV6.size(); i++)
        {
            memcpy(addresses->V6Ranges.pRanges[i].Begin, ipRangesV6[i].first.u.Byte, 16);
            memcpy(addresses->V6Ranges.pRanges[i].End, ipRangesV6[i].second.u.Byte, 16);
        }
    }
}

std::vector<SWindowsFwRule::IcmpTypeCode> SWindowsFirewall::GetIcmpTypesAndCodes(const FW_ICMP_TYPE_CODE_LIST* TypeCodeList)
{
    std::vector<SWindowsFwRule::IcmpTypeCode> list;
    if (TypeCodeList->dwNumEntries == 0)
        return list;

    list.resize(TypeCodeList->dwNumEntries);
    for (ULONG i = 0; i < TypeCodeList->dwNumEntries; i++)
    {
        list[i].Type = TypeCodeList->pEntries[i].bType;
        list[i].Code = TypeCodeList->pEntries[i].wCode;
    }

    return list;
}

void SWindowsFirewall::MakeIcmpTypesAndCodes(PFW_ICMP_TYPE_CODE_LIST TypeCodeList, const std::vector<SWindowsFwRule::IcmpTypeCode>& list, std::list<std::vector<BYTE>>& Buffer)
{
    Buffer.push_back(std::vector<BYTE>(list.size() * sizeof(FW_ICMP_TYPE_CODE)));
    TypeCodeList->dwNumEntries = (ULONG)list.size();
    TypeCodeList->pEntries = (FW_ICMP_TYPE_CODE*)Buffer.back().data();
    for (DWORD i = 0; i < list.size(); i++) {
        TypeCodeList->pEntries[i].bType = list[i].Type;
        TypeCodeList->pEntries[i].wCode = list[i].Code;
    }
}

std::vector<std::wstring> SWindowsFirewall::GetPlatformValidity(const FW_OS_PLATFORM_LIST* platformList)
{
    std::vector<std::wstring> list;
    if (platformList->dwNumEntries == 0)
        return list;

    list.resize(platformList->dwNumEntries);
    for (ULONG i = 0; i < platformList->dwNumEntries; i++)
    {
        list[i] = std::to_wstring(platformList->pPlatforms[i].bPlatform) + L"." + std::to_wstring(platformList->pPlatforms[i].bMajorVersion) + L"." + std::to_wstring(platformList->pPlatforms[i].bMinorVersion);
        if(platformList->pPlatforms[i].Reserved)
            list[i] += L"." + std::to_wstring(platformList->pPlatforms[i].Reserved);
    }

    return list;
}

void SWindowsFirewall::MakePlatformValidity(FW_OS_PLATFORM_LIST* platformList, std::vector<std::wstring> list, std::list<std::vector<BYTE>>& Buffer)
{
    Buffer.push_back(std::vector<BYTE>(list.size() * sizeof(FW_OS_PLATFORM)));
    platformList->dwNumEntries = (ULONG)list.size();
    platformList->pPlatforms = (FW_OS_PLATFORM*)Buffer.back().data();
    for (SIZE_T i = 0; i < list.size(); i++)
    {
        std::vector<std::wstring> portList = SplitStr(list[i], L".");
        platformList->pPlatforms[i].bPlatform = (UCHAR)_wtoi(portList[0].c_str());
        if(portList.size() > 1) platformList->pPlatforms[i].bMajorVersion = (UCHAR)_wtoi(portList[1].c_str());
        if(portList.size() > 2) platformList->pPlatforms[i].bMinorVersion = (UCHAR)_wtoi(portList[2].c_str());
        if(portList.size() > 3) platformList->pPlatforms[i].Reserved = (UCHAR)_wtoi(portList[3].c_str());
    }
}

SWindowsFwRulePtr SWindowsFirewall::LoadRule(const FW_RULE* fwRule)
{
    SWindowsFwRulePtr pRule = std::make_shared<SWindowsFwRule>();

#define SAFE_GET_STR(x) (x ? x : std::wstring())

    pRule->guid = SAFE_GET_STR(fwRule->wszRuleId);

    pRule->BinaryPath = SAFE_GET_STR(fwRule->wszLocalApplication);
    pRule->ServiceTag = SAFE_GET_STR(fwRule->wszLocalService);
    if(fwRule->wSchemaVersion >= FW_WIN8_1_BINARY_VERSION)
        pRule->AppContainerSid = SAFE_GET_STR(fwRule->wszPackageId);

    pRule->Name = SAFE_GET_STR(fwRule->wszName);
    pRule->Description = SAFE_GET_STR(fwRule->wszDescription);
    pRule->Grouping = SAFE_GET_STR(fwRule->wszEmbeddedContext);

#undef SAFE_GET_STR

    pRule->Enabled = (fwRule->wFlags & FW_RULE_FLAGS_ACTIVE);

    switch (fwRule->Direction)
    {
        case FW_DIR_IN: pRule->Direction = EFwDirections::Inbound; break;
        case FW_DIR_OUT: pRule->Direction = EFwDirections::Outbound; break;
        default:
            break;
    }

    switch (fwRule->Action)
    {
        case FW_RULE_ACTION_ALLOW: pRule->Action = EFwActions::Allow; break;
        case FW_RULE_ACTION_BLOCK: pRule->Action = EFwActions::Block; break;
        default:
            break;
    }

    pRule->Profile = fwRule->dwProfiles;

    //FW_INTERFACE_LUIDS LocalInterfaceIds;

    pRule->Interface = fwRule->dwLocalInterfaceTypes;

    pRule->Protocol = (EFwKnownProtocols)fwRule->wIpProtocol;
    switch (pRule->Protocol)
    {
        case EFwKnownProtocols::ICMP:
            pRule->IcmpTypesAndCodes = GetIcmpTypesAndCodes(&fwRule->V4TypeCodeList);
            break;
        case EFwKnownProtocols::ICMPv6:
            pRule->IcmpTypesAndCodes = GetIcmpTypesAndCodes(&fwRule->V6TypeCodeList);
            break;
        case EFwKnownProtocols::TCP:
        case EFwKnownProtocols::UDP:
            pRule->LocalPorts = GetPortsStrings(&fwRule->LocalPorts);
            pRule->RemotePorts = GetPortsStrings(&fwRule->RemotePorts);
            break;
    }

    pRule->LocalAddresses = GetAddressesStrings(&fwRule->LocalAddresses);
    pRule->RemoteAddresses = GetAddressesStrings(&fwRule->RemoteAddresses);

    if (fwRule->wFlags & FW_RULE_FLAGS_ROUTEABLE_ADDRS_TRAVERSE)
        pRule->EdgeTraversal = NET_FW_EDGE_TRAVERSAL_TYPE_ALLOW;
    else if (fwRule->wFlags & FW_RULE_FLAGS_ROUTEABLE_ADDRS_TRAVERSE_DEFER_APP)
        pRule->EdgeTraversal = NET_FW_EDGE_TRAVERSAL_TYPE_DEFER_TO_APP;
    else if (fwRule->wFlags & FW_RULE_FLAGS_ROUTEABLE_ADDRS_TRAVERSE_DEFER_USER)
        pRule->EdgeTraversal = NET_FW_EDGE_TRAVERSAL_TYPE_DEFER_TO_USER;
    else
        pRule->EdgeTraversal = 0;

    //if(fwRule->wFlags & FW_RULE_FLAGS_LOOSE_SOURCE_MAPPED)

    //if(fwRule->wFlags & FW_RULE_FLAGS_AUTHENTICATE)
    //if(fwRule->wFlags & FW_RULE_FLAGS_AUTH_WITH_NO_ENCAPSULATION)
    //if(fwRule->wFlags & FW_RULE_FLAGS_AUTHENTICATE_WITH_ENCRYPTION)
    //if(fwRule->wFlags & FW_RULE_FLAGS_AUTH_WITH_ENC_NEGOTIATE)
    //if(fwRule->wFlags & FW_RULE_FLAGS_AUTHENTICATE_BYPASS_OUTBOUND)


    //string RemoteMachineAuthorizationList;
    //string RemoteUserAuthorizationList;
    pRule->OsPlatformValidity = GetPlatformValidity(&fwRule->PlatformValidityList);
    //pRule->Status = (uint)fwRule->Status;
    //FW_RULE_ORIGIN_TYPE Origin;
    //string GPOName;
    //uint Reserved;

    //if (fwRule->wSchemaVersion < (ushort)FW_BINARY_VERSION.FW_BINARY_VERSION_WIN8)
    //    return true;
    //if(fwRule->wFlags & FW_RULE_FLAGS_LOCAL_ONLY_MAPPED)
    //IntPtr pMetaData;
    //string LocalUserAuthorizationList;
    //string LocalUserOwner;
    //FW_TRUST_TUPLE_KEYWORD dwTrustTupleKeywords;



    //if (fwRule->wSchemaVersion < (ushort)FW_BINARY_VERSION.FW_BINARY_VERSION_THRESHOLD) // 1507
    //    return true;
    //if(fwRule->wFlags & FW_RULE_FLAGS_LUA_CONDITIONAL_ACE)
    //FW_NETWORK_NAMES OnNetworkNames;
    //string SecurityRealmId;

    //if (fwRule->wSchemaVersion < (ushort)FW_BINARY_VERSION.FW_BINARY_VERSION_THRESHOLD2) // 1511
    //    return true;
    //FW_RULE_FLAGS2 wFlags2;

    //if (fwRule->wSchemaVersion < (ushort)FW_BINARY_VERSION.FW_BINARY_VERSION_REDSTONE1) // 1607
    //    return true;
    //FW_NETWORK_NAMES RemoteOutServerNames;

    //if (fwRule->wSchemaVersion < (ushort)FW_BINARY_VERSION.FW_BINARY_VERSION_170x) // 1703
    //    return true;
    //string Fqbn;
    //uint compartmentId;

    return pRule;
}

FW_RULE* SWindowsFirewall::SaveRule(const SWindowsFwRulePtr& pRule, std::list<std::vector<BYTE>>& Buffer)
{
    Buffer.push_back(std::vector<BYTE>(sizeof(FW_RULE)));
    FW_RULE* fwRule = (FW_RULE*)Buffer.back().data();

    fwRule->pNext = NULL;
    fwRule->wSchemaVersion = uApiVersion;

#define SAFE_SET_STR(x) (x.empty() ? NULL : (WCHAR*)(x).c_str())

    fwRule->wszRuleId = SAFE_SET_STR(pRule->guid);

    fwRule->wszLocalApplication = SAFE_SET_STR(pRule->BinaryPath);
    fwRule->wszLocalService = SAFE_SET_STR(pRule->ServiceTag);
    if (fwRule->wSchemaVersion >= FW_WIN8_1_BINARY_VERSION)
        fwRule->wszPackageId = SAFE_SET_STR(pRule->AppContainerSid);

    fwRule->wszName = SAFE_SET_STR(pRule->Name);
    fwRule->wszDescription = SAFE_SET_STR(pRule->Description);
    fwRule->wszEmbeddedContext = SAFE_SET_STR(pRule->Grouping);

#undef SAFE_SET_STR

    fwRule->wFlags = FW_RULE_FLAGS_NONE;
    if (pRule->Enabled) *(ULONG*)&fwRule->wFlags |= FW_RULE_FLAGS_ACTIVE;

    switch (pRule->Direction)
    {
        case EFwDirections::Inbound: fwRule->Direction = FW_DIR_IN; break;
        case EFwDirections::Outbound: fwRule->Direction = FW_DIR_OUT; break;
    }

    switch (pRule->Action)
    {
        case EFwActions::Allow: fwRule->Action = FW_RULE_ACTION_ALLOW; break;
        case EFwActions::Block: fwRule->Action = FW_RULE_ACTION_BLOCK; break;
    }

    fwRule->dwProfiles = (FW_PROFILE_TYPE)pRule->Profile;

    fwRule->LocalInterfaceIds.dwNumLUIDs = 0;
    fwRule->LocalInterfaceIds.pLUIDs = NULL;
                
    fwRule->dwLocalInterfaceTypes = (FW_INTERFACE_TYPE)pRule->Interface;

    fwRule->wIpProtocol = (USHORT)pRule->Protocol;
    switch (pRule->Protocol)
    {
        case EFwKnownProtocols::ICMP:
            MakeIcmpTypesAndCodes(&fwRule->V4TypeCodeList, pRule->IcmpTypesAndCodes, Buffer);
            break;
        case EFwKnownProtocols::ICMPv6:
            MakeIcmpTypesAndCodes(&fwRule->V6TypeCodeList, pRule->IcmpTypesAndCodes, Buffer);
            break;
        case EFwKnownProtocols::TCP:
        case EFwKnownProtocols::UDP:
            MakePortsArray(&fwRule->LocalPorts, pRule->LocalPorts, Buffer);
            MakePortsArray(&fwRule->RemotePorts, pRule->RemotePorts, Buffer);
            break;
    }

    MakeAddressList(&fwRule->LocalAddresses, pRule->LocalAddresses, Buffer);
    MakeAddressList(&fwRule->RemoteAddresses, pRule->RemoteAddresses, Buffer);

    if (pRule->EdgeTraversal == NET_FW_EDGE_TRAVERSAL_TYPE_ALLOW) *(ULONG*)&fwRule->wFlags |= FW_RULE_FLAGS_ROUTEABLE_ADDRS_TRAVERSE;
    if (pRule->EdgeTraversal == NET_FW_EDGE_TRAVERSAL_TYPE_DEFER_TO_APP) *(ULONG*)&fwRule->wFlags |= FW_RULE_FLAGS_ROUTEABLE_ADDRS_TRAVERSE_DEFER_APP;
    if (pRule->EdgeTraversal == NET_FW_EDGE_TRAVERSAL_TYPE_DEFER_TO_USER) *(ULONG*)&fwRule->wFlags |= FW_RULE_FLAGS_ROUTEABLE_ADDRS_TRAVERSE_DEFER_USER;

    //fwRule->wFlags

    fwRule->wszRemoteMachineAuthorizationList = NULL;
    fwRule->wszRemoteUserAuthorizationList = NULL;

    if (fwRule->wszPackageId && *fwRule->wszPackageId
        /*|| (fwRule->LocalUserAuthorizationList != null && fwRule->LocalUserAuthorizationList.Length > 0)
        || (fwRule->LocalUserOwner != null && fwRule->LocalUserOwner.Length > 0)
        || fwRule->dwTrustTupleKeywords != FW_TRUST_TUPLE_KEYWORD.FW_TRUST_TUPLE_KEYWORD_NONE*/)
    {
        pRule->OsPlatformValidity.resize(1);
        pRule->OsPlatformValidity[0] = L"10.6.2";
    }
    else
        pRule->OsPlatformValidity.clear();
    MakePlatformValidity(&fwRule->PlatformValidityList, pRule->OsPlatformValidity, Buffer);

    fwRule->Status = FW_RULE_STATUS_OK;
    fwRule->Origin = FW_RULE_ORIGIN_LOCAL;
    fwRule->wszGPOName = NULL;
    fwRule->Reserved = FW_OBJECT_CTRL_FLAG_NONE;

    // Windows 8+
    if (uApiVersion < FW_WIN8_1_BINARY_VERSION)
        return fwRule;
    fwRule->pMetaData = NULL;
    fwRule->wszLocalUserAuthorizationList = NULL;
    fwRule->wszLocalUserOwner = NULL;
    fwRule->dwTrustTupleKeywords = FW_TRUST_TUPLE_KEYWORD_NONE;


    // Windows 10+
    if (uApiVersion < FW_THRESHOLD_BINARY_VERSION) // 1507
        return fwRule;
    fwRule->OnNetworkNames.dwNumEntries = 0;
    fwRule->OnNetworkNames.wszNames = NULL;
    fwRule->wszSecurityRealmId = NULL;

    if (uApiVersion < FW_THRESHOLD2_BINARY_VERSION) // 1511
        return fwRule;
    fwRule->wFlags2 = FW_RULE_FLAGS2_NONE;

    if (uApiVersion < FW_REDSTONE1_BINARY_VERSION) // 1607
        return fwRule;
    fwRule->RemoteOutServerNames.dwNumEntries = 0;
    fwRule->RemoteOutServerNames.wszNames = NULL;

    if (uApiVersion < FW_180x_BINARY_VERSION) // 1703
        return fwRule;
    fwRule->Fqbn = NULL;
    fwRule->compartmentId = 0;

    // Windows 11+
    if (uApiVersion < FW_21H2_BINARY_VERSION) // Server 2022
        return fwRule;

    memset(&fwRule->providerContextKey, 0, sizeof(GUID));
    fwRule->RemoteDynamicKeywordAddresses.dwNumIds = 0;
    fwRule->RemoteDynamicKeywordAddresses.ids = NULL;

    return fwRule;
}

ULONG SWindowsFirewall::GetFWConfig(FW_PROFILE_TYPE profile, FW_PROFILE_CONFIG conf)
{
    CSectionLock Lock(&cPolicyLock);

    FW_CONFIG_FLAGS dwFlags = FW_CONFIG_FLAG_RETURN_DEFAULT_IF_NOT_FOUND;
    ULONG value = 0;
    ULONG valueSize = sizeof(value);
    if (pFWGetConfig(hPolicyHandle, conf, profile, dwFlags, (PCHAR)&value, &valueSize) != ERROR_SUCCESS)
        return -1;
    return value;
}

ULONG SWindowsFirewall::SetFWConfig(FW_PROFILE_TYPE profile, FW_PROFILE_CONFIG conf, ULONG value)
{
    CSectionLock Lock(&cPolicyLock);

    return pFWSetConfig(hPolicyHandle, conf, profile, *(FW_PROFILE_CONFIG_VALUE*)&value, sizeof(value));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CWindowsFirewall

CWindowsFirewall::CWindowsFirewall()
{
    m = new SWindowsFirewall;

    InitializeCriticalSection(&m->cPolicyLock);

    // todo check error
    if (m->pFWOpenPolicyStore) {
        HRESULT res = m->pFWOpenPolicyStore(m->uApiVersion, NULL, FW_STORE_TYPE_LOCAL, FW_POLICY_ACCESS_RIGHT_READ_WRITE, FW_POLICY_STORE_FLAGS_NONE, &m->hPolicyHandle);
        if(res != ERROR_SUCCESS)
            m->hPolicyHandle = NULL; // failed
    }
}

CWindowsFirewall::~CWindowsFirewall()
{
    DeleteCriticalSection(&m->cPolicyLock);

    if (m->hPolicyHandle && m->pFWClosePolicyStore)
        m->pFWClosePolicyStore(&m->hPolicyHandle);
}

CWindowsFirewall* CWindowsFirewall::Instance()
{
    //static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static CWindowsFirewall* instance = NULL;

    //if (PhBeginInitOnce(&initOnce))
    if (!instance) // todo
    {
        instance = new CWindowsFirewall();

    //    PhEndInitOnce(&initOnce);
    }

    return instance;
}

bool CWindowsFirewall::IsReady()
{
    return m->hPolicyHandle != NULL;
}

RESULT(std::shared_ptr<SWindowsFwRuleList>) CWindowsFirewall::LoadRules()
{
    if (!m->pFWEnumFirewallRules || !m->pFWFreeFirewallRules)
        return ERR(STATUS_ENTRYPOINT_NOT_FOUND);

    auto list = std::make_shared<SWindowsFwRuleList>();

    FW_ENUM_RULES_FLAGS wFlags = FW_ENUM_RULES_FLAG_NONE;
    //wFlags |= FW_ENUM_RULES_FLAG_RESOLVE_NAME | FW_ENUM_RULES_FLAG_RESOLVE_DESCRIPTION;
    //wFlags |= FW_ENUM_RULES_FLAG_RESOLVE_APPLICATION;
    //wFlags |= FW_ENUM_RULES_FLAG_RESOLVE_KEYWORD; // for testing only dynamic_store

    CSectionLock Lock(&m->cPolicyLock);
    
    m_FwRules.clear();

    PFW_RULE pRules = NULL;
    ULONG uRuleCount = 0;
    ULONG result = m->pFWEnumFirewallRules(m->hPolicyHandle, FW_RULE_STATUS_CLASS_ALL, FW_PROFILE_TYPE_ALL, wFlags, &uRuleCount, &pRules);

    if (result == ERROR_SUCCESS && pRules && uRuleCount > 0)
    {
        for (PFW_RULE fwRule = pRules; fwRule != NULL; fwRule = fwRule->pNext)
        {
            SWindowsFwRulePtr pRule = m->LoadRule(fwRule);
            if (pRule) {
                list->push_front(pRule);
                m_FwRules[pRule->guid] = pRule;
                pRule->Index = (int)(uRuleCount - m_FwRules.size());
            }
        }
    }

    if (pRules)
        m->pFWFreeFirewallRules(pRules);

    RETURN(list);
}
    
RESULT(std::shared_ptr<SWindowsFwRuleMap>) CWindowsFirewall::LoadRules(const std::vector<std::wstring>& RuleIds)
{
    if (!m->pFWQueryFirewallRules || !m->pFWFreeFirewallRules)
        return ERR(STATUS_ENTRYPOINT_NOT_FOUND);
    
    FW_QUERY ruleQuery;
    ruleQuery.dwNumEntries = (ULONG)RuleIds.size();
    ruleQuery.Status = FW_RULE_STATUS_ALL;
    ruleQuery.wSchemaVersion = m->uApiVersion;

    std::vector<FW_QUERY_CONDITION>fwQueryCondition;
    fwQueryCondition.resize(ruleQuery.dwNumEntries);

    std::vector<FW_QUERY_CONDITIONS>fwQueryConditionList;
    fwQueryConditionList.resize(ruleQuery.dwNumEntries);

    for(DWORD i = 0; i < RuleIds.size(); i++)
    {
        fwQueryCondition[i].matchType = FW_MATCH_TYPE_TRAFFIC_MATCH;
        fwQueryCondition[i].matchKey = FW_MATCH_KEY_OBJECTID;
        fwQueryCondition[i].matchValue.type = FW_DATA_TYPE_UNICODE_STRING;
        fwQueryCondition[i].matchValue.wszString = (WCHAR*)RuleIds[i].c_str();

        fwQueryConditionList[i].dwNumEntries = 1;
        fwQueryConditionList[i].AndedConditions = &fwQueryCondition[i];
    }

    ruleQuery.ORConditions = fwQueryConditionList.data();

    FW_ENUM_RULES_FLAGS wFlags = FW_ENUM_RULES_FLAG_NONE;
    //wFlags |= FW_ENUM_RULES_FLAG_RESOLVE_NAME | FW_ENUM_RULES_FLAG_RESOLVE_DESCRIPTION;
    //wFlags |= FW_ENUM_RULES_FLAG_RESOLVE_APPLICATION;

    auto map = std::make_shared<SWindowsFwRuleMap>();

    CSectionLock Lock(&m->cPolicyLock);

    PFW_RULE pRules = NULL;
    ULONG uRuleCount = 0;
    ULONG result = m->pFWQueryFirewallRules(m->hPolicyHandle, &ruleQuery, wFlags, &uRuleCount, &pRules);

    if (result == ERROR_SUCCESS && pRules && uRuleCount > 0)
    {
        for (PFW_RULE fwRule = pRules; fwRule != NULL; fwRule = fwRule->pNext)
        {
            SWindowsFwRulePtr pRule = m->LoadRule(fwRule);
            if (pRule)
                map->insert(std::make_pair(pRule->guid, pRule));
        }
    }

    if (pRules)
        m->pFWFreeFirewallRules(pRules);

    // update rule cache with most up to date values
    for(DWORD i = 0; i < RuleIds.size(); i++)
    {
        auto New = map->find(RuleIds[i]);
        if (New != map->end())
        {
            auto Old = m_FwRules.find(RuleIds[i]);
            if (Old == m_FwRules.end())
            {
                New->second->Index = (int)m_FwRules.size();
                m_FwRules[RuleIds[i]] = New->second; // add new rule
            }
            else
            {
                New->second->Index = Old->second->Index;
                Old->second = New->second; // update cahed rule
            }
        }
        else
            m_FwRules.erase(RuleIds[i]);
    }

    RETURN(map);
}

SWindowsFwRulePtr CWindowsFirewall::GetRule(const std::wstring RuleId) const
{
    CSectionLock Lock(&m->cPolicyLock);
    auto F = m_FwRules.find(RuleId);
    if (F == m_FwRules.end())
        return SWindowsFwRulePtr();
    return F->second;
}

SWindowsFwRuleMap CWindowsFirewall::GetAllRules() const
{
    CSectionLock Lock(&m->cPolicyLock);
    return m_FwRules;
}

SWindowsFwRuleMap CWindowsFirewall::GetRules(const std::vector<std::wstring>& RuleIds) const
{
    SWindowsFwRuleMap map;

    CSectionLock Lock(&m->cPolicyLock);

    for (DWORD i = 0; i < RuleIds.size(); i++)
    {
        auto F = m_FwRules.find(RuleIds[i]);
        if (F != m_FwRules.end())
            map.insert(std::make_pair(RuleIds[i], F->second));
    }

    return map;
}

STATUS CWindowsFirewall::UpdateRule(const SWindowsFwRulePtr& pRule)
{
    CSectionLock Lock(&m->cPolicyLock);

    bool bAdd = false;

    auto F = pRule->guid.empty() ? m_FwRules.end() : m_FwRules.find(pRule->guid);
    if (F == m_FwRules.end())
    {
        bAdd = true;
        if (pRule->guid.empty()) {
            GUID guid;
            CoCreateGuid(&guid);
            wchar_t wszGuid[40] = { 0 };
            StringFromGUID2(guid, wszGuid, sizeof(wszGuid) / sizeof(wchar_t));
            pRule->guid = wszGuid;
        }
    }
    
    std::list<std::vector<BYTE>> Buffer;
    FW_RULE* fwRule = m->SaveRule(pRule, Buffer);
    if (!fwRule)
        return ERR(STATUS_INTERNAL_ERROR);

    ULONG result = bAdd ? ERROR_RULE_NOT_FOUND : m->pFWSetFirewallRule(m->hPolicyHandle, fwRule);
    if (result == ERROR_RULE_NOT_FOUND)
        result = m->pFWAddFirewallRule(m->hPolicyHandle, fwRule);

    if (result != ERROR_SUCCESS)
    {
        WCHAR message[0x400];
        ULONG messageLen = ARRAYSIZE(message);
        if (m->pFWStatusMessageFromStatusCode(fwRule->Status, message, &messageLen) == ERROR_SUCCESS)
            return ERR(STATUS_UNSUCCESSFUL, std::wstring(message, messageLen));
        return ERR(STATUS_UNSUCCESSFUL);
    }

    if (bAdd)
    {
        pRule->Index = (int)m_FwRules.size();
        m_FwRules[pRule->guid] = pRule;
    }
    else
        F->second = pRule;
    return OK;
}

STATUS CWindowsFirewall::RemoveRule(const std::wstring RuleId)
{
    if (!m->pFWDeleteFirewallRule)
        return ERR(STATUS_ENTRYPOINT_NOT_FOUND);

    CSectionLock Lock(&m->cPolicyLock);

    auto F = m_FwRules.find(RuleId);
    if (F == m_FwRules.end())
        return OK; // already removed

    ULONG result = m->pFWDeleteFirewallRule(m->hPolicyHandle, (WCHAR*)RuleId.c_str());
    if (result != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);

    m_FwRules.erase(F);
    return OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Settings

bool CWindowsFirewall::GetFirewallEnabled(EFwProfiles profileType)
{
    return m->GetFWConfig((FW_PROFILE_TYPE)profileType, FW_PROFILE_CONFIG_ENABLE_FW) == 1u;
}

void CWindowsFirewall::SetFirewallEnabled(EFwProfiles profileType, bool Enabled)
{
    m->SetFWConfig((FW_PROFILE_TYPE)profileType, FW_PROFILE_CONFIG_ENABLE_FW, Enabled ? 1u : 0u);
}

bool CWindowsFirewall::GetBlockAllInboundTraffic(EFwProfiles profileType)
{
    return m->GetFWConfig((FW_PROFILE_TYPE)profileType, FW_PROFILE_CONFIG_SHIELDED) == 1u;
}

void CWindowsFirewall::SetBlockAllInboundTraffic(EFwProfiles profileType, bool Block)
{
    m->SetFWConfig((FW_PROFILE_TYPE)profileType, FW_PROFILE_CONFIG_SHIELDED, Block ? 1u : 0u);
}

bool CWindowsFirewall::GetInboundEnabled(EFwProfiles profileType)
{
    return m->GetFWConfig((FW_PROFILE_TYPE)profileType, FW_PROFILE_CONFIG_DISABLE_INBOUND_NOTIFICATIONS) == 0u;
}

void CWindowsFirewall::SetInboundEnabled(EFwProfiles profileType, bool Enable)
{
    m->SetFWConfig((FW_PROFILE_TYPE)profileType, FW_PROFILE_CONFIG_DISABLE_INBOUND_NOTIFICATIONS, Enable ? 0u : 1u);
}

EFwActions CWindowsFirewall::GetDefaultInboundAction(EFwProfiles profileType)
{
    if (m->GetFWConfig((FW_PROFILE_TYPE)profileType, FW_PROFILE_CONFIG_DEFAULT_INBOUND_ACTION) == 1u)
        return EFwActions::Block;
    return EFwActions::Allow;
}

void CWindowsFirewall::SetDefaultInboundAction(EFwProfiles profileType, EFwActions Action)
{
    m->SetFWConfig((FW_PROFILE_TYPE)profileType, FW_PROFILE_CONFIG_DEFAULT_INBOUND_ACTION, Action == EFwActions::Block ? 1u : 0u);
}

EFwActions CWindowsFirewall::GetDefaultOutboundAction(EFwProfiles profileType)
{
    if (m->GetFWConfig((FW_PROFILE_TYPE)profileType, FW_PROFILE_CONFIG_DEFAULT_OUTBOUND_ACTION) == 1u)
        return EFwActions::Block;
    return EFwActions::Allow;
}

void CWindowsFirewall::SetDefaultOutboundAction(EFwProfiles profileType, EFwActions Action)
{
    m->SetFWConfig((FW_PROFILE_TYPE)profileType, FW_PROFILE_CONFIG_DEFAULT_OUTBOUND_ACTION, Action == EFwActions::Block ? 1u : 0u);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// App Package List

RESULT(std::shared_ptr<SAppContainerList>) CWindowsFirewall::EnumAppContainers()
{
    if (!m->pNetworkIsolationEnumAppContainers || !m->pNetworkIsolationFreeAppContainers)
        return ERR(STATUS_ENTRYPOINT_NOT_FOUND);

    auto list = std::make_shared<SAppContainerList>();

    DWORD dwNumAppContainers = 0;
    PINET_FIREWALL_APP_CONTAINER pAppContainers = NULL;
    if (m->pNetworkIsolationEnumAppContainers(0, &dwNumAppContainers, &pAppContainers) != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);
    if (!pAppContainers)
        RETURN(list);

    for (DWORD i = 0; i < dwNumAppContainers; i++)
    {
        PINET_FIREWALL_APP_CONTAINER Entry = &pAppContainers[i];

        SAppContainerPtr pAppContainer = std::make_shared<SAppContainer>();

        pAppContainer->ContainerSid = Entry->appContainerSid;
        pAppContainer->UserSid = Entry->userSid;
        pAppContainer->Id = Entry->appContainerName;
        pAppContainer->Name = Entry->displayName;
        pAppContainer->Description = Entry->description;

        list->push_back(pAppContainer);
    }

    m->pNetworkIsolationFreeAppContainers(pAppContainers);
    RETURN(list);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Network Lists

RESULT(std::shared_ptr<SFwNetworkList>) CWindowsFirewall::EnumFwNetworks()
{
    if (!m->pFWEnumNetworks || !m->pFWFreeNetworks)
        return ERR(STATUS_ENTRYPOINT_NOT_FOUND);

    auto list = std::make_shared<SFwNetworkList>();

    ULONG dwNumNetworks = 0;
    PFW_NETWORK pNetworks = NULL;
    if (m->pFWEnumNetworks(m->hPolicyHandle, &dwNumNetworks, &pNetworks) != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);
    if (!pNetworks)
        RETURN(list);

    for (DWORD i = 0; i < dwNumNetworks; i++)
    {
        PFW_NETWORK Entry = &pNetworks[i];

        SFwNetworkPtr pNetwork = std::make_shared<SFwNetwork>();

        pNetwork->pszName = Entry->pszName;
        pNetwork->ProfileType = (EFwProfiles)Entry->ProfileType;
        
        list->push_back(pNetwork);
    }

    m->pFWFreeNetworks(pNetworks);
    RETURN(list);
}

RESULT(std::shared_ptr<SNetAdapterList>) CWindowsFirewall::EnumNetAdapters()
{
    if (!m->pFWEnumAdapters || !m->pFWFreeAdapters)
        return ERR(STATUS_ENTRYPOINT_NOT_FOUND);

    auto list = std::make_shared<SNetAdapterList>();

    ULONG dwNumAdapters = 0;
    PFW_ADAPTER pAdapters = NULL;
    if (m->pFWEnumAdapters(m->hPolicyHandle, &dwNumAdapters, &pAdapters) != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);
    if (!pAdapters)
        RETURN(list);

    for (DWORD i = 0; i < dwNumAdapters; i++)
    {
        PFW_ADAPTER Entry = &pAdapters[i];

        SNetAdapterPtr pAdapter = std::make_shared<SNetAdapter>();

        pAdapter->pszFriendlyName = Entry->pszFriendlyName;
        wchar_t guidStr[40];
        if (StringFromGUID2(Entry->Guid, guidStr, sizeof(guidStr)) != 0)
            pAdapter->Guid = guidStr;
        
        list->push_back(pAdapter);
    }

    m->pFWFreeAdapters(pAdapters);
    RETURN(list);
}