#include "pch.h"
#include "FirewallRule.h"
#include "WindowsFirewall.h"
#include "ServiceAPI.h"
#include "..\..\Processes\Process.h"

CFirewallRule::CFirewallRule()
{

}

CFirewallRule::CFirewallRule(const std::shared_ptr<struct SWindowsFwRule>& Rule)
{
	std::wstring BinaryPath = Rule->BinaryPath;
	if (_wcsicmp(BinaryPath.c_str(), L"system") == 0)
		BinaryPath = CProcess::NtOsKernel_exe;
		
	m_ProgramID.Set(BinaryPath, Rule->ServiceTag, Rule->AppContainerSid);

	Update(Rule);
}

CFirewallRule::~CFirewallRule()
{

}

void CFirewallRule::Update(const std::shared_ptr<struct SWindowsFwRule>& Rule)
{
	std::unique_lock Lock(m_Mutex);

	m_Data = Rule;
}

std::wstring CFirewallRule::GetGuid() const
{
	std::shared_lock Lock(m_Mutex);

	return m_Data->guid;
}

CVariant CFirewallRule::ToVariant() const
{
	std::shared_lock Lock(m_Mutex);

	CVariant FwRule;

	FwRule[SVC_API_FW_GUID] = m_Data->guid;
	FwRule[SVC_API_FW_INDEX] = m_Data->Index;

	FwRule[SVC_API_ID_PROG] = m_ProgramID.ToVariant();

	FwRule[SVC_API_ID_FILE_PATH] = m_Data->BinaryPath;
	FwRule[SVC_API_ID_SVC_TAG] = m_Data->ServiceTag;
	FwRule[SVC_API_ID_APP_SID] = m_Data->AppContainerSid;

	FwRule[SVC_API_FW_NAME] = m_Data->Name;
	FwRule[SVC_API_FW_GROUP] = m_Data->Grouping;
	FwRule[SVC_API_FW_DESCRIPTION] = m_Data->Description;

	FwRule[SVC_API_FW_ENABLED] = m_Data->Enabled;

	switch (m_Data->Action) {
	case EFwActions::Allow: FwRule[SVC_API_FW_ACTION] = SVC_API_FW_ALLOW; break;
	case EFwActions::Block: FwRule[SVC_API_FW_ACTION] = SVC_API_FW_BLOCK; break;
	}

	switch (m_Data->Direction) {
	case EFwDirections::Inbound:	FwRule[SVC_API_FW_DIRECTION] = SVC_API_FW_INBOUND; break;
	case EFwDirections::Outbound:	FwRule[SVC_API_FW_DIRECTION] = SVC_API_FW_OUTBOUND; break;
	}

	std::vector<std::string> Profiles;
	if (m_Data->Profile == (int)EFwProfiles::All)
		Profiles.push_back(SVC_API_FW_ALL);
	else
	{
		if(m_Data->Profile & (int)EFwProfiles::Domain)	Profiles.push_back(SVC_API_FW_DOMAIN);
		if(m_Data->Profile & (int)EFwProfiles::Private)	Profiles.push_back(SVC_API_FW_PRIVATE);
		if(m_Data->Profile & (int)EFwProfiles::Public)	Profiles.push_back(SVC_API_FW_PUBLIC);
	}
	FwRule[SVC_API_FW_PROFILE] = Profiles;

	FwRule[SVC_API_FW_PROTOCOL] = (uint32)m_Data->Protocol;

	std::vector<std::string> Interfaces;
	if (m_Data->Interface == (int)EFwInterfaces::All)
		Interfaces.push_back(SVC_API_FW_ANY);
	else
	{
		if(m_Data->Interface & (int)EFwInterfaces::Lan)			Interfaces.push_back(SVC_API_FW_LAN);
		if(m_Data->Interface & (int)EFwInterfaces::Wireless)	Interfaces.push_back(SVC_API_FW_WIRELESS);
		if(m_Data->Interface & (int)EFwInterfaces::RemoteAccess)Interfaces.push_back(SVC_API_FW_REMOTEACCESS);
	}
	FwRule[SVC_API_FW_INTERFACE] = Interfaces;

	FwRule[SVC_API_FW_LOCAL_ADDR] = m_Data->LocalAddresses;
	FwRule[SVC_API_FW_LOCAL_PORT] = m_Data->LocalPorts;
	FwRule[SVC_API_FW_REMOTE_ADDR] = m_Data->RemoteAddresses;
	FwRule[SVC_API_FW_REMOTE_PORT] = m_Data->RemotePorts;

	std::vector<std::string> IcmpTypesAndCodes;
	for (auto I : m_Data->IcmpTypesAndCodes)
		IcmpTypesAndCodes.push_back(std::to_string(I.Type) + ":" + (I.Code == 256 ? "*" : std::to_string(I.Code)));
	FwRule[SVC_API_FW_ICMP_OPT] = IcmpTypesAndCodes;

	FwRule[SVC_API_FW_OS_P] = m_Data->OsPlatformValidity;

	FwRule[SVC_API_FW_EDGE_T] = m_Data->EdgeTraversal;

	return FwRule;
}

