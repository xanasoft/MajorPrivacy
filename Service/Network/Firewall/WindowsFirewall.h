#pragma once
#include "../Library/Status.h"
#include "../Library/Helpers/SID.h"
#include "../Library/Common/Address.h"
#include "FirewallDefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
// SWindowsFwRule

struct SWindowsFwRule
{
    struct IcmpTypeCode
    {
        BYTE Type;
        USHORT Code; // 0-255; 256 = any
    };

    // supported port keywords:
    static inline const wchar_t* PortKeywordRpc = L"RPC";
    static inline const wchar_t* PortKeywordRpcEp = L"RPC-EPMap";
    static inline const wchar_t* PortKeywordTeredo = L"Teredo";
    // sinve win7
    static inline const wchar_t* PortKeywordIpTlsIn = L"IPHTTPSIn";
    static inline const wchar_t* PortKeywordIpTlsOut = L"IPHTTPSOut";
    // since win8
    static inline const wchar_t* PortKeywordDhcp = L"DHCP";
    static inline const wchar_t* PortKeywordPly2Disc = L"Ply2Disc";
    // since win10
    static inline const wchar_t* PortKeywordMDns = L"mDNS";
    // since 1607
    static inline const wchar_t* PortKeywordCortan = L"Cortana";
    // since 1903
    static inline const wchar_t* PortKeywordProximalTcpCdn = L"ProximalTcpCdn"; // wtf is that?

    // supported address keywords:
    static inline const wchar_t* AddrKeywordLocalSubnet = L"LocalSubnet"; // indicates any local address on the local subnet.
    static inline const wchar_t* AddrKeywordDNS = L"DNS";
    static inline const wchar_t* AddrKeywordDHCP = L"DHCP";
    static inline const wchar_t* AddrKeywordWINS = L"WINS";
    static inline const wchar_t* AddrKeywordDefaultGateway = L"DefaultGateway";
    // since win8
    static inline const wchar_t* AddrKeywordIntrAnet = L"LocalIntranet";
    static inline const wchar_t* AddrKeywordRmtIntrAnet = L"RemoteIntranet";
    static inline const wchar_t* AddrKeywordIntErnet = L"Internet";
    static inline const wchar_t* AddrKeywordPly2Renders = L"Ply2Renders";
    // since 1903
    static inline const wchar_t* AddrKeywordCaptivePortal = L"CaptivePortal";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // 

    std::wstring Guid;  // Note: usually this is a guid but some default windows rules use a string name instead
    int Index = 0;      // this is only used for sorting by newest rules

    std::wstring BinaryPath;
    std::wstring ServiceTag;
    std::wstring AppContainerSid;
    std::wstring LocalUserOwner;
    std::wstring PackageFamilyName;
        
    std::wstring Name;
    std::wstring Grouping;
    std::wstring Description;

    bool Enabled = false;
    EFwActions Action = EFwActions::Undefined;
    EFwDirections Direction = EFwDirections::Unknown;
    int Profile = (int)EFwProfiles::All;

    EFwKnownProtocols Protocol = EFwKnownProtocols::Any;
    int Interface = (int)EFwInterfaces::All;
    std::vector<std::wstring> LocalAddresses;
    std::vector<std::wstring> LocalPorts;
    std::vector<std::wstring> RemoteAddresses;
    std::vector<std::wstring> RemotePorts;
    std::vector<IcmpTypeCode> IcmpTypesAndCodes;

    std::vector<std::wstring> OsPlatformValidity;

    int EdgeTraversal = 0;
};

typedef std::shared_ptr<SWindowsFwRule> SWindowsFwRulePtr;

typedef std::map<std::wstring, SWindowsFwRulePtr> SWindowsFwRuleMap;
typedef std::list<SWindowsFwRulePtr> SWindowsFwRuleList;

////////////////////////////////////////////////////////////////////////////////////////////////////////
// App Package List

struct SAppContainer
{
    SSid ContainerSid;
    SSid UserSid;
    std::wstring Id;
    std::wstring Name;
    std::wstring Description;
};

typedef std::shared_ptr<SAppContainer> SAppContainerPtr;
typedef std::list<SAppContainerPtr> SAppContainerList;

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Network Lists

struct SFwNetwork
{
    std::wstring pszName;
    EFwProfiles ProfileType;
};

typedef std::shared_ptr<SFwNetwork> SFwNetworkPtr;
typedef std::list<SFwNetworkPtr> SFwNetworkList;

struct SNetAdapter
{
    std::wstring pszFriendlyName;
    std::wstring Guid;
};

typedef std::shared_ptr<SNetAdapter> SNetAdapterPtr;
typedef std::list<SNetAdapterPtr> SNetAdapterList;

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CWindowsFirewall

class CWindowsFirewall
{
	CWindowsFirewall();
    ~CWindowsFirewall();
public:

	static CWindowsFirewall* Instance();
    static void Dispose();
   
    bool IsReady();

    RESULT(std::shared_ptr<SWindowsFwRuleList>)     LoadRules();
    RESULT(std::shared_ptr<SWindowsFwRuleMap>)      LoadRules(const std::vector<std::wstring>& RuleIds);

    SWindowsFwRulePtr                               GetRule(const std::wstring RuleId) const;
    SWindowsFwRuleMap                               GetAllRules() const;
    SWindowsFwRuleMap                               GetRules(const std::vector<std::wstring>& RuleIds) const;

    STATUS                                          UpdateRule(const SWindowsFwRulePtr& pRule);

    STATUS                                          RemoveRule(const std::wstring RuleId);

    bool                                            GetFirewallEnabled(EFwProfiles profileType);
    void                                            SetFirewallEnabled(EFwProfiles profileType, bool Enabled);
    bool                                            GetBlockAllInboundTraffic(EFwProfiles profileType);
    void                                            SetBlockAllInboundTraffic(EFwProfiles profileType, bool Block);
    bool                                            GetInboundEnabled(EFwProfiles profileType);
    void                                            SetInboundEnabled(EFwProfiles profileType, bool Enable);
    EFwActions                                      GetDefaultInboundAction(EFwProfiles profileType);
    void                                            SetDefaultInboundAction(EFwProfiles profileType, EFwActions Action);
    EFwActions                                      GetDefaultOutboundAction(EFwProfiles profileType);
    void                                            SetDefaultOutboundAction(EFwProfiles profileType, EFwActions Action);

    RESULT(std::shared_ptr<SAppContainerList>)      EnumAppContainers();
    RESULT(std::shared_ptr<SFwNetworkList>)         EnumFwNetworks();
    RESULT(std::shared_ptr<SNetAdapterList>)        EnumNetAdapters();

protected:

    SWindowsFwRuleMap   m_FwRules;

private:

	static CWindowsFirewall* m_Instance;
	struct SWindowsFirewall* m;
};