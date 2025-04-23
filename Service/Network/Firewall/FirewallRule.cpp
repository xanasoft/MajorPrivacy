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

	m_BinaryPath = m_Data->BinaryPath;
	if (_wcsicmp(m_BinaryPath.c_str(), L"system") == 0)
		m_BinaryPath = CProcess::NtOsKernel_exe;
	//DbgPrint(L"%s\n", BinaryPath.c_str());
	
	std::wstring AppContainerSid = m_Data->AppContainerSid;
	if(AppContainerSid.empty() && !m_Data->PackageFamilyName.empty())
		AppContainerSid = GetAppContainerSidFromName(m_Data->PackageFamilyName);
	m_ProgramID = CProgramID::FromFw(m_BinaryPath, m_Data->ServiceTag, AppContainerSid);
}

CFirewallRule::~CFirewallRule()
{

}

void CFirewallRule::Update(const std::shared_ptr<struct SWindowsFwRule>& Rule)
{
	std::unique_lock Lock(m_Mutex);

	m_Data = Rule;

	m_bTemporary = _wcsnicmp(m_Data->Grouping.c_str(), L"&Temporary", 10) == 0;
}

void CFirewallRule::Update(const std::shared_ptr<CFirewallRule>& pRule)
{
	std::unique_lock Lock(m_Mutex);

	m_bTemporary = pRule->m_bTemporary;
	m_uTimeOut = pRule->m_uTimeOut;
}

std::wstring CFirewallRule::GetGuidStr() const
{
	std::shared_lock Lock(m_Mutex);

	return m_Data->Guid;
}

std::wstring CFirewallRule::GetBinaryPath() const
{
	std::shared_lock Lock(m_Mutex);

	return m_BinaryPath;
}

bool CFirewallRule::IsEnabled() const
{ 
	std::shared_lock Lock(m_Mutex); 

	return m_Data->Enabled; 
}

bool CFirewallRule::IsExpired() const
{ 
	std::shared_lock Lock(m_Mutex); 
	if(m_uTimeOut == -1)
		return false;
	uint64 CurrentTime = FILETIME2time(GetCurrentTimeAsFileTime());
	if (m_uTimeOut < CurrentTime) {
//#ifdef _DEBUG
//		DbgPrint("Firewall Rule %S has expired\r\n", m_Data->Name.c_str());
//#endif
		return true;
	}
//#ifdef _DEBUG
//	DbgPrint("Firewall Rule %S will expire in %llu seconds\r\n", m_Data->Name.c_str(), m_uTimeOut - CurrentTime);
//#endif
	return false;
}

StVariant CFirewallRule::ToVariant(const SVarWriteOpt& Opts) const
{
	std::shared_lock Lock(m_Mutex);

	StVariantWriter Rule;
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

	if (!m_Data) m_Data = std::make_shared<SWindowsFwRule>();

	if (Rule.GetType() == VAR_TYPE_MAP)         StVariantReader(Rule).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
	else if (Rule.GetType() == VAR_TYPE_INDEX)  StVariantReader(Rule).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
	else
		return false;

	std::wstring AppContainerSid = m_Data->AppContainerSid;
	if(AppContainerSid.empty() && !m_Data->PackageFamilyName.empty())
		AppContainerSid = GetAppContainerSidFromName(m_Data->PackageFamilyName);
	m_ProgramID = CProgramID::FromFw(m_BinaryPath, m_Data->ServiceTag, AppContainerSid);

	m_Data->BinaryPath = m_BinaryPath;
	if (theCore->ProgramManager()->IsNtOsKrnl(m_BinaryPath))
		m_Data->BinaryPath = L"system";

	return true;
}