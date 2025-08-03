#include "pch.h"
#include "FirewallRule.h"
#include "WindowsFirewall.h"
#include "../Library/API/PrivacyAPI.h"
#include "..\..\Processes\Process.h"
#include "../Library/Common/Strings.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../../ServiceCore.h"
#include "../../Programs/ProgramManager.h"
#include "../Library/Helpers/NtUtil.h"


CFirewallRule::CFirewallRule()
{

}

CFirewallRule::CFirewallRule(const std::shared_ptr<struct SWindowsFwRule>& Rule)
{
	Update(Rule);

	m_BinaryPath = m_FwRule->BinaryPath;
	if (_wcsicmp(m_BinaryPath.c_str(), L"system") == 0)
		m_BinaryPath = CProcess::NtOsKernel_exe;
	//DbgPrint(L"%s\n", BinaryPath.c_str());
	
	std::wstring AppContainerSid = m_FwRule->AppContainerSid;
	if(AppContainerSid.empty() && !m_FwRule->PackageFamilyName.empty())
		AppContainerSid = GetAppContainerSidFromName(m_FwRule->PackageFamilyName);
	m_ProgramID = CProgramID::FromFw(m_BinaryPath, m_FwRule->ServiceTag, AppContainerSid);
}

CFirewallRule::~CFirewallRule()
{

}

bool CFirewallRule::Match(const std::shared_ptr<struct SWindowsFwRule>& Rule)const
{
	std::shared_lock Lock(m_Mutex);

	return Match(m_FwRule, Rule);
}

bool CFirewallRule::Match(const std::shared_ptr<struct SWindowsFwRule>& RuleL, const std::shared_ptr<struct SWindowsFwRule>& RuleR)
{
	if (RuleL->Name != RuleR->Name)
		return false; // metadata only
	if (RuleL->Grouping != RuleR->Grouping)
		return false; // metadata only
	if (RuleL->Description != RuleR->Description)
		return false; // metadata only

	if (_wcsicmp(RuleL->BinaryPath.c_str(), RuleR->BinaryPath.c_str()) != 0)
		return false;
	if (_wcsicmp(RuleL->ServiceTag.c_str(), RuleR->ServiceTag.c_str()) != 0)
		return false;
	if (_wcsicmp(RuleL->AppContainerSid.c_str(), RuleR->AppContainerSid.c_str()) != 0)
		return false;
	//if (_wcsicmp(RuleL->LocalUserOwner.c_str(), RuleR->LocalUserOwner.c_str()) != 0)
	//	return false; // this is metadata only (has no effect on fitlering) and we can NOT set it
	if (_wcsicmp(RuleL->PackageFamilyName.c_str(), RuleR->PackageFamilyName.c_str()) != 0)
		return false;

	if (RuleL->Enabled != RuleR->Enabled)
		return false;
	if (RuleL->Action != RuleR->Action)
		return false;
	if (RuleL->Direction != RuleR->Direction)
		return false;
	if (RuleL->Profile != RuleR->Profile)
		return false;

	if (RuleL->Protocol != RuleR->Protocol)
		return false;
	if (RuleL->Interface != RuleR->Interface)
		return false;
	if (RuleL->LocalAddresses != RuleR->LocalAddresses)
		return false;
	if (RuleL->LocalPorts != RuleR->LocalPorts)
		return false;
	if (RuleL->RemoteAddresses != RuleR->RemoteAddresses)
		return false;
	if (RuleL->RemotePorts != RuleR->RemotePorts)
		return false;
	if (RuleL->IcmpTypesAndCodes != RuleR->IcmpTypesAndCodes)
		return false;

	if (RuleL->OsPlatformValidity != RuleR->OsPlatformValidity)
		return false; // metadata has effect on loading but not on filetring

	if (RuleL->EdgeTraversal != RuleR->EdgeTraversal)
		return false;

	return true;
}

void CFirewallRule::Update(const std::shared_ptr<struct SWindowsFwRule>& Rule)
{
	std::unique_lock Lock(m_Mutex);

	m_FwRule = Rule;

	m_bTemporary = _wcsnicmp(m_FwRule->Grouping.c_str(), L"#Temporary", 10) == 0;
}