bool CFirewallRule::FromVariant(const CVariant& FwRule)
{
	std::unique_lock Lock(m_Mutex);

	if (!m_Data) m_Data = std::make_shared<SWindowsFwRule>();

	m_Data->guid = FwRule[SVC_API_FW_GUID];
	m_Data->Index = FwRule[SVC_API_FW_INDEX];

	m_ProgramID.FromVariant(FwRule[SVC_API_ID_PROG]);

	m_Data->BinaryPath = FwRule[SVC_API_ID_FILE_PATH];
	m_Data->ServiceTag = FwRule[SVC_API_ID_SVC_TAG];
	m_Data->AppContainerSid = FwRule[SVC_API_ID_APP_SID];

	m_Data->Name = FwRule[SVC_API_FW_NAME];
	m_Data->Grouping = FwRule[SVC_API_FW_GROUP];
	m_Data->Description = FwRule[SVC_API_FW_DESCRIPTION];

	m_Data->Enabled = FwRule[SVC_API_FW_ENABLED];
	if (FwRule[SVC_API_FW_ACTION].To<std::string>() == SVC_API_FW_ALLOW)
		m_Data->Action = EFwActions::Allow;
	else if (FwRule[SVC_API_FW_ACTION].To<std::string>() == SVC_API_FW_BLOCK)
		m_Data->Action = EFwActions::Block;

	if (FwRule[SVC_API_FW_DIRECTION].To<std::string>() == SVC_API_FW_BIDIRECTIONAL)
		m_Data->Direction = EFwDirections::Bidirectiona;
	else if (FwRule[SVC_API_FW_DIRECTION].To<std::string>() == SVC_API_FW_INBOUND)
		m_Data->Direction = EFwDirections::Inbound;
	else if (FwRule[SVC_API_FW_DIRECTION].To<std::string>() == SVC_API_FW_OUTBOUND)
		m_Data->Direction = EFwDirections::Outbound;
	
	std::vector<std::string> Profiles = FwRule[SVC_API_FW_PROFILE].AsList<std::string>();
	if (Profiles.empty() || (Profiles.size() == 1 && (Profiles[0] == SVC_API_FW_ALL || Profiles[0].empty())))
		m_Data->Profile = (int)EFwProfiles::All;
	else
	{
		m_Data->Profile = 0;
		for (auto Profile : Profiles)
		{
			if (Profile == SVC_API_FW_DOMAIN)
				m_Data->Profile |= (int)EFwProfiles::Domain;
			if (Profile == SVC_API_FW_PRIVATE)
				m_Data->Profile |= (int)EFwProfiles::Private;
			if (Profile == SVC_API_FW_PUBLIC)
				m_Data->Profile |= (int)EFwProfiles::Public;
		}
	}

	m_Data->Protocol = (EFwKnownProtocols)FwRule[SVC_API_FW_PROTOCOL].To<uint32>();

	std::vector<std::string> Interfaces = FwRule[SVC_API_FW_INTERFACE].AsList<std::string>();
	if (Interfaces.empty() || (Interfaces.size() == 1 && (Interfaces[0] == SVC_API_FW_ALL || Interfaces[0].empty())))
		m_Data->Interface = (int)EFwInterfaces::All;
	else
	{
		m_Data->Interface = 0;
		for (auto Interface : Interfaces)
		{
			if (Interface == SVC_API_FW_LAN)
				m_Data->Interface |= (int)EFwInterfaces::Lan;
			if (Interface == SVC_API_FW_WIRELESS)
				m_Data->Interface |= (int)EFwInterfaces::Wireless;
			if (Interface == SVC_API_FW_REMOTEACCESS)
				m_Data->Interface |= (int)EFwInterfaces::RemoteAccess;
		}
	}

	m_Data->LocalAddresses = FwRule[SVC_API_FW_LOCAL_ADDR].AsList<std::wstring>();
	m_Data->LocalPorts = FwRule[SVC_API_FW_LOCAL_PORT].AsList<std::wstring>();
	m_Data->RemoteAddresses = FwRule[SVC_API_FW_REMOTE_ADDR].AsList<std::wstring>();
	m_Data->RemotePorts = FwRule[SVC_API_FW_REMOTE_PORT].AsList<std::wstring>();

	std::vector<std::string> IcmpTypesAndCodes = FwRule[SVC_API_FW_ICMP_OPT].AsList<std::string>();
	for (auto Icmp : IcmpTypesAndCodes)
	{
		auto TypeCode = Split2(Icmp, ":");
		SWindowsFwRule::IcmpTypeCode IcmpTypeCode;
		IcmpTypeCode.Type = std::atoi(TypeCode.first.c_str());
		IcmpTypeCode.Code = TypeCode.second == "*" ? 256 : std::atoi(TypeCode.second.c_str());
		m_Data->IcmpTypesAndCodes.push_back(IcmpTypeCode);
	}

	m_Data->OsPlatformValidity = FwRule[SVC_API_FW_OS_P].AsList<std::wstring>();

	m_Data->EdgeTraversal = FwRule[SVC_API_FW_EDGE_T];

	return true;
}