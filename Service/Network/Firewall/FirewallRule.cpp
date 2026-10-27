#include "pch.h"
#include "FirewallRule.h"
#include "WindowsFirewall.h"
#include "../Library/API/PrivacyAPI.h"
#include "..\..\Processes\Process.h"
#include "../Library/Common/Strings.h"
#include <algorithm>
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

	SetPrincipalSddl(m_FwRule->LocalUserAuthorizationList);
}

CFirewallRule::~CFirewallRule()
{

}

std::shared_ptr<CFirewallRule> CFirewallRule::Clone(bool CloneGuid) const
{
	std::shared_lock Lock(m_Mutex);

	std::shared_ptr<CFirewallRule> pRule = std::make_shared<CFirewallRule>(std::make_shared<struct SWindowsFwRule>(*m_FwRule));

	pRule->SetPrincipalSddl(m_PrincipalSddl);

	pRule->m_State = m_State;
	pRule->m_OriginalGuid = m_OriginalGuid;

	pRule->m_Source = m_Source;
	pRule->m_TemplateGuid = m_TemplateGuid;

	pRule->m_bTemporary = m_bTemporary;
	pRule->m_uTimeOut = m_uTimeOut;

	return pRule;
}

bool CFirewallRule::Match(const std::shared_ptr<struct SWindowsFwRule>& Rule)const
{
	std::shared_lock Lock(m_Mutex);

	return Match(m_FwRule, Rule);
}

bool CFirewallRule::Match(const std::shared_ptr<struct SWindowsFwRule>& RuleL, const std::shared_ptr<struct SWindowsFwRule>& RuleR)
{
	return MatchDiff(RuleL, RuleR).empty();
}

//
// Whether two rules are the same rule, and where they part company when they are not. Asking it
// this way round means the yes or no answer and the reason behind it can never drift apart, and
// a program which puts back almost what it took away can say which field it changed on the way.
//
std::wstring CFirewallRule::MatchDiff(const std::shared_ptr<struct SWindowsFwRule>& RuleL, const std::shared_ptr<struct SWindowsFwRule>& RuleR)
{
	if (RuleL->Name != RuleR->Name)
		return L"Name"; // metadata only
	if (RuleL->Grouping != RuleR->Grouping)
		return L"Grouping"; // metadata only
	if (RuleL->Description != RuleR->Description)
		return L"Description"; // metadata only

	if (_wcsicmp(RuleL->BinaryPath.c_str(), RuleR->BinaryPath.c_str()) != 0)
		return L"BinaryPath";
	if (_wcsicmp(RuleL->ServiceTag.c_str(), RuleR->ServiceTag.c_str()) != 0)
		return L"ServiceTag";

	// Only AppContainerSid or PackageFamilyName should be set in a rule and the API wrapper SWindowsFirewall::SaveRule obays this,
	// but our UI and potentially other comonents will not be aware of the rule scheme capabilities and attempt to set booth,
	// so we need to handle this case here in a smart way.

	bool AppSidSet = !RuleL->AppContainerSid.empty() || !RuleR->AppContainerSid.empty();
	bool AppSidMissmatch = _wcsicmp(RuleL->AppContainerSid.c_str(), RuleR->AppContainerSid.c_str()) != 0;
	bool AppPFNSet = !RuleL->PackageFamilyName.empty() || !RuleR->PackageFamilyName.empty();
	bool AppPFNMissmatch = _wcsicmp(RuleL->PackageFamilyName.c_str(), RuleR->PackageFamilyName.c_str()) != 0;
	if (AppSidSet && AppPFNSet) {
		if (AppSidMissmatch && AppPFNMissmatch)
			return L"AppContainer";
	} else {
		if (AppSidMissmatch || AppPFNMissmatch)
			return L"AppContainer";
	}

	if (_wcsicmp(RuleL->LocalUserAuthorizationList.c_str(), RuleR->LocalUserAuthorizationList.c_str()) != 0)
		return L"LocalUserAuthorizationList";

	//if (_wcsicmp(RuleL->LocalUserOwner.c_str(), RuleR->LocalUserOwner.c_str()) != 0)
	//	return false; // this is metadata only (has no effect on fitlering) and we can NOT set it

	if (RuleL->Enabled != RuleR->Enabled)
		return L"Enabled";
	if (RuleL->Action != RuleR->Action)
		return L"Action";
	if (RuleL->Direction != RuleR->Direction)
		return L"Direction";
	if (RuleL->Profile != RuleR->Profile)
		return L"Profile";

	if (RuleL->Protocol != RuleR->Protocol)
		return L"Protocol";
	if (RuleL->Interface != RuleR->Interface)
		return L"Interface";
	if (RuleL->LocalAddresses != RuleR->LocalAddresses)
		return L"LocalAddresses";
	if (RuleL->LocalPorts != RuleR->LocalPorts)
		return L"LocalPorts";
	if (RuleL->RemoteAddresses != RuleR->RemoteAddresses)
		return L"RemoteAddresses";
	if (RuleL->RemotePorts != RuleR->RemotePorts)
		return L"RemotePorts";
	if (RuleL->IcmpTypesAndCodes != RuleR->IcmpTypesAndCodes)
		return L"IcmpTypesAndCodes";

	if (RuleL->OsPlatformValidity != RuleR->OsPlatformValidity)
		return L"OsPlatformValidity"; // metadata has effect on loading but not on filetring

	if (RuleL->EdgeTraversal != RuleR->EdgeTraversal)
		return L"EdgeTraversal";

	return std::wstring();
}

void CFirewallRule::Update(const std::shared_ptr<struct SWindowsFwRule>& Rule)
{
	std::unique_lock Lock(m_Mutex);

	m_FwRule = Rule;

	SetPrincipalSddl(m_FwRule->LocalUserAuthorizationList);

	m_bTemporary = _wcsnicmp(m_FwRule->Grouping.c_str(), L"#Temporary", 10) == 0;
}

void CFirewallRule::Update(const std::shared_ptr<CFirewallRule>& pRule)
{
	std::unique_lock Lock(m_Mutex);

	SetPrincipalSddl(pRule->m_PrincipalSddl);

	m_State = pRule->m_State;
	m_OriginalGuid = pRule->m_OriginalGuid;

	m_Source = pRule->m_Source;

	m_bTemporary = pRule->m_bTemporary;
	m_uTimeOut = pRule->m_uTimeOut;
}

//
// Unlike Update this leaves the identity alone, the guid and the backup link stay those of this
// rule. Only the verdict and our own bookkeeping move over: a rule deleted and recreated by its
// program is the same rule to the user, approved if it was, unapproved if it was not.
//
void CFirewallRule::TakeOver(const std::shared_ptr<CFirewallRule>& pRule)
{
	EFwRuleState State;
	EFwRuleSource Source;
	std::wstring TemplateGuid;
	uint64 uTimeOut;
	{
		std::shared_lock Lock(pRule->m_Mutex);
		State = pRule->m_State;
		Source = pRule->m_Source;
		TemplateGuid = pRule->m_TemplateGuid;
		uTimeOut = pRule->m_uTimeOut;
	}

	std::unique_lock Lock(m_Mutex);
	m_State = State;
	m_Source = Source;
	m_TemplateGuid = TemplateGuid;
	m_uTimeOut = uTimeOut;
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
	//m_State = EFwRuleState::eBackup;
	//
	// The backup stays linked to the rule it was taken from, also when the program of that rule
	// changed. It used to get a guid of its own then, as if a new rule had been created and the
	// original removed, but that cut the link: the UI could not find the backup to offer it, and
	// restoring it made a second rule at the old path instead of moving the rule back.
	//
	UNREFERENCED_PARAMETER(bProgramChanged);
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
	if(m_ProgramID.GetType() == EProgramType::eAllPrograms)
		m_FwRule->BinaryPath= L"";
	else if (theCore->ProgramManager()->IsNtOsKrnl(m_BinaryPath))
		m_FwRule->BinaryPath = L"system";
	else
		m_FwRule->BinaryPath = m_BinaryPath;

	SetPrincipalSddl(m_PrincipalSddl);

	return true;
}

void CFirewallRule::SetPrincipalSddl(const std::wstring& PrincipalSddl)
{
	if (&m_PrincipalSddl != &PrincipalSddl) m_PrincipalSddl = PrincipalSddl;
	if (&m_FwRule->LocalUserAuthorizationList != &PrincipalSddl) m_FwRule->LocalUserAuthorizationList = PrincipalSddl;

	// Parse SDDL and populate a new SID table
	auto newSids = std::make_shared<std::unordered_map<std::wstring, SSidEntryInfo>>();
	bool bHasApplyEntries = false;

	if (!m_PrincipalSddl.empty()) {
		// Find DACL section (after "D:")
		size_t daclPos = m_PrincipalSddl.find(L"D:");
		if (daclPos != std::wstring::npos) {
			size_t pos = daclPos + 2;

			while (pos < m_PrincipalSddl.length()) {
				// Find start of ACE
				size_t aceStart = m_PrincipalSddl.find(L'(', pos);
				if (aceStart == std::wstring::npos)
					break;

				size_t aceEnd = m_PrincipalSddl.find(L')', aceStart);
				if (aceEnd == std::wstring::npos)
					break;

				// Extract ACE content
				std::wstring ace = m_PrincipalSddl.substr(aceStart + 1, aceEnd - aceStart - 1);
				pos = aceEnd + 1;

				// Parse ACE: type;flags;rights;object_guid;inherit_object_guid;account_sid
				auto parts = SplitStr(ace, L";");
				if (parts.size() < 6)
					continue;

				wchar_t aceType = parts[0].empty() ? 0 : parts[0][0];
				const std::wstring& aceSid = parts[5];

				if (aceSid.empty())
					continue;

				SSidEntryInfo info;
				if (aceType == L'A') {
					info.Type = ESidEntryType::eApply;
					bHasApplyEntries = true;
				} else if (aceType == L'D') {
					info.Type = ESidEntryType::eExempt;
				} else {
					continue;
				}

				// Store SID in uppercase for case-insensitive comparison
				std::wstring sidUpper = aceSid;
				std::transform(sidUpper.begin(), sidUpper.end(), sidUpper.begin(), ::towupper);
				(*newSids)[sidUpper] = info;
			}
		}
	}

	// Atomically assign the new map
	m_PrincipalSids = newSids->empty() ? nullptr : newSids;
	m_bHasApplyEntries = bHasApplyEntries;
}