void CFirewallRule::Update(const std::shared_ptr<CFirewallRule>& pRule)
{
	std::unique_lock Lock(m_Mutex);

	m_State = pRule->m_State;
	m_OriginalGuid = pRule->m_OriginalGuid;

	m_Source = pRule->m_Source;

	m_bTemporary = pRule->m_bTemporary;
	m_uTimeOut = pRule->m_uTimeOut;
}

std::wstring CFirewallRule::GetGuidStr() const
{
	std::shared_lock Lock(m_Mutex);

	return m_FwRule->Guid;
}

std::wstring CFirewallRule::GetName() const
{
	std::shared_lock Lock(m_Mutex);

	return m_FwRule->Name;
}

void CFirewallRule::SetAsBackup(bool bRemoved, bool bProgramChanged)
{
	std::unique_lock Lock(m_Mutex);
	m_State = EFwRuleState::eBackup;
	//
	// Special case when the program of a rule changed,
	// in such scenario we behave as if a new rule for the new program woudl have been created
	// and the original rule removed.
	//
	if(bProgramChanged)
		m_OriginalGuid = MkGuid();
	else
		m_OriginalGuid = m_FwRule->Guid;
	// Generate a new random GUID for the backup rule.
	m_FwRule->Guid = MkGuid();
}

std::wstring CFirewallRule::GetBinaryPath() const
{
	std::shared_lock Lock(m_Mutex);

	return m_BinaryPath;
}

void CFirewallRule::SetEnabled(bool bEnabled)
{ 
	std::unique_lock Lock(m_Mutex); 

	m_FwRule->Enabled = bEnabled; 
}

bool CFirewallRule::IsEnabled() const
{ 
	std::shared_lock Lock(m_Mutex); 

	return m_FwRule->Enabled; 
}

bool CFirewallRule::IsExpired() const
{ 
	std::shared_lock Lock(m_Mutex); 
	if(m_uTimeOut == -1)
		return false;
	uint64 CurrentTime = FILETIME2time(GetCurrentTimeAsFileTime());
	if (m_uTimeOut < CurrentTime) {
//#ifdef _DEBUG
//		DbgPrint("Firewall Rule %S has expired\r\n", m_FwRule->Name.c_str());
//#endif
		return true;
	}
//#ifdef _DEBUG
//	DbgPrint("Firewall Rule %S will expire in %llu seconds\r\n", m_FwRule->Name.c_str(), m_uTimeOut - CurrentTime);
//#endif
	return false;
}

StVariant CFirewallRule::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	std::shared_lock Lock(m_Mutex);

	StVariantWriter Rule(pMemPool);
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Rule.BeginIndex();
		WriteIVariant(Rule, Opts);
	} else {  
		Rule.BeginMap();
		WriteMVariant(Rule, Opts);
	}
	return Rule.Finish();
}

bool CFirewallRule::FromVariant(const StVariant& Rule)
{
	std::unique_lock Lock(m_Mutex);

	if (!m_FwRule) m_FwRule = std::make_shared<SWindowsFwRule>();

	if (Rule.GetType() == VAR_TYPE_MAP)         StVariantReader(Rule).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
	else if (Rule.GetType() == VAR_TYPE_INDEX)  StVariantReader(Rule).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
	else
		return false;

	std::wstring AppContainerSid = m_FwRule->AppContainerSid;
	if(AppContainerSid.empty() && !m_FwRule->PackageFamilyName.empty())
		AppContainerSid = GetAppContainerSidFromName(m_FwRule->PackageFamilyName);
	m_ProgramID = CProgramID::FromFw(m_BinaryPath, m_FwRule->ServiceTag, AppContainerSid);

	m_FwRule->BinaryPath = m_BinaryPath;
	if (theCore->ProgramManager()->IsNtOsKrnl(m_BinaryPath))
		m_FwRule->BinaryPath = L"system";

	return true;
}