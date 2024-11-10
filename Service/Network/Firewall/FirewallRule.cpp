#include "pch.h"
#include "FirewallRule.h"
#include "WindowsFirewall.h"
#include "../Library/API/PrivacyAPI.h"
#include "..\..\Processes\Process.h"
#include "../Library/Common/Strings.h"
#include "../../Library/Helpers/AppUtil.h"

CFirewallRule::CFirewallRule()
{

}

CFirewallRule::CFirewallRule(const std::shared_ptr<struct SWindowsFwRule>& Rule)
{
	m_Data = Rule;

	m_BinaryPath = m_Data->BinaryPath;
	if (_wcsicmp(m_BinaryPath.c_str(), L"system") == 0)
		m_BinaryPath = L"%SystemRoot%\\system32\\ntoskrnl.exe";
	//DbgPrint(L"%s\n", BinaryPath.c_str());
	
	std::wstring AppContainerSid = m_Data->AppContainerSid;
	if(AppContainerSid.empty() && !m_Data->PackageFamilyName.empty())
		AppContainerSid = GetAppContainerSidFromName(m_Data->PackageFamilyName);
	m_ProgramID.Set(m_BinaryPath, m_Data->ServiceTag, AppContainerSid);
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

	return m_Data->Guid;
}

CVariant CFirewallRule::ToVariant(const SVarWriteOpt& Opts) const
{
	std::shared_lock Lock(m_Mutex);

	CVariant Rule;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Rule.BeginIMap();
		WriteIVariant(Rule, Opts);
	} else {  
		Rule.BeginMap();
		WriteMVariant(Rule, Opts);
	}
	Rule.Finish();
	return Rule;
}

bool CFirewallRule::FromVariant(const CVariant& Rule)
{
	std::unique_lock Lock(m_Mutex);

	if (!m_Data) m_Data = std::make_shared<SWindowsFwRule>();

	if (Rule.GetType() == VAR_TYPE_MAP)         Rule.ReadRawMap([&](const SVarName& Name, const CVariant& Data) { ReadMValue(Name, Data); });
	else if (Rule.GetType() == VAR_TYPE_INDEX)  Rule.ReadRawIMap([&](uint32 Index, const CVariant& Data)        { ReadIValue(Index, Data); });
	else
		return false;

	std::wstring AppContainerSid = m_Data->AppContainerSid;
	if(AppContainerSid.empty() && !m_Data->PackageFamilyName.empty())
		AppContainerSid = GetAppContainerSidFromName(m_Data->PackageFamilyName);
	m_ProgramID.Set(m_BinaryPath, m_Data->ServiceTag, AppContainerSid);

	m_Data->BinaryPath = m_BinaryPath;
	if (_wcsicmp(m_Data->BinaryPath.c_str(), L"%SystemRoot%\\system32\\ntoskrnl.exe") == 0)
		m_Data->BinaryPath = L"system";

	return true;
}