#include "pch.h"
#include "Firewall.h"
#include "WindowsFirewall.h"
#include "../../ServiceCore.h"
//#include "../Library/API/AlpcPortServer.h"
#include "../../Library/IPC/PipeServer.h"
#include "WindowsFwLog.h"
#include "WindowsFwGuard.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../NetworkManager.h"
#include "../SocketList.h"
#include "../../Programs/ProgramManager.h"
#include "../../Processes/ProcessList.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/RegUtil.h"
#include "../NetLogEntry.h"
#include "../Library/Common/UIntX.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "../Library/Helpers/NtObj.h"


#define API_FIREWALL_FILE_NAME L"Firewall.dat"
#define API_FIREWALL_FILE_VERSION 1

EFwProfiles CFirewall__Profiles[] = { EFwProfiles::Private, EFwProfiles::Public, EFwProfiles::Domain };

CFirewall::CFirewall()
	: m_FwRuleTemplates(&g_DefaultMemPool), m_FwTemplatesTree(&g_DefaultMemPool), m_AutoGuardTree(&g_DefaultMemPool)
{
	m_FwTemplatesTree.SetSimplePattern(true);
	m_AutoGuardTree.SetSimplePattern(true);

	m_pLog = new CWindowsFwLog();
	m_pLog->RegisterHandler(&CFirewall::OnFwLogEvent, this);

	m_pGuard = new CWindowsFwGuard();
	m_pGuard->RegisterHandler(&CFirewall::OnFwGuardEvent, this);

	m_RegWatcher.AddKey(L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\DomainProfile");
	m_RegWatcher.AddKey(L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\PublicProfile");
	m_RegWatcher.AddKey(L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile");
	//static const std::wstring FwRuleKey = L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\FirewallRules";
	//m_RegWatcher.AddKey(FwRuleKey);

	m_RegWatcher.RegisterHandler([&](const std::wstring& Key){
		//if(Key == FwRuleKey)
		//	m_UpdateAllRules = false;
		//else
			m_UpdateDefaultProfiles = 1;
	});

	m_RegWatcher.Start();
}

CFirewall::~CFirewall()
{
	m_RegWatcher.Stop();

	delete m_pLog;
	delete m_pGuard;

	CWindowsFirewall::Discard();
	CEventLogListener::FreeDll();
}

STATUS CFirewall::Init()
{
	Load();

	if(!m_pLog->Start((int)GetAuditPolicy(false)))
		theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed set Connection auditing policy");
	if(!m_pGuard->Start())
		theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed set Firewall Rule auditing policy");

	InitAutoGuard();

	if(!UpdateRules())
		theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed set load Firewall Rules");
	UpdateDefaults();

	PurgeExpired(true);

	RevertChanges();

	if(theCore->Config()->GetBool("Service", "AutoCleanUpFwTemplateRules", false))
		CleanupOldTemplateRules();

	if(theCore->Config()->GetBool("Service", "LoadWindowsFirewallLog", false))
		LoadFwLog();

	return OK;
}

STATUS CFirewall::Load()
{
	CBuffer Buffer;
	if (!ReadFile(theCore->GetDataFolder() + L"\\" API_FIREWALL_FILE_NAME, Buffer)) {
		theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_FIREWALL_FILE_NAME L" not found");
		return ERR(STATUS_NOT_FOUND);
	}

	StVariant Data;
	//try {
	auto ret = Data.FromPacket(&Buffer, true);
	//} catch (const CException&) {
	//	return ERR(STATUS_UNSUCCESSFUL);
	//}
	if (ret != StVariant::eErrNone) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_FIREWALL_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	if (Data[API_S_VERSION].To<uint32>() != API_FIREWALL_FILE_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_FIREWALL_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	LoadRules(Data[API_S_FW_RULES]);

	return OK;
}

STATUS CFirewall::Store()
{
	SVarWriteOpt Opts;
	Opts.Format = SVarWriteOpt::eMap;
	Opts.Flags = SVarWriteOpt::eSaveToFile;

	StVariant Data;
	Data[API_S_VERSION] = API_FIREWALL_FILE_VERSION;
	Data[API_S_FW_RULES] = SaveRules(Opts);

	CBuffer Buffer;
	Data.ToPacket(&Buffer);
	WriteFile(theCore->GetDataFolder() + L"\\" API_FIREWALL_FILE_NAME, Buffer);

	return OK;
}

STATUS CFirewall::LoadRules(const StVariant& Entries)
{
	std::unique_lock Lock(m_RulesMutex);

	for (uint32 i = 0; i < Entries.Count(); i++)
	{
		StVariant Entry = Entries[i];

		CFirewallRulePtr pFwRule = std::make_shared<CFirewallRule>();
		if(pFwRule->FromVariant(Entry))
			AddRuleUnsafe(pFwRule);
		else
			theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed to load Firewall Rule");
	}

	return OK;
}

StVariant CFirewall::SaveRules(const SVarWriteOpt& Opts)
{
	std::unique_lock Lock(m_RulesMutex);

	StVariant Rules;

	for (auto I : m_FwRules)
	{
		if(!(Opts.Flags & SVarWriteOpt::eSaveAll) && I.second->IsTemporary())
			continue;

		Rules.Append(I.second->ToVariant(Opts));
	}

	return Rules;
}

void CFirewall::Update()
{
	if (m_UpdateDefaultProfiles)
	{
		bool bEmit = (m_UpdateDefaultProfiles == 1);

		UpdateDefaults();

		if (bEmit)
		{
			StVariant Data;
			Data[API_V_FW_RULE_FILTER_MODE] = (uint32)GetFilteringMode();
			theCore->EmitEvent(ELogLevels::eWarning, eLogFwModeChanged, Data);
		}
	}

	if (m_bUpdateAutoGuard) {
		m_bUpdateAutoGuard = false;
		InitAutoGuard();
	}

	if (m_UpdateAllRules)
		UpdateRules();

	// purge rules once per minute
	if (m_LastRulePurge + 60 * 1000 < GetTickCount64())
		PurgeExpired(false);

	RevertChanges();
}

STATUS CFirewall::UpdateRules()
{
	auto ret = CWindowsFirewall::Instance()->LoadRules();
	if(ret.IsError())
		return ret;

	auto pRuleList = ret.GetValue();

	std::unique_lock Lock(m_RulesMutex);

	std::map<CFlexGuid, CFirewallRulePtr> OldFwRules = m_FwRules;

	for (auto pRule : *pRuleList) 
	{
		CFirewallRulePtr pFwRule;
		auto I = OldFwRules.find(pRule->Guid);
		if (I != OldFwRules.end())
		{
			pFwRule = I->second;
			OldFwRules.erase(I);
		}

		FWRuleChangedUnsafe(pRule, pFwRule);

		//CFirewallRulePtr pFwRule;
		//auto I = OldFwRules.find(pRule->Guid);
		//if (I != OldFwRules.end())
		//{
		//	pFwRule = I->second;
		//	OldFwRules.erase(I);
		//	if (!MatchProgramID(pRule, pFwRule))
		//	{
		//		RemoveRuleUnsafe(pFwRule);
		//		pFwRule.reset();
		//	}
		//}
		//
		//if (!pFwRule) 
		//{
		//	pFwRule = std::make_shared<CFirewallRule>(pRule);
		//	AddRuleUnsafe(pFwRule);
		//}
		//else
		//	pFwRule->Update(pRule);
	}

	for (auto I : OldFwRules)
	{
		if(I.second->IsTemplate())
			continue;
		FWRuleChangedUnsafe(NULL, I.second);
		//RemoveRuleUnsafe(I.second);
	}

	//
	// Check if rules are default Windows rules
	//

	for (auto I : m_FwRules) {
		if (I.second->GetSource() != EFwRuleSource::eUnknown)
			continue;
		if (IsDefaultWindowsRule(I.second))
			I.second->SetSource(EFwRuleSource::eWindowsDefault);
		else if (IsWindowsStoreRule(I.second))
			I.second->SetSource(EFwRuleSource::eWindowsStore);
	}


	// todo add an other watcher not reliable on security events by monitoring the registry key

	m_UpdateAllRules = false;
	return OK;
}

void CFirewall::UpdateDefaults()
{
	for (int i = 0; i < ARRAYSIZE(CFirewall__Profiles); i++) 
	{
		m_BlockAllInbound[i] = CWindowsFirewall::Instance()->GetBlockAllInboundTraffic(CFirewall__Profiles[i]);
		m_DefaultInboundAction[i] = CWindowsFirewall::Instance()->GetDefaultInboundAction(CFirewall__Profiles[i]);
		m_DefaultoutboundAction[i] = CWindowsFirewall::Instance()->GetDefaultOutboundAction(CFirewall__Profiles[i]);
	}

	m_UpdateDefaultProfiles = 0;
}

void CFirewall::PurgeExpired(bool All)
{
	std::unique_lock Lock(m_RulesMutex);
	for (auto I = m_FwRules.begin(); I != m_FwRules.end(); ) {
		auto J = I++;
		if (All ? J->second->IsTemporary() : J->second->IsExpired()) {
			theCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, 1, StrLine(L"Removed expired Firewall Rule: %s", J->second->GetFwRule()->Name.c_str()).c_str());
			DelRule(J->first);
		}
	}

	m_LastRulePurge = GetTickCount64();
}

void CFirewall::RevertChanges()
{
	std::unique_lock Lock(m_RulesMutex);

	bool bDelRuled = theCore->Config()->GetBool("Service", "DeleteRogueFwRules", false);

	for (auto& pFwRule : m_RulesToRemove)
	{
		STATUS Status;
		if (bDelRuled)
		{
			Status = CWindowsFirewall::Instance()->RemoveRule(pFwRule->GetGuidStr());
			if (Status)
				UpdateFWRuleUnsafe(NULL, pFwRule->GetGuidStr());
		}
		else 
		{
			if(!pFwRule->IsEnabled())
				continue;

			std::unique_lock Lock(pFwRule->m_Mutex);
			pFwRule->m_FwRule->Enabled = false;
			pFwRule->m_State = EFwRuleState::eUnapprovedDisabled;
			Status = UpdateRuleAndTest(pFwRule->m_FwRule);
		}
		if (Status.IsError())
			theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed to %s Firewall Rule %s: %s", bDelRuled ? L"Delete" : L"Disable", pFwRule->GetFwRule()->Name.c_str(), Status.GetMessageText());
		else
			theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, 1, L"%s Firewall Rule: %s", bDelRuled ? L"Deleted" : L"Disabled", pFwRule->GetFwRule()->Name.c_str());

		StVariant Data;
		Data[API_V_GUID] = pFwRule->GetGuidStr();
		Data[API_V_NAME] = pFwRule->GetName(); 
		Data[API_V_ID] = pFwRule->GetProgramID().ToVariant(SVarWriteOpt());
		Data[API_V_STATUS] = Status.GetStatus();
		theCore->EmitEvent(Status.IsError() ? ELogLevels::eInfo : ELogLevels::eError, eLogFwRuleRejected, Data);
	}
	m_RulesToRemove.clear();

	for (auto& pFwRuleBackup : m_RulesToRevert)
	{
		if(pFwRuleBackup->GetSetErrorCount() > 0)
			continue;

		STATUS Status = RestoreRule(pFwRuleBackup);

		StVariant Data;
		Data[API_V_GUID] = pFwRuleBackup->GetOriginalGuid();
		Data[API_V_NAME] = pFwRuleBackup->GetName(); 
		Data[API_V_ID] = pFwRuleBackup->GetProgramID().ToVariant(SVarWriteOpt());
		Data[API_V_STATUS] = Status.GetStatus();
		theCore->EmitEvent(Status.IsError() ? ELogLevels::eInfo : ELogLevels::eError, eLogFwRuleRestored, Data);
	}
	m_RulesToRevert.clear();
}

void CFirewall::CleanupOldTemplateRules()
{
	std::unique_lock Lock(m_RulesMutex);

	std::vector<CFirewallRulePtr> RulesToRemove;
	for (auto& [Guid, pFwRule] : m_FwRules)
	{
		if (pFwRule->GetSource() != EFwRuleSource::eAutoTemplate || pFwRule->IsTemplate())
			continue;

		std::wstring BinaryPath = pFwRule->GetBinaryPath();
		if (BinaryPath.empty() || BinaryPath.substr(0, 2) == L"\\\\") // Skip network paths
			continue;

		// Check if the file exists
		std::wstring NtPath = CNtPathMgr::Instance()->TranslateDosToNtPath(BinaryPath);
		if (!NtPath.empty())
		{
			FILE_BASIC_INFORMATION FileInfo = { 0 };
			if (!NT_SUCCESS(NtQueryAttributesFile(SNtObject(NtPath).Get(), &FileInfo)))
			{
				// File doesn't exist, mark for removal
				RulesToRemove.push_back(pFwRule);
			}
		}
	}

	Lock.unlock();


	for (auto& pFwRule : RulesToRemove)
	{
		std::wstring RuleName = pFwRule->GetName();
		std::wstring BinaryPath = pFwRule->GetBinaryPath();
		CFlexGuid RuleGuid = pFwRule->GetGuidStr();

		STATUS Status = CWindowsFirewall::Instance()->RemoveRule(RuleGuid.ToWString());
		if (Status.IsError())
		{
			theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to remove obsolete template rule '%s' for '%s'", RuleName.c_str(), BinaryPath.c_str());
		}
		else
		{
			Lock.lock();
			UpdateFWRuleUnsafe(NULL, RuleGuid);
			Lock.unlock();

			theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Removed obsolete template rule '%s' for '%s'", RuleName.c_str(), BinaryPath.c_str());

			// Emit event for the action list
			StVariant Data;
			Data[API_V_GUID] = pFwRule->GetGuidStr();
			Data[API_V_NAME] = RuleName;
			Data[API_V_STATUS] = Status.GetStatus();
			theCore->EmitEvent(ELogLevels::eInfo, eLogFwRuleRemoved, Data);
		}
	}

	if (RulesToRemove.size() > 0)
		theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Cleaned up %d obsolete template-generated firewall rule(s)", (int)RulesToRemove.size());
}

STATUS CFirewall::RestoreRule(const CFirewallRulePtr& pFwRuleBackup)
{
	SWindowsFwRulePtr pData = std::make_shared<SWindowsFwRule>(*pFwRuleBackup->GetFwRule());
	auto pFwRule = std::make_shared<CFirewallRule>(pData);
	pFwRule->Update(pFwRuleBackup);

	pFwRule->m_State = EFwRuleState::eApproved;
	pFwRule->m_FwRule->Guid = pFwRuleBackup->m_OriginalGuid; // pData->Guid = ...
	pFwRule->m_OriginalGuid.clear();

	// remove the changed active rule, if it exists
	auto F = m_FwRules.find(pFwRuleBackup->m_OriginalGuid);
	if (F != m_FwRules.end()) {
		auto pOldRule = F->second;
		RemoveRuleUnsafe(pOldRule);
	}

	STATUS Status = CWindowsFirewall::Instance()->UpdateRule(pData);

	SWindowsFwRulePtr pCurData;
	if (Status) {
		std::vector<std::wstring> RuleIds;
		RuleIds.push_back(pData->Guid);
		auto ret = CWindowsFirewall::Instance()->LoadRules(RuleIds);
		if(ret.GetValue()) 
			pCurData = (*ret.GetValue())[pData->Guid];	
		else
			Status = ERR(STATUS_UNSUCCESSFUL);
	}

	if (pCurData)
	{
		if (CFirewallRule::Match(pCurData, pData))
		{
			CFirewallRulePtr pNewRule = std::make_shared<CFirewallRule>(pCurData);
			AddRuleUnsafe(pNewRule);
			pNewRule->Update(pFwRule);

			RemoveRuleUnsafe(pFwRuleBackup);

			theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, 1, L"Reverted Firewall Rule: %s", pFwRule->GetFwRule()->Name.c_str());
		}
		else // set rule is not what we expect
		{
			pFwRuleBackup->IncrSetErrorCount();

			EmitChangeEvent(pFwRuleBackup->GetGuidStr(), pFwRuleBackup->GetFwRule()->Name, EConfigEvent::eModified, true); // reset backup rule in ui

			if (!MatchProgramID(pCurData, pFwRule) || pCurData->Action != pFwRule->GetFwRule()->Action)
			{
				// fatal missmatch, remove the new rule
				CWindowsFirewall::Instance()->RemoveRule(pCurData->Guid);

				Status = ERR(STATUS_UNSUCCESSFUL);
			}
			else
			{
				CFirewallRulePtr pNewRule = std::make_shared<CFirewallRule>(pCurData);
				AddRuleUnsafe(pNewRule);
				pNewRule->Update(pFwRule);
				pNewRule->SetDiverged(true); // kinda approved but nor really

				theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, 1, L"Missmatch reverting Firewall Rule %s: %s", pFwRule->GetFwRule()->Name.c_str(), Status.GetMessageText());
				return ERR(STATUS_ERR_RULE_DIVERGENT);
			}
		}
	}
	
	if(Status.IsError()) {
		pFwRuleBackup->IncrSetErrorCount();
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed to revert Firewall Rule %s: %s", pFwRule->GetFwRule()->Name.c_str(), Status.GetMessageText());
	}
	return Status;
}

void CFirewall::AddRuleUnsafe(const CFirewallRulePtr& pFwRule)
{
	if (pFwRule->IsTemplate())
	{
		m_FwRules.insert(std::make_pair(pFwRule->GetGuidStr(), pFwRule));

		FW::SharedPtr<CFwRuleTemplate> pTemplate = g_DefaultMemPool.New<CFwRuleTemplate>(pFwRule);
		CFlexGuid Guid = pFwRule->GetGuidStr();
		if (m_FwRuleTemplates.Insert(Guid, pTemplate, FW::EInsertMode::eNoReplace) == FW::EInsertResult::eOK)
		{
			if (pFwRule->IsEnabled())
				m_FwTemplatesTree.AddEntry(pTemplate);
		}
	}
	else
		AddRuleUnsafeImpl(pFwRule);

	theCore->ProgramManager()->AddFwRule(pFwRule);
}

void CFirewall::AddRuleUnsafeImpl(const CFirewallRulePtr& pFwRule)
{
	m_FwRules.insert(std::make_pair(pFwRule->GetGuidStr(), pFwRule));

	const CProgramID& ProgID = pFwRule->GetProgramID();

	if (!ProgID.GetFilePath().empty())
		m_FileRules.insert(std::make_pair(ProgID.GetFilePath(), pFwRule));
	if (!ProgID.GetServiceTag().empty())
		m_SvcRules.insert(std::make_pair(ProgID.GetServiceTag(), pFwRule));
	if (ProgID.GetType() == EProgramType::eAppPackage)
		m_AppRules.insert(std::make_pair(ProgID.GetAppContainerSid(), pFwRule));

	//if (ProgID.GetFilePath().empty() && ProgID.GetServiceTag().empty() && ProgID.GetAppContainerSid().empty())
	//	m_AllProgramsRules.insert(std::make_pair(pFwRule->GetGuid(), pFwRule));
}

void CFirewall::RemoveRuleUnsafe(const CFirewallRulePtr& pFwRule)
{
	if (pFwRule->IsTemplate())
	{
		m_FwRules.erase(pFwRule->GetGuidStr());

		CFlexGuid Guid(pFwRule->GetGuidStr());
		auto pEntry = m_FwRuleTemplates.Take(Guid);
		if (pEntry)
		{
			auto pFwRule = pEntry.Cast<CFwRuleTemplate>()->GetRule();
			if (pFwRule->IsEnabled())
				m_FwTemplatesTree.RemoveEntry(pEntry);
		}
	}
	else
		RemoveRuleUnsafeImpl(pFwRule);

	theCore->ProgramManager()->RemoveFwRule(pFwRule);
}

void CFirewall::RemoveRuleUnsafeImpl(const CFirewallRulePtr& pFwRule)
{
	m_FwRules.erase(pFwRule->GetGuidStr());

	const CProgramID& ProgID = pFwRule->GetProgramID();

	if (!ProgID.GetFilePath().empty())
		mmap_erase(m_FileRules, ProgID.GetFilePath(), pFwRule);
	if (!ProgID.GetServiceTag().empty())
		mmap_erase(m_SvcRules, ProgID.GetServiceTag(), pFwRule);
	if (ProgID.GetType() == EProgramType::eAppPackage)
		mmap_erase(m_AppRules, ProgID.GetAppContainerSid(), pFwRule);

	//if (ProgID.GetFilePath().empty() && ProgID.GetServiceTag().empty() && ProgID.GetAppContainerSid().empty())
	//	m_AllProgramsRules.erase(pFwRule->GetGuid())
}

std::map<CFlexGuid, CFirewallRulePtr> CFirewall::FindRules(const CProgramID& ID)
{
	std::unique_lock Lock(m_RulesMutex);

	std::map<CFlexGuid, CFirewallRulePtr> Found;

	switch (ID.GetType())
	{
	case EProgramType::eProgramFile:
		for (auto I = m_FileRules.find(ID.GetFilePath()); I != m_FileRules.end() && I->first == ID.GetFilePath(); ++I) {
			Found.insert(std::make_pair(I->second->GetGuidStr(), I->second));
		}
		break;
	case EProgramType::eWindowsService:
		for (auto I = m_SvcRules.find(ID.GetServiceTag()); I != m_SvcRules.end() && I->first == ID.GetServiceTag(); ++I) {
			if (!ID.GetFilePath().empty() && I->second->GetProgramID().GetFilePath() != ID.GetFilePath())
				continue;
			Found.insert(std::make_pair(I->second->GetGuidStr(), I->second));
		}
		break;
	case EProgramType::eAppPackage:
		for (auto I = m_AppRules.find(ID.GetAppContainerSid()); I != m_AppRules.end() && I->first == ID.GetAppContainerSid(); ++I) {
			Found.insert(std::make_pair(I->second->GetGuidStr(), I->second));
		}
		break;
	case EProgramType::eAllPrograms:
		//return m_AllProgramsRules;
		return m_FwRules;
	}
	return Found;
}

std::map<CFlexGuid, CFirewallRulePtr> CFirewall::GetAllRules()
{
	std::unique_lock Lock(m_RulesMutex);
	return m_FwRules;
}

std::vector<CFirewallRulePtr> CFirewall::GetRulesFromTemplate(const CFlexGuid& TemplateGuid)
{
	std::unique_lock Lock(m_RulesMutex);

	std::vector<CFirewallRulePtr> DerivedRules;
	for (const auto& [RuleGuid, pRule] : m_FwRules) {
		if (!pRule->IsTemplate() && pRule->GetTemplateGuid() == TemplateGuid.ToWString())
			DerivedRules.push_back(pRule);
	}
	return DerivedRules;
}

STATUS CFirewall::SetRule(const CFirewallRulePtr& pFwRule)
{
	if (pFwRule->IsTemplate())
	{
		SetTempalte(pFwRule);
		return OK;
	}

	CFirewallRulePtr pCurRule = GetRule(pFwRule->GetGuidStr());
	//bool bRemoveBackup = false;
	if(pCurRule)
	{
		if (pCurRule->IsBackup())
		{
			std::unique_lock Lock(m_RulesMutex);
			return RestoreRule(pCurRule);

			//if (!pFwRule->IsApproved()) // to Resotre a rule se must set the backup as approved
			//	return ERR(STATUS_INVALID_PARAMETER);
			//
			//// remove the changed active rule, if it exists
			//std::unique_lock Lock(m_RulesMutex);
			//UpdateFWRuleUnsafe(NULL, pFwRule->m_OriginalGuid);
			//
			//{
			//	std::unique_lock Lock(pFwRule->m_Mutex);
			//	pFwRule->m_FwRule->Guid = pFwRule->m_OriginalGuid;
			//	pFwRule->m_OriginalGuid.clear();
			//}
			//
			//bRemoveBackup = true;
			//
			//// continue with normal set operation
		}
		else if(pCurRule->Match(pFwRule->GetFwRule())) // compare with windows firewall rule, is same
		{
			pCurRule->Update(pFwRule); // changing private paremeter only
			return OK; 
		}
	}

	SWindowsFwRulePtr pData = pFwRule->GetFwRule();

	if (pData->Direction == EFwDirections::Bidirectional)
	{
		if(!pData->Guid.empty()) // Bidirectional is only valid when creating a new rule(s)
			return ERR(STATUS_INVALID_PARAMETER);
	
		SWindowsFwRulePtr pData2 = std::make_shared<struct SWindowsFwRule>(*pData); // copy the ruledata
		
		pData2->Direction = EFwDirections::Inbound;
		pData->Direction = EFwDirections::Outbound;
	
		std::unique_lock Lock(m_RulesMutex);
		STATUS Status = UpdateRuleAndTest(pData2); // this may set the GUID member
		if (!Status)
			return Status;
	
		CFirewallRulePtr pRule = UpdateFWRuleUnsafe(pData2, pData2->Guid);
		pRule->Update(pFwRule);
	}

	std::unique_lock Lock(m_RulesMutex);
	STATUS Status = UpdateRuleAndTest(pData); // this may set the GUID member
	if (!Status)
		return Status;

	CFirewallRulePtr pRule = UpdateFWRuleUnsafe(pData, pData->Guid);
	pRule->Update(pFwRule);

	//if (bRemoveBackup)
	//	RemoveRuleUnsafe(pCurRule);

	return OK;
}

STATUS CFirewall::UpdateRuleAndTest(const std::shared_ptr<struct SWindowsFwRule>& pData)
{
	STATUS Status = CWindowsFirewall::Instance()->UpdateRule(pData); // this may set the GUID member
	if (!Status)
		return Status;

	SWindowsFwRulePtr pCurData;
	std::vector<std::wstring> RuleIds;
	RuleIds.push_back(pData->Guid);
	auto ret = CWindowsFirewall::Instance()->LoadRules(RuleIds);
	if(ret.GetValue()) pCurData = (*ret.GetValue())[pData->Guid];

	if(!pCurData || !CFirewallRule::Match(pData, pCurData))
		return ERR(STATUS_ERR_RULE_DIVERGENT);
	return OK;
}

CFirewallRulePtr CFirewall::GetRule(const CFlexGuid& Guid)
{
	std::unique_lock Lock(m_RulesMutex);

	CFirewallRulePtr pFwRule;
	auto F = m_FwRules.find(Guid);
	if (F != m_FwRules.end())
		pFwRule = F->second;
	return pFwRule;
}

STATUS CFirewall::DelRule(const CFlexGuid& Guid)
{
	if (TryRemoveTemplate(Guid))
		return OK;

	std::unique_lock Lock(m_RulesMutex);
	STATUS Status = CWindowsFirewall::Instance()->RemoveRule(Guid.ToWString());
	if (!Status)
		return Status;

	CFirewallRulePtr pFwRule = UpdateFWRuleUnsafe(NULL, Guid);

	if(pFwRule)
		EmitChangeEvent(pFwRule->GetGuidStr(), pFwRule->GetName(), EConfigEvent::eRemoved, true);

	return OK;
}

void CFirewall::SetTempalte(const CFirewallRulePtr& pFwRule)
{
	SWindowsFwRulePtr pData = pFwRule->GetFwRule();

	bool bAdded = false;
	if (pData->Guid.empty()) {
		pData->Guid = MkGuid();
		bAdded = true;
	}

	std::unique_lock Lock(m_RulesMutex);
	CFirewallRulePtr pRule = UpdateFWRuleUnsafe(pData, pData->Guid);
	pRule->Update(pFwRule);
	Lock.unlock();

	EmitChangeEvent(pData->Guid, pData->Name, bAdded ? EConfigEvent::eAdded : EConfigEvent::eModified, true);
}

bool CFirewall::TryRemoveTemplate(const CFlexGuid& Guid)
{
	std::unique_lock Lock(m_RulesMutex);

	CFirewallRulePtr pFwRule;
	if (!Guid.IsNull()) {
		auto F = m_FwRules.find(Guid);
		if (F != m_FwRules.end())
			pFwRule = F->second;
	}
	if (!pFwRule || !pFwRule->IsTemplate())
		return false;
	
	SWindowsFwRulePtr pData = pFwRule->GetFwRule();

	RemoveRuleUnsafe(pFwRule);

	EmitChangeEvent(pData->Guid, pData->Name, EConfigEvent::eRemoved, true);

	return true;
}

bool CFirewall::MatchProgramID(const SWindowsFwRulePtr& pRule, const CFirewallRulePtr& pFwRule)
{
	std::wstring BinaryPath = pRule->BinaryPath;
	if (_wcsicmp(BinaryPath.c_str(), L"system") == 0)
		BinaryPath = CProcess::NtOsKernel_exe;

	const CProgramID& ProgID = pFwRule->GetProgramID();

	// match fails when anythign changes
	if (theCore->NormalizePath(BinaryPath) != ProgID.GetFilePath())
		return false;

	if (MkLower(pRule->ServiceTag) != ProgID.GetServiceTag())
		return false;

	std::wstring AppContainerSid = pRule->AppContainerSid;
	if(AppContainerSid.empty() && !pRule->PackageFamilyName.empty())
		AppContainerSid = GetAppContainerSidFromName(pRule->PackageFamilyName);
	if (ProgID.GetType() == EProgramType::eAppPackage)
	{
		if (_wcsicmp(AppContainerSid.c_str(), ProgID.GetAppContainerSid().c_str()) != 0)
			return false;
	}
	else if (!AppContainerSid.empty())
		return false;

	return true;
}

CFirewallRulePtr CFirewall::UpdateFWRuleUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule, const CFlexGuid& Guid)
{
	CFirewallRulePtr pFwRule;
	if (!Guid.IsNull()) {
		auto F = m_FwRules.find(Guid);
		if (F != m_FwRules.end())
			pFwRule = F->second;
	}

	if (pRule && pFwRule && MatchProgramID(pRule, pFwRule)) // false on remove as then pRule == NULL
	{
		if (pFwRule->IsTemplate() && pRule->Enabled != pFwRule->IsEnabled())
		{
			FW::SharedPtr<CFwRuleTemplate> pTemplate = m_FwRuleTemplates.FindValue(Guid);
			if (pTemplate) {
				if (pRule->Enabled)
					m_FwTemplatesTree.AddEntry(pTemplate);
				else
					m_FwTemplatesTree.RemoveEntry(pTemplate);
			}
#ifdef _DEBUG
			else
				DebugBreak();
#endif
		}

		pFwRule->Update(pRule);
	}
	else // when the rule changes the programs it applyes to we remove it and tehn add it
	{
		if (pFwRule) 
			RemoveRuleUnsafe(pFwRule);

		if (pRule) {
			pFwRule = std::make_shared<CFirewallRule>(pRule);
			AddRuleUnsafe(pFwRule);
		}
	}

	return pFwRule;
}

void CFirewall::InitAutoGuard()
{
	std::unique_lock Lock(m_RulesMutex);

	m_AutoGuardTree.Clear();

	auto AutoApproveList = SplitStr(theCore->Config()->GetValue("Service", "FwAutoApprove"), L"|");
	for (auto& AutoApprove : AutoApproveList)
	{
		if(AutoApprove.empty() || AutoApprove[0] == L'-')
			continue;

		FW::SharedPtr<CFwAutoGuardEntry> pEntry = g_DefaultMemPool.New<CFwAutoGuardEntry>(AutoApprove, CFwAutoGuardEntry::eApprove);
		m_AutoGuardTree.AddEntry(pEntry);
	}

	auto AutoRejectList = SplitStr(theCore->Config()->GetValue("Service", "FwAutoReject"), L"|");
	for (auto& AutoReject : AutoRejectList)
	{
		if(AutoReject.empty() || AutoReject[0] == L'-')
			continue;

		FW::SharedPtr<CFwAutoGuardEntry> pEntry = g_DefaultMemPool.New<CFwAutoGuardEntry>(AutoReject, CFwAutoGuardEntry::eReject);
		m_AutoGuardTree.AddEntry(pEntry);
	}
}

bool CFirewall::FWRuleChangedUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule, const CFirewallRulePtr& pFwRule, bool* pExpected)
{
	bool bProgramChanged = false;
	if (pRule && pFwRule && !(bProgramChanged = !MatchProgramID(pRule, pFwRule))) // false on remove as then pRule == NULL
	{
		if(pFwRule->Match(pRule))
			return false; // nothing changed
	}

	FW::SharedPtr<CFwAutoGuardEntry> pGuardEntry;

	if (!bProgramChanged) 
	{
		// TODO: add support for not only path based macthing

		CProgramID ProgID;
		if(pFwRule)
			ProgID = pFwRule->GetProgramID();
		else
		{
			std::wstring BinaryPath = pRule->BinaryPath;
			if (_wcsicmp(BinaryPath.c_str(), L"system") == 0)
				BinaryPath = CProcess::NtOsKernel_exe;

			//std::wstring AppContainerSid = pRule->AppContainerSid;
			//if(AppContainerSid.empty() && !pRule->PackageFamilyName.empty())
			//	AppContainerSid = GetAppContainerSidFromName(pRule->PackageFamilyName);

			ProgID = CProgramID::FromFw(BinaryPath, pRule->ServiceTag, L"");
		}

		auto& DosPath = ProgID.GetFilePath();
		pGuardEntry = m_AutoGuardTree.GetBestEntry(FW::StringW(&g_DefaultMemPool, DosPath.c_str())).Cast<CFwAutoGuardEntry>();
	}

	CFwAutoGuardEntry::EMode GuardAction = CFwAutoGuardEntry::eNone;
	if (pGuardEntry)
		GuardAction = pGuardEntry->GetMode();
	else if(theCore->Config()->GetBool("Service", "GuardFwRules", false))
		GuardAction = CFwAutoGuardEntry::eReject;

	if (pFwRule && pFwRule->IsBackup())
	{
		// we arrive here only from UpdateRules() 
		if (GuardAction == CFwAutoGuardEntry::eReject)
			m_RulesToRevert.insert(pFwRule);
		return false; // nothing changed
	}

	bool bExpected = false;
	if (pFwRule)  // modified or removed
	{
		ASSERT(!pFwRule->IsTemplate()); // should not happen

		if (!pFwRule->IsApproved() || GuardAction == CFwAutoGuardEntry::eApprove)
			RemoveRuleUnsafe(pFwRule); // a non approved rule chaneg or the rule is to be auto approved
		else
		{
			RemoveRuleUnsafeImpl(pFwRule);

			pFwRule->SetAsBackup(!pRule, bProgramChanged);

			m_FwRules.insert(std::make_pair(pFwRule->GetGuidStr(), pFwRule));

			EmitChangeEvent(pFwRule->GetGuidStr(), pFwRule->GetFwRule()->Name, EConfigEvent::eAdded, true); // event for the backup rule

			if (GuardAction == CFwAutoGuardEntry::eReject) 
			{
				m_RulesToRevert.insert(pFwRule);
				bExpected = true;
			}
		}
	}

	if (pRule) // added or modified
	{
		CFirewallRulePtr pNewRule = std::make_shared<CFirewallRule>(pRule);
		AddRuleUnsafe(pNewRule);
		if (GuardAction == CFwAutoGuardEntry::eApprove)
		{
			pNewRule->SetApproved(true);
			bExpected = true;

			StVariant Data;
			Data[API_V_GUID] = pNewRule->GetOriginalGuid();
			Data[API_V_NAME] = pNewRule->GetName(); 
			Data[API_V_ID] = pNewRule->GetProgramID().ToVariant(SVarWriteOpt());
			theCore->EmitEvent(ELogLevels::eInfo, eLogFwRuleApproved, Data);
		}
		else if (GuardAction == CFwAutoGuardEntry::eReject && !bExpected)
		{
			m_RulesToRemove.insert(pNewRule);
			bExpected = true;
		}
	}

	if (pExpected) *pExpected = bExpected;
	return true;
}

uint32 CFirewall::OnFwGuardEvent(const struct SWinFwGuardEvent* pEvent)
{
	SWindowsFwRulePtr pRule;
	if (pEvent->Type != EConfigEvent::eRemoved) {
		std::vector<std::wstring> RuleIds;
		RuleIds.push_back(pEvent->RuleId);
		auto ret = CWindowsFirewall::Instance()->LoadRules(RuleIds);
		if(ret.GetValue()) pRule = (*ret.GetValue())[pEvent->RuleId]; // CWindowsFirewall::Instance()->GetRule(pEvent->RuleId);
	}

	std::unique_lock Lock(m_RulesMutex);

	CFirewallRulePtr pFwRule;
	if (!pEvent->RuleId.empty()) {
		auto F = m_FwRules.find(pEvent->RuleId);
		if (F != m_FwRules.end())
			pFwRule = F->second;
	}

	if(!pRule && !pFwRule)
		return 0;

	bool bExpected = false;
	bool bChanged = FWRuleChangedUnsafe(pRule, pFwRule, &bExpected);

	Lock.unlock();

	if (bChanged) {

		StVariant Data;
		Data[API_V_GUID] = pEvent->RuleId;
		Data[API_V_NAME] = pEvent->RuleName; 
		if(pFwRule) 
			Data[API_V_ID] = pFwRule->GetProgramID().ToVariant(SVarWriteOpt());
		switch (pEvent->Type)
		{
		case EConfigEvent::eAdded:		theCore->EmitEvent(ELogLevels::eWarning, eLogFwRuleAdded, Data); break;
		case EConfigEvent::eModified:	theCore->EmitEvent(ELogLevels::eWarning, eLogFwRuleModified, Data); break;
		case EConfigEvent::eRemoved:	theCore->EmitEvent(ELogLevels::eWarning, eLogFwRuleRemoved, Data); break;
		}
	}

	EmitChangeEvent(pEvent->RuleId, pEvent->RuleName, pEvent->Type, !bChanged || bExpected);

	return 0;
}

uint32 CFirewall::OnFwLogEvent(const SWinFwLogEvent* pEvent)
{
	CSocketPtr pSocket = theCore->NetworkManager()->SocketList()->OnFwLogEvent(pEvent);

	ProcessFwEvent(pEvent, pSocket.get());

	return 0;
}

void CFirewall::EmitChangeEvent(const CFlexGuid& Guid, const std::wstring& Name, enum class EConfigEvent Event, bool bExpected)
{
	StVariant vEvent;
	vEvent[API_V_GUID] = Guid.ToVariant(false);
	vEvent[API_V_NAME] = Name;
	vEvent[API_V_EVENT_TYPE] = (uint32)Event;
	vEvent[API_V_EVENT_EXPECTED] = bExpected;

	theCore->BroadcastMessage(SVC_API_EVENT_FW_RULE_CHANGED, vEvent);
}

void CFirewall::ProcessFwEvent(const struct SWinFwLogEvent* pEvent, class CSocket* pSocket)
{
	CProcessPtr pProcess;
	CProgramFilePtr pProgram;
	CAppPackagePtr pApp;
	std::list<CWindowsServicePtr> Svcs;
	CHostNamePtr pRemoteHostName;

	SAdapterInfoPtr pNicInfo = theCore->NetworkManager()->GetAdapterInfoByIP(pEvent->LocalAddress);

	if (_wcsicmp(pEvent->ProcessFileName.c_str(), L"System") == 0)
	{
		pProcess = theCore->ProcessList()->GetProcess(NT_OS_KERNEL_PID, true);
		pProgram = theCore->ProgramManager()->GetNtOsKernel();
	}
	else if (pSocket != (CSocket*)-1)
	{
		pProcess = pSocket ? pSocket->GetProcess() : theCore->ProcessList()->GetProcess(pEvent->ProcessId, true); // todo socket should always have a process
		if (pProcess)
		{
			pProgram = pProcess->GetProgram();

			std::wstring AppSid = pProcess->GetAppContainerSid();
			if (!AppSid.empty())
				pApp = theCore->ProgramManager()->GetAppPackage(AppSid);

			std::set<std::wstring> SvcTags = pProcess->GetServices();
			for (auto I : SvcTags) {
				CWindowsServicePtr pSvc = theCore->ProgramManager()->GetService(I);
				if (pSvc)
					Svcs.push_back(pSvc);
			}
		}
	}

	if (pProcess)
		pProcess->UpdateLastNetActivity(pEvent->TimeStamp); // pEvent->Type

	//
	// If we were unable to determine exact informations by querying the running process,
	// fall back to finding the executable file item and decuce from it the likely app package and/or services
	//

	if (!pProgram) {
		pProgram = theCore->ProgramManager()->GetProgramFile(pEvent->ProcessFileName);
		pApp = pProgram->GetAppPackage();
		Svcs = pProgram->GetAllServices();
	}

	pProgram->UpdateLastFwActivity(pEvent->TimeStamp, pEvent->Type == EFwActions::Block);
	if (Svcs.size() == 1)
		Svcs.front()->UpdateLastFwActivity(pEvent->TimeStamp, pEvent->Type == EFwActions::Block);

	//
	// Evaluate firewall event
	//

	EFwEventStates EventState = EFwEventStates::Undefined;
	std::wstring ServiceTag;

	std::vector<CFlexGuid> AllowRules;
	std::vector<CFlexGuid> BlockRules;

	if (pSocket == (CSocket*)-1)
		EventState = EFwEventStates::FromLog;
	else
	{
		if (pSocket) {
			ServiceTag = pSocket->GetOwnerServiceName();
			pRemoteHostName = pSocket->GetRemoteHostName();
		}

		struct SRuleAction
		{
			bool bHasRules = false;
			bool bConflict = false;

			bool TestAndSetOrClear(SRuleMatch& Match, const struct SWinFwLogEvent* pEvent, bool ForceSet = true)
			{
				EFwActions& Result = Match.Result;
				if (Result == EFwActions::Undefined)
					return false;

				//
				// Block rules take precedence over allow rules, encountering allow rules on a blocked event is fine
				// howe ever encuntering a block rule on a allowed event is an issue
				// 
				// todo: fix me: rules are ineffective on locak host connections take this into account
				//

				if (Result == EFwActions::Block && pEvent->Type == EFwActions::Allow) // Conflict
				{
					if (!ForceSet)
					{
						//
						// if we dont know for sure this was an app package ...
						//	or
						// if we dont know for sure this socket belonged to a service ...
						// 
						// ... we assume it wasnt and discard the result on conflict
						//

						Result = EFwActions::Undefined; // Clear
						return false;
					}
					bConflict = true;
				}

				bHasRules = true;
				return true;
			}
		}
		RuleAction;

		//
		// We evaluate rules for the specific binary as well as rules applying to all processes
		// We also evaluate rules applying to the App package if one is present
		//

		SRuleMatch AllMatch = MatchRulesWithEvent(theCore->ProgramManager()->GetAllItem()->GetFwRules(), pEvent, AllowRules, BlockRules, pNicInfo);
		RuleAction.TestAndSetOrClear(AllMatch, pEvent);

		SRuleMatch FileMatch = MatchRulesWithEvent(pProgram->GetFwRules(), pEvent, AllowRules, BlockRules, pNicInfo);
		RuleAction.TestAndSetOrClear(FileMatch, pEvent);

		SRuleMatch AppMatch;
		if (pApp) AppMatch = MatchRulesWithEvent(pApp->GetFwRules(), pEvent, AllowRules, BlockRules, pNicInfo);
		RuleAction.TestAndSetOrClear(AppMatch, pEvent, pProcess != NULL);

		//
		// Evaluate service results, we idealy expect one result, but on windows 7 and some cases on later versions
		// one process may host multiple services, hence we may have a ambiguous situation.
		// In which case we pick the first service to have an action matching with the event
		// If we dont have any of these but one that failed, we have to pick that one.
		//

		std::list<std::pair<CWindowsServicePtr, SRuleAction>> SvcActionsPass;
		std::list<std::pair<CWindowsServicePtr, SRuleAction>> SvcActionsFail;
		for (auto pSvc : Svcs) {

			if (!ServiceTag.empty() && _wcsicmp(pSvc->GetServiceTag().c_str(), ServiceTag.c_str()) != 0)
				continue;

			SRuleMatch SvcMatch = MatchRulesWithEvent(pSvc->GetFwRules(), pEvent, AllowRules, BlockRules, pNicInfo);
			SRuleAction SvcAction = RuleAction; // copy
			if (SvcAction.TestAndSetOrClear(SvcMatch, pEvent, !ServiceTag.empty())) {
				if (SvcAction.bConflict)	SvcActionsFail.push_back(std::make_pair(pSvc, SvcAction));
				else						SvcActionsPass.push_back(std::make_pair(pSvc, SvcAction));
			}
		}
		if (SvcActionsPass.size() > 0)		RuleAction = SvcActionsPass.front().second;
		else if (SvcActionsFail.size() > 0)	RuleAction = SvcActionsFail.front().second;

		//
		// Check if there is no specific rule yet for the observed event
		//

		if (!RuleAction.bHasRules) {
			EFwActions DefaultAction = GetDefaultAction(pEvent->Direction, pNicInfo ? pNicInfo->Profile : theCore->NetworkManager()->GetDefaultProfile());
			if (DefaultAction != pEvent->Type) // Conflicts with default action
				RuleAction.bConflict = true;
		}

		if (RuleAction.bConflict)
			EventState = EFwEventStates::RuleError;
		else if (!RuleAction.bHasRules)
			EventState = EFwEventStates::UnRuled;
		else if (pEvent->Type == EFwActions::Allow)
			EventState = EFwEventStates::RuleAllowed;
		else if (pEvent->Type == EFwActions::Block)
			EventState = EFwEventStates::RuleBlocked;
	}

	if (EventState == EFwEventStates::UnRuled && theCore->Config()->GetBool("Service", "UseFwRuleTemplates", true))
	{
		auto DosPath = pProgram->GetPath();
		auto Entry = m_FwTemplatesTree.GetBestEntry(FW::StringW(&g_DefaultMemPool, DosPath.c_str()));
		if (Entry)
		{
			auto pFwRule = Entry.Cast<CFwRuleTemplate>()->GetRule();
			
			SWindowsFwRulePtr pTemplate = pFwRule->GetFwRule();

			if (pTemplate->Direction == EFwDirections::Bidirectional || pTemplate->Direction == pEvent->Direction)
			{
				SWindowsFwRulePtr pData = std::make_shared<struct SWindowsFwRule>(*pTemplate); // copy the ruledata

				pData->Guid.clear(); // clear the guid
				pData->BinaryPath = DosPath; // set exact pact
				pData->Name = StrReplaceAll(pData->Name, L"MajorPrivacy-Template", L"MajorPrivacy-Rule");
				if(pData->Direction == EFwDirections::Bidirectional)
					pData->Direction = pEvent->Direction;

				std::unique_lock Lock(m_RulesMutex);
				STATUS Status = UpdateRuleAndTest(pData); // this may set the GUID member
				if (Status.IsError())
					theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to create Firewall rule form template: %s", pFwRule->GetFwRule()->Name.c_str());
				else
				{
					CFirewallRulePtr pRule = UpdateFWRuleUnsafe(pData, pData->Guid);
					pRule->Update(pFwRule);
					pRule->SetApproved(true);
					pRule->SetSource(EFwRuleSource::eAutoTemplate);
					pRule->SetTemplateGuid(pFwRule->GetGuidStr());

					EventState = EFwEventStates::RuleGenerated;

					StVariant Data;
					Data[API_V_GUID] = pRule->GetGuidStr();
					Data[API_V_NAME] = pRule->GetName();
					theCore->EmitEvent(ELogLevels::eInfo, eLogFwRuleGenerated, Data);
				}
			}
		}
	}

#ifdef DEF_USE_POOL
	CNetLogEntryPtr pLogEntry = pProgram->Allocator()->New<CNetLogEntry>(pEvent, EventState, pRemoteHostName, AllowRules, BlockRules, pEvent->ProcessId, ServiceTag);
	uint64 LogIndex = pProgram->AddTraceLogEntry(pLogEntry, ETraceLogs::eNetLog);
#else
	CNetLogEntryPtr pLogEntry = std::make_shared<CNetLogEntry>(pEvent, EventState, pRemoteHostName, AllowRules, BlockRules, pEvent->ProcessId, ServiceTag);
	uint64 LogIndex = pProgram->AddTraceLogEntry(pLogEntry, ETraceLogs::eNetLog);
#endif

	StVariant Event;
	//Event[SVC_API_EVENT_TYPE]	= SVC_API_EVENT_FW_EVENT;
	Event[API_V_ID]		= pProgram->GetID().ToVariant(SVarWriteOpt());
	Event[API_V_EVENT_INDEX]	= LogIndex;
	Event[API_V_EVENT_DATA]		= pLogEntry->ToVariant();

	theCore->BroadcastMessage(SVC_API_EVENT_NET_ACTIVITY, Event); //, pProgram); - always bradcast network activity wven if the process is not actively watched
}

EFwActions CFirewall::GetDefaultAction(EFwDirections Direction, EFwProfiles Profiles)
{
    // Note: FwProfile should have only one bit set, but just in case we can haldnel more than one, but not accurately
    int BlockRules = 0;
    int AllowRules = 0;

	for (int i = 0; i < ARRAYSIZE(CFirewall__Profiles); i++)
	{
		if (((int)Profiles & (int)CFirewall__Profiles[i]) == 0)
			continue;

		switch (Direction)
		{
		case EFwDirections::Inbound:
			if (m_BlockAllInbound[i])
				BlockRules++;
			else
				switch (m_DefaultInboundAction[i])
				{
				case EFwActions::Allow: AllowRules++; break;
				case EFwActions::Block: BlockRules++; break;
				}
			break;
		case EFwDirections::Outbound:
			switch (m_DefaultoutboundAction[i])
			{
			case EFwActions::Allow: AllowRules++; break;
			case EFwActions::Block: BlockRules++; break;
			}
			break;
		}
	}

    // Note: block rules take precedence
    if (BlockRules > 0)
        return EFwActions::Block;
    if (AllowRules > 0)
        return EFwActions::Allow;
    return EFwActions::Undefined;
}

CFirewall::SRuleMatch CFirewall::MatchRulesWithEvent(const std::set<CFirewallRulePtr>& Rules, const struct SWinFwLogEvent* pEvent, std::vector<CFlexGuid>& AllowRules, std::vector<CFlexGuid>& BlockRules, std::shared_ptr<struct SAdapterInfo> pNicInfo)
{
	SRuleMatch MatchingRules;

	for(auto pRule: Rules)
    {
		if(pRule->IsBackup())
			continue; // Backup rules are not active
		ASSERT(!pRule->IsTemplate());

		SWindowsFwRulePtr pData = pRule->GetFwRule();

        if (!pData->Enabled)
            continue;
        if (pData->Direction != pEvent->Direction)
            continue;
        if (pData->Protocol != EFwKnownProtocols::Any && pEvent->ProtocolType != pData->Protocol)
            continue;
        if (pNicInfo && ((int)pNicInfo->Profile & pData->Profile) == 0)
            continue;
        if (pNicInfo && pData->Interface != (int)EFwInterfaces::All && (int)pNicInfo->Type != pData->Interface)
            continue;
        if (!MatchEndpoint(pData->RemoteAddresses, pData->RemotePorts, pEvent->RemoteAddress, pEvent->RemotePort, pNicInfo))
            continue;
        if (!MatchEndpoint(pData->LocalAddresses, pData->LocalPorts, pEvent->RemoteAddress, pEvent->RemotePort, pNicInfo))
            continue;

        pRule->IncrHitCount();

		if (pData->Action == EFwActions::Allow) {
			MatchingRules.AllowCount++;
			//MatchingRules.AllowRules.push_back(pRule);
			AllowRules.push_back(pData->Guid);
		}
		else if (pData->Action == EFwActions::Block) {
			MatchingRules.BlockCount++;
			//MatchingRules.BlockRules.push_back(pRule);
			BlockRules.push_back(pData->Guid);
		}
    }

    // Note: block rules take precedence
	if (MatchingRules.BlockCount > 0)
		MatchingRules.Result = EFwActions::Block;
    else if (MatchingRules.AllowCount > 0)
        MatchingRules.Result = EFwActions::Allow;
	return MatchingRules;
}

bool CFirewall::IsEmptyOrStar(const std::vector<std::wstring>& value)
{
	return value.empty() || (value.size() == 1 && (value[0].empty() || value[0] == L"*"));
}

bool CFirewall::MatchPort(uint16 Port, const std::vector<std::wstring>& Ports)
{
    for (auto Range: Ports)
    {
		auto PortRange = SplitStr(Range, L"-");
        if (PortRange.size() == 1)
        {
			uint16 SinglePort = _wtoi(PortRange[0].c_str());
            if (!SinglePort)
            {
                // todo: xxx some rule port values are strings :(
                // how can we test that?!
            }
            else if (SinglePort == Port)
                return true;
        }
        else if (PortRange.size() == 2)
        {
            uint16 BeginPort = _wtoi(PortRange[0].c_str());
            uint16 EndPort = _wtoi(PortRange[1].c_str());
            if (BeginPort && EndPort)
            {
                if (BeginPort <= Port && Port <= EndPort)
                    return true;
            }
        }
    }
    return false;
}

CUInt128 CFirewall__IpToInt(const CAddress& Address)
{
	CUInt128 IP;
	if(Address.Type() == CAddress::EAF::INET)
		IP = CUInt128(Address.ToIPv4());
	else if(Address.Type() == CAddress::EAF::INET6)
		IP = CUInt128(Address.Data());
	return IP;
}

bool CFirewall::MatchAddress(const CAddress& Address, const std::vector<std::wstring>& Addresses, std::shared_ptr<struct SAdapterInfo> pNicInfo)
{
    CUInt128 uIP = CFirewall__IpToInt(Address);

    for (auto Range: Addresses)
    {
        auto IPRange = SplitStr(Range, L"-");
        if (IPRange.size() == 1)
        {
			size_t pos = IPRange[0].find(L"/");
            if (pos != -1) // ip/net
            {
				auto IPNet = Split2(IPRange[0], L"/");
            
				CAddress BeginIP(IPNet.first);
				if (BeginIP.Type() == Address.Type())
				{
					CUInt128 uBeginIP = CFirewall__IpToInt(BeginIP);
					int shift = _wtoi(IPNet.second.c_str());
					CUInt128 uEndIP = uBeginIP + ((CUInt128(1) << (32 - shift)) - 1);

					if (uBeginIP <= uIP && uIP <= uEndIP)
						return true;
				}
            }
            else
            {
                std::vector<std::wstring> SpecialAddresses = GetSpecialNet(IPRange[0], pNicInfo);
                if (!SpecialAddresses.empty())
                    return MatchAddress(Address, SpecialAddresses);
                else
                {
					CAddress SingleIP(IPRange[0]);
					if (SingleIP.Type() == Address.Type())
					{
						CUInt128 uSingleIP = CFirewall__IpToInt(SingleIP);
						if (uSingleIP == uIP)
							return true;
					}
                }
            }
        }
        else if (IPRange.size() == 2)
        {
            CAddress BeginIP(IPRange[0]);
			CAddress EndIP(IPRange[1]);
			if (BeginIP.Type() == Address.Type() && BeginIP.Type() == EndIP.Type())
			{
				CUInt128 uBeginIP = CFirewall__IpToInt(BeginIP);
				CUInt128 uEndIP = CFirewall__IpToInt(EndIP);
				if (uBeginIP <= uIP && uIP <= uEndIP)
					return true;
			}
        }
    }
    return false;
}

bool CFirewall::MatchEndpoint(const std::vector<std::wstring>& Addresses, const std::vector<std::wstring>& Ports, const CAddress& Address, uint16 Port, std::shared_ptr<struct SAdapterInfo> pNicInfo)
{
    if (!IsEmptyOrStar(Ports) && !MatchPort(Port, Ports))
        return false;
    if (!Address.IsNull() && !IsEmptyOrStar(Addresses) && !MatchAddress(Address, Addresses, pNicInfo))
        return false;
    return true;
}

std::vector<std::wstring> CFirewall__CopyStrIPs(const std::list<CAddress>& Addresses)
{
	std::vector<std::wstring> IpRanges;
	for (auto Address : Addresses)
		IpRanges.push_back(Address.ToWString());
	return IpRanges;
}

std::vector<std::wstring> CFirewall::GetSpecialNet(std::wstring SubNet, std::shared_ptr<struct SAdapterInfo> pNicInfo)
{
    std::vector<std::wstring> IpRanges;
    if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordLocalSubnet) == 0 || _wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordIntrAnet) == 0)
    {
        // todo: create the list base on NicInfo.Addresses
		// 
        // IPv4
        IpRanges.push_back(L"10.0.0.0-10.255.255.255");
        IpRanges.push_back(L"127.0.0.0-127.255.255.255"); // localhost
        IpRanges.push_back(L"172.16.0.0-172.31.255.255");
        IpRanges.push_back(L"192.168.0.0-192.168.255.255");
        IpRanges.push_back(L"224.0.0.0-239.255.255.255"); // multicast

        // IPv6
        IpRanges.push_back(L"::1"); // localhost
        IpRanges.push_back(L"fc00::-fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"); // Unique local address
        IpRanges.push_back(L"fe80::-fe80::ffff:ffff:ffff:ffff"); //IpRanges.push_back(L"fe80::-febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff"); // Link-local address
        IpRanges.push_back(L"ff00::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"); // multicast
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordIntErnet) == 0)
    {
        // todo: ceate the list base on NicInfo.Addresses
        // IPv4
        IpRanges.push_back(L"0.0.0.0-9.255.255.255");
        // 10.0.0.0 - 10.255.255.255
        IpRanges.push_back(L"11.0.0.0-126.255.255.255");
        // 127.0.0.0 - 127.255.255.255 // localhost
        IpRanges.push_back(L"128.0.0.0-172.15.255.255");
        // 172.16.0.0 - 172.31.255.255
        IpRanges.push_back(L"172.32.0.0-192.167.255.255");
        // 192.168.0.0 - 192.168.255.255
        IpRanges.push_back(L"192.169.0.0-223.255.255.255");
        // 224.0.0.0-239.255.255.255 // multicast
        IpRanges.push_back(L"240.0.0.0-255.255.255.255");

        // ipv6
        //"::1" // localhost
        IpRanges.push_back(L"::2-fc00::");
        //"fc00::-fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" // Unique local address
        IpRanges.push_back(L"fc00::ffff:ffff:ffff:ffff-fe80::");
        //"fe80::-fe80::ffff:ffff:ffff:ffff" // fe80::-febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff // Link-local address 
        IpRanges.push_back(L"fe80::ffff:ffff:ffff:ffff-ff00::");
        //"ff00::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" // multicast
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordDNS) == 0)
    {
        IpRanges = CFirewall__CopyStrIPs(pNicInfo->DnsAddresses);
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordDHCP) == 0)
    {
        IpRanges = CFirewall__CopyStrIPs(pNicInfo->DhcpServerAddresses);
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordWINS) == 0)
    {
        IpRanges = CFirewall__CopyStrIPs(pNicInfo->WinsServersAddresses);
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordDefaultGateway) == 0)
    {
        IpRanges = CFirewall__CopyStrIPs(pNicInfo->GatewayAddresses);
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordRmtIntrAnet) == 0
            || _wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordPly2Renders) == 0
            || _wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordCaptivePortal) == 0)
    {
        // todo: !!!!
    }
    return IpRanges;
}

void CFirewall::LoadFwLog()
{
	// todo move to separate thread

	std::wstring XmlQuery = CWindowsFwLog::GetXmlQuery();
	
	CScopedHandle hQuery(CEventLogCallback::EvtQuery(NULL, NULL, XmlQuery.c_str(), EvtQueryChannelPath), CEventLogCallback::EvtClose);
	if (!hQuery)
		return;
	
	for(;;)
	{
		EVT_HANDLE hEvent[100];

		DWORD count = 0;
		if (!CEventLogCallback::EvtNext(hQuery, ARRAYSIZE(hEvent), hEvent, 0, 0, &count)) 
		{
			DWORD status = GetLastError();
			if (status == ERROR_NO_MORE_ITEMS)
				break;
			//else {
			//	// error
			//	break;
			//}
		}

		for (DWORD i = 0; i < count; i++)
		{
			//auto test = CEventLogCallback::GetEventXml(hEvent[i]);
				
			SWinFwLogEvent Event;
			CWindowsFwLog::ReadEvent(hEvent[i], Event);

			ProcessFwEvent(&Event, (CSocket*)-1);

			CEventLogCallback::EvtClose(hEvent[i]);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// Firewall profile Configuration

std::vector<EFwProfiles> FwProfiles = { EFwProfiles::Private, EFwProfiles::Public, EFwProfiles::Domain };

void SetDefaultOutboundAction(EFwActions Action)
{
	for (auto& profile : FwProfiles)
	{
		CWindowsFirewall::Instance()->SetDefaultOutboundAction(profile, Action);
	}
}

bool TestDefaultOutboundAction(EFwActions Action)
{
	for (auto& profile : FwProfiles)
	{
		if (CWindowsFirewall::Instance()->GetDefaultOutboundAction(profile) != Action)
			return false;
	}
	return true;
}

void SetBlockAllInboundTraffic(bool Block)
{
	for (auto& profile : FwProfiles)
	{
		CWindowsFirewall::Instance()->SetBlockAllInboundTraffic(profile, Block);
	}
}

bool TestBlockAllInboundTraffic(bool Block)
{
	for (auto& profile : FwProfiles)
	{
		if (CWindowsFirewall::Instance()->GetBlockAllInboundTraffic(profile) != Block)
			return false;
	}
	return true;
}

void SetFirewallEnabled(bool Enable)
{
	for (auto& profile : FwProfiles)
	{
		CWindowsFirewall::Instance()->SetFirewallEnabled(profile, Enable);
	}
}

bool TestFirewallEnabled(bool Enable)
{
	for (auto& profile : FwProfiles)
	{
		if (CWindowsFirewall::Instance()->GetFirewallEnabled(profile) != Enable)
			return false;
	}
	return true;
}

STATUS CFirewall::SetFilteringMode(FwFilteringModes Mode)
{
	bool DoNotAllowExceptions = RegQueryDWord(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile", L"DoNotAllowExceptions", 0) != 0;

	SetFirewallEnabled(Mode != FwFilteringModes::NoFiltering);
	switch (Mode)
	{
	case FwFilteringModes::NoFiltering:
		break;
	case FwFilteringModes::BlockList:
		SetBlockAllInboundTraffic(DoNotAllowExceptions);
		SetDefaultOutboundAction(EFwActions::Allow);
		break;
	case FwFilteringModes::AllowList:
		SetBlockAllInboundTraffic(DoNotAllowExceptions);
		SetDefaultOutboundAction(EFwActions::Block);
		break;
	//case FwFilteringModes::BlockAll:
	//	BlockAllTrafic();
	//	break;
	default: return ERR(STATUS_INVALID_PARAMETER);
	}

	return OK;
}

FwFilteringModes CFirewall::GetFilteringMode()
{
	if (TestFirewallEnabled(true))
	{
		if (TestDefaultOutboundAction(EFwActions::Allow))
			return FwFilteringModes::BlockList;

		if (TestDefaultOutboundAction(EFwActions::Block))
			return FwFilteringModes::AllowList;
	}
	return FwFilteringModes::NoFiltering;
}

STATUS CFirewall::SetAuditPolicy(FwAuditPolicy Mode)
{
	std::wstring AuditPolicy;
	switch (Mode)
	{
	case FwAuditPolicy::All:		AuditPolicy = L"All"; break;
	case FwAuditPolicy::Blocked:	AuditPolicy = L"Blocked"; break;
	case FwAuditPolicy::Allowed:	AuditPolicy = L"Allowed"; break;
	case FwAuditPolicy::Off:		AuditPolicy = L"Off"; break;
	}
	theCore->Config()->SetValue("Firewall", "AuditPolicy", AuditPolicy);

	return m_pLog->UpdatePolicy((int)Mode);
}

FwAuditPolicy CFirewall::GetAuditPolicy(bool bCurrent)
{
	if(bCurrent)
		return (FwAuditPolicy)m_pLog->GetCurrentPolicy();

	std::wstring AuditPolicy = theCore->Config()->GetValue("Firewall", "AuditPolicy", L"All");
	if (AuditPolicy == L"All")		return FwAuditPolicy::All;
	if (AuditPolicy == L"Blocked")	return FwAuditPolicy::Blocked;
	if (AuditPolicy == L"Allowed")	return FwAuditPolicy::Allowed;
	return FwAuditPolicy::Off;
}


bool CFirewall::IsDefaultWindowsRule(const CFirewallRulePtr& pFwRule) const
{
	const wchar_t* DefaultRules[] = {
		L"FPS-ICMP6-ERQ-In-V2",
		L"FPS-LLMNR-Out-UDP",
		L"FPS-LLMNR-In-UDP",
		L"FPS-ICMP4-ERQ-In-NoScope",
		L"FPS-NB_Datagram-In-UDP",
		L"FPS-NB_Name-Out-UDP",
		L"FPS-ICMP6-ERQ-In-NoScope",
		L"FPS-RPCSS-In-TCP-V2",
		L"FPS-ICMP4-ERQ-Out-V2",
		L"FPS-ICMP6-ERQ-In",
		L"FPS-NB_Session-Out-TCP-NoScope",
		L"FPS-NB_Datagram-Out-UDP",
		L"FPS-ICMP4-ERQ-In-V2",
		L"FPS-SpoolWorker-In-TCP-NoScope",
		L"FPS-ICMP6-ERQ-Out-NoScope",
		L"FPS-SMB-In-TCP-NoScope",
		L"FPS-SMB-Out-TCP-V2",
		L"FPS-SMB-In-TCP",
		L"FPS-SpoolWorker-In-TCP-V2",
		L"FPS-ICMP4-ERQ-In",
		L"FPS-NB_Session-In-TCP",
		L"FPS-SpoolSvc-In-TCP-V2",
		L"FPS-NB_Name-In-UDP-NoScope",
		L"FPS-ICMP6-ERQ-Out",
		L"FPS-RPCSS-In-TCP",
		L"FPS-LLMNR-Out-UDP-V2",
		L"FPS-LLMNR-In-UDP-V2",
		L"FPS-NB_Session-Out-TCP",
		L"FPS-NB_Name-In-UDP",
		L"FPS-NB_Datagram-In-UDP-NoScope",
		L"FPS-SpoolWorker-In-TCP",
		L"FPS-SMB-In-TCP-V2",
		L"FPS-SMB-Out-TCP-NoScope",
		L"FPS-ICMP4-ERQ-Out-NoScope",
		L"FPS-SMB-Out-TCP",
		L"FPS-NB_Name-Out-UDP-NoScope",
		L"FPS-ICMP4-ERQ-Out",
		L"FPS-SpoolSvc-In-TCP",
		L"FPS-NB_Datagram-Out-UDP-NoScope",
		L"FPS-NB_Session-In-TCP-NoScope",
		L"FPS-ICMP6-ERQ-Out-V2",
		L"FPS-SpoolSvc-In-TCP-NoScope",
		L"FPS-RPCSS-In-TCP-NoScope",
		L"WiFiDirect-KM-Driver-In-UDP",
		L"Microsoft-Windows-WLANSvc-ASP-CP-Out",
		L"Wininit-Shutdown-In-Rule-TCP-RPC-EPMapper",
		L"WiFiDirect-KM-Driver-Out-TCP",
		L"WiFiDirect-KM-Driver-In-TCP",
		L"Microsoft-Windows-WLANSvc-ASP-CP-In",
		L"Wininit-Shutdown-In-Rule-TCP-RPC",
		L"WiFiDirect-KM-Driver-Out-UDP",
		L"MCX-QWave-In-TCP",
		L"WMPNSS-Out-TCP-NoScope",
		L"WMPNSS-QWave-In-UDP-NoScope",
		L"Microsoft-Windows-PeerDist-WSD-Out",
		L"WMP-Out-TCP",
		L"WMP-Out-UDP",
		L"Microsoft-Windows-PeerDist-HttpTrans-In",
		L"WMPNSS-HTTPSTR-In-TCP-NoScope",
		L"WMPNSS-WMP-Out-UDP",
		L"PlayTo-In-RTSP-PlayToScope",
		L"PlayTo-HTTPSTR-In-TCP-PlayToScope",
		L"PlayTo-HTTPSTR-In-TCP-NoScope",
		L"WMP-Out-TCP-x86",
		L"WMP-Out-UDP-x86",
		L"MCX-PlayTo-Out-TCP",
		L"MCX-SSDPSrv-Out-UDP",
		L"Microsoft-Windows-PeerDist-HttpTrans-Out",
		L"RemoteDesktop-UserMode-In-UDP",
		L"RemoteDesktop-UserMode-In-TCP",
		L"MCX-PlayTo-In-TCP",
		L"SPPSVC-In-TCP-NoScope",
		L"PlayTo-In-UDP-PlayToScope",
		L"WMPNSS-Out-UDP-NoScope",
		L"PlayTo-In-RTSP-LocalSubnetScope",
		L"MCX-PlayTo-Out-UDP",
		L"MCX-Out-UDP",
		L"MCX-Out-TCP",
		L"PlayTo-UPnP-Events-PlayToScope",
		L"WPDMTP-UPnP-Out-TCP",
		L"WPDMTP-SSDPSrv-Out-UDP",
		L"PlayTo-Out-UDP-NoScope",
		L"WPDMTP-SSDPSrv-In-UDP",
		L"MCX-McrMgr-Out-TCP",
		L"PlayTo-SSDP-Discovery-PlayToScope",
		L"Microsoft-Windows-PeerDist-HostedClient-Out",
		L"WMPNSS-Out-TCP",
		L"MCX-MCX2SVC-Out-TCP",
		L"MCX-FDPHost-Out-TCP",
		L"WMPNSS-QWave-Out-TCP-NoScope",
		L"WMPNSS-QWave-Out-UDP-NoScope",
		L"RemoteDesktop-Shadow-In-TCP",
		L"MCX-SSDPSrv-In-UDP",
		L"PlayTo-QWave-In-TCP-PlayToScope",
		L"PlayTo-HTTPSTR-In-TCP-LocalSubnetScope",
		L"WMPNSS-QWave-Out-UDP",
		L"WMPNSS-HTTPSTR-In-TCP",
		L"WMPNSS-Out-UDP",
		L"WPDMTP-UPnPHost-Out-TCP",
		L"WMPNSS-HTTPSTR-Out-TCP",
		L"WPDMTP-UPnPHost-In-TCP",
		L"WMPNSS-SSDPSrv-In-UDP",
		L"FPSSMBD-iWARP-In-TCP",
		L"WMPNSS-UPnPHost-Out-TCP",
		L"WMPNSS-WMP-Out-TCP-NoScope",
		L"WMPNSS-QWave-Out-TCP",
		L"SPPSVC-In-TCP",
		L"WMPNSS-SSDPSrv-Out-UDP",
		L"MCX-TERMSRV-In-TCP",
		L"RemoteDesktop-In-TCP-WSS",
		L"PlayTo-QWave-Out-UDP-PlayToScope",
		L"MCX-QWave-Out-TCP",
		L"MCX-QWave-Out-UDP",
		L"MCX-In-TCP",
		L"WMPNSS-WMP-In-UDP",
		L"CloudIdSvc-Allow-HTTPS-Out-TCP",
		L"WMPNSS-HTTPSTR-Out-TCP-NoScope",
		L"PlayTo-QWave-In-UDP-PlayToScope",
		L"WMPNSS-WMP-In-UDP-NoScope",
		L"WPDMTP-Out-TCP-NoScope",
		L"WMPNSS-In-TCP",
		L"WMPNSS-In-UDP",
		L"WMP-In-UDP-x86",
		L"MCX-In-UDP",
		L"PlayTo-In-RTSP-NoScope",
		L"WMPNSS-QWave-In-TCP-NoScope",
		L"WMPNSS-In-TCP-NoScope",
		L"WMPNSS-In-UDP-NoScope",
		L"WMPNSS-UPnP-Out-TCP",
		L"Microsoft-Windows-PeerDist-HostedServer-In",
		L"WMPNSS-WMP-Out-UDP-NoScope",
		L"WMPNSS-UPnPHost-In-TCP",
		L"PlayTo-In-UDP-NoScope",
		L"WMPNSS-QWave-In-UDP",
		L"WMPNSS-QWave-In-TCP",
		L"MCX-Prov-Out-TCP",
		L"PlayTo-Out-UDP-LocalSubnetScope",
		L"PlayTo-Out-UDP-PlayToScope",
		L"MCX-QWave-In-UDP",
		L"WPDMTP-Out-TCP",
		L"MCX-HTTPSTR-In-TCP",
		L"PlayTo-In-UDP-LocalSubnetScope",
		L"Microsoft-Windows-PeerDist-WSD-In",
		L"RemoteDesktop-In-TCP-WS",
		L"PlayTo-QWave-Out-TCP-PlayToScope",
		L"Microsoft-Windows-PeerDist-HostedServer-Out",
		L"WMP-In-UDP",
		L"WMPNSS-WMP-Out-TCP",
		L"MDNS-In-UDP-Public-Active",
		L"CoreNet-Diag-ICMP6-EchoRequest-In-NoScope",
		L"CoreNet-GP-Out-TCP",
		L"NETDIS-NB_Name-Out-UDP-NoScope",
		L"MSDTC-In-TCP-NoScope",
		L"RRAS-GRE-Out",
		L"NETDIS-NB_Name-Out-UDP-Active",
		L"WINRM-HTTP-Compat-In-TCP-NoScope",
		L"CoreNet-Diag-ICMP4-EchoRequest-In",
		L"NETDIS-UPnP-Out-TCP-Active",
		L"RemoteAssistance-In-TCP-EdgeScope-Active",
		L"Collab-P2PHost-In-TCP",
		L"MDNS-In-UDP-Domain-Active",
		L"CoreNet-ICMP6-TE-In",
		L"Netlogon-TCP-RPC-In",
		L"WMI-WINMGMT-Out-TCP-NoScope",
		L"RVM-VDS-In-TCP-NoScope",
		L"CoreNet-DHCPV6-Out",
		L"MSDTC-In-TCP",
		L"DIAL-Protocol-Server-HTTPSTR-In-TCP-LocalSubnetScope",
		L"SSTP-IN-TCP",
		L"CoreNet-Diag-ICMP4-EchoRequest-In-NoScope",
		L"CoreNet-ICMP6-LR-In",
		L"Microsoft-Windows-DeviceManagement-OmaDmClient-TCP-Out",
		L"Collab-PNRP-SSDPSrv-In-UDP",
		L"CoreNet-ICMP6-LQ-In",
		L"RemoteEventLogSvc-In-TCP-NoScope",
		L"NETDIS-FDPHOST-In-UDP",
		L"NETDIS-FDPHOST-Out-UDP-Active",
		L"RVM-VDSLDR-In-TCP-NoScope",
		L"CoreNet-ICMP6-LR2-In",
		L"WMI-WINMGMT-In-TCP-NoScope",
		L"CoreNet-GP-NP-Out-TCP",
		L"CoreNet-IGMP-In",
		L"NETDIS-FDRESPUB-WSD-In-UDP",
		L"MsiScsi-Out-TCP-NoScope",
		L"ProximityUxHost-Sharing-In-TCP-NoScope",
		L"MSDTC-KTMRM-In-TCP-NoScope",
		L"RemoteAssistance-SSDPSrv-In-TCP-Active",
		L"WMI-RPCSS-In-TCP-NoScope",
		L"CoreNet-Diag-ICMP6-EchoRequest-Out-NoScope",
		L"RemoteEventLogSvc-RPCSS-In-TCP",
		L"NETDIS-WSDEVNTS-Out-TCP-Active",
		L"CoreNet-IPv6-Out",
		L"PerfLogsAlerts-DCOM-In-TCP",
		L"NETDIS-UPnPHost-In-TCP-NoScope",
		L"PNRPMNRS-SSDPSrv-In-UDP",
		L"WirelessDisplay-In-TCP",
		L"Collab-PNRP-Out-UDP",
		L"WFDPRINT-DAFWSD-In-Active",
		L"CoreNet-ICMP6-NDS-Out",
		L"NETDIS-WSDEVNT-In-TCP",
		L"NETDIS-FDPHOST-Out-UDP",
		L"WINRM-HTTP-In-TCP-NoScope",
		L"PerfLogsAlerts-PLASrv-In-TCP-NoScope",
		L"RRAS-PPTP-Out-TCP",
		L"PerfLogsAlerts-DCOM-In-TCP-NoScope",
		L"NETDIS-NB_Datagram-Out-UDP-NoScope",
		L"MDNS-Out-UDP-Private-Active",
		L"CoreNet-Diag-ICMP4-EchoRequest-Out",
		L"NETDIS-SSDPSrv-Out-UDP",
		L"RemoteTask-In-TCP",
		L"CDPSvc-WFD-In-TCP",
		L"NETDIS-NB_Datagram-In-UDP-Active",
		L"WFDPRINT-SCAN-Out-Active",
		L"CoreNet-ICMP6-DU-In",
		L"NETDIS-WSDEVNTS-In-TCP",
		L"Microsoft-Windows-Unified-Telemetry-Client",
		L"CoreNet-ICMP6-LD-In",
		L"RemoteAssistance-PnrpSvc-UDP-OUT-Active",
		L"AllJoyn-Router-In-UDP",
		L"AllJoyn-Router-In-TCP",
		L"RemoteFwAdmin-In-TCP",
		L"NETDIS-WSDEVNT-Out-TCP",
		L"CoreNet-IPHTTPS-In",
		L"CoreNet-DHCP-Out",
		L"CoreNet-ICMP6-PTB-In",
		L"RRAS-L2TP-In-UDP",
		L"WFDPRINT-DAFWSD-Out-Active",
		L"TPMVSCMGR-Server-Out-TCP-NoScope",
		L"CoreNet-ICMP6-LR-Out",
		L"vm-monitoring-icmpv6",
		L"NETDIS-LLMNR-In-UDP-Active",
		L"ProximityUxHost-Sharing-Out-TCP-NoScope",
		L"RemoteAssistance-In-TCP-EdgeScope",
		L"AllJoyn-Router-Out-TCP",
		L"MDNS-In-UDP-Private-Active",
		L"RemoteTask-RPCSS-In-TCP",
		L"CDPSvc-In-TCP",
		L"CDPSvc-In-UDP",
		L"vm-monitoring-icmpv4",
		L"NETDIS-SSDPSrv-In-UDP-Active",
		L"Netlogon-NamedPipe-In",
		L"CoreNet-ICMP6-TE-Out",
		L"NETDIS-FDRESPUB-WSD-Out-UDP",
		L"NETDIS-UPnPHost-In-TCP-Teredo",
		L"NETDIS-LLMNR-Out-UDP-Active",
		L"NETDIS-WSDEVNTS-Out-TCP-NoScope",
		L"RemoteAssistance-SSDPSrv-In-UDP-Active",
		L"TPMVSCMGR-Server-Out-TCP",
		L"TPMVSCMGR-Server-In-TCP",
		L"TPMVSCMGR-RPCSS-In-TCP",
		L"RVM-VDSLDR-In-TCP",
		L"RVM-RPCSS-In-TCP-NoScope",
		L"CoreNet-Diag-ICMP6-EchoRequest-In",
		L"NETDIS-NB_Name-In-UDP-NoScope",
		L"Collab-PNRP-SSDPSrv-Out-UDP",
		L"CoreNet-ICMP6-LQ-Out",
		L"Microsoft-Windows-DeviceManagement-deviceenroller-TCP-Out",
		L"RemoteSvcAdmin-NP-In-TCP",
		L"RemoteFwAdmin-RPCSS-In-TCP-NoScope",
		L"AllJoyn-Router-Out-UDP",
		L"CoreNet-ICMP6-NDA-Out",
		L"NETDIS-UPnPHost-In-TCP",
		L"MSDTC-RPCSS-In-TCP",
		L"PNRPMNRS-PNRP-Out-UDP",
		L"NVS-FrameServer-Out-TCP-NoScope",
		L"RemoteSvcAdmin-In-TCP-NoScope",
		L"CoreNet-Teredo-In",
		L"RemoteSvcAdmin-NP-In-TCP-NoScope",
		L"NETDIS-NB_Datagram-In-UDP",
		L"NETDIS-NB_Datagram-Out-UDP-Active",
		L"CoreNet-DNS-Out-UDP",
		L"CoreNet-ICMP6-PP-In",
		L"NETDIS-NB_Name-In-UDP-Active",
		L"MsiScsi-In-TCP-NoScope",
		L"Collab-P2PHost-WSD-Out-UDP",
		L"NETDIS-DAS-In-UDP",
		L"WINRM-HTTP-Compat-In-TCP",
		L"CoreNet-IGMP-Out",
		L"RemoteSvcAdmin-RPCSS-In-TCP",
		L"RemoteSvcAdmin-In-TCP",
		L"NETDIS-SSDPSrv-In-UDP",
		L"NVS-FrameServer-In-TCP-NoScope",
		L"CoreNet-IPHTTPS-Out",
		L"CoreNet-ICMP6-NDA-In",
		L"NETDIS-WSDEVNTS-In-TCP-NoScope",
		L"Collab-P2PHost-Out-TCP",
		L"CoreNet-ICMP6-RA-Out",
		L"RemoteAssistance-RAServer-Out-TCP-NoScope-Active",
		L"WirelessDisplay-Out-UDP",
		L"WirelessDisplay-Out-TCP",
		L"RemoteFwAdmin-In-TCP-NoScope",
		L"vm-monitoring-nb-session",
		L"NETDIS-LLMNR-Out-UDP",
		L"NETDIS-FDRESPUB-WSD-Out-UDP-Active",
		L"NETDIS-WSDEVNT-Out-TCP-NoScope",
		L"WMI-ASYNC-In-TCP-NoScope",
		L"RemoteSvcAdmin-RPCSS-In-TCP-NoScope",
		L"NETDIS-WSDEVNT-In-TCP-NoScope",
		L"DIAL-Protocol-Server-In-TCP-NoScope",
		L"Collab-PNRP-In-UDP",
		L"NETDIS-UPnPHost-Out-TCP-NoScope",
		L"RemoteAssistance-PnrpSvc-UDP-In-EdgeScope-Active",
		L"RemoteTask-RPCSS-In-TCP-NoScope",
		L"Microsoft-Windows-Troubleshooting-HTTP-HTTPS-Out",
		L"CoreNet-GP-LSASS-Out-TCP",
		L"NETDIS-WSDEVNTS-Out-TCP",
		L"NETDIS-FDRESPUB-WSD-In-UDP-Active",
		L"PNRPMNRS-SSDPSrv-Out-UDP",
		L"WirelessDisplay-Infra-In-TCP",
		L"WMI-RPCSS-In-TCP",
		L"CoreNet-ICMP6-RS-In",
		L"vm-monitoring-dcom",
		L"NETDIS-NB_Datagram-In-UDP-NoScope",
		L"WMI-WINMGMT-Out-TCP",
		L"TPMVSCMGR-Server-In-TCP-NoScope",
		L"RVM-VDS-In-TCP",
		L"CoreNet-Teredo-Out",
		L"RRAS-PPTP-In-TCP",
		L"RemoteAssistance-PnrpSvc-UDP-In-EdgeScope",
		L"MsiScsi-In-TCP",
		L"Microsoft-Windows-DeviceManagement-CertificateInstall-TCP-Out",
		L"CoreNet-ICMP6-LD-Out",
		L"vm-monitoring-rpc",
		L"NETDIS-NB_Name-In-UDP",
		L"MSDTC-Out-TCP-NoScope",
		L"DeliveryOptimization-TCP-In",
		L"WINRM-HTTP-In-TCP",
		L"PerfLogsAlerts-PLASrv-In-TCP",
		L"RemoteFwAdmin-RPCSS-In-TCP",
		L"WMI-WINMGMT-In-TCP",
		L"MDNS-Out-UDP-Public-Active",
		L"CoreNet-Diag-ICMP4-EchoRequest-Out-NoScope",
		L"WMI-ASYNC-In-TCP",
		L"SNMPTRAP-In-UDP-NoScope",
		L"Microsoft-Windows-Enrollment-WinRT-TCP-Out",
		L"NVS-FrameServer-In-UDP-NoScope",
		L"EventForwarder-In-TCP",
		L"CoreNet-Diag-ICMP6-EchoRequest-Out",
		L"CoreNet-ICMP6-LR2-Out",
		L"RemoteEventLogSvc-RPCSS-In-TCP-NoScope",
		L"NETDIS-UPnPHost-In-TCP-Active",
		L"RemoteAssistance-RAServer-In-TCP-NoScope-Active",
		L"RVM-RPCSS-In-TCP",
		L"NETDIS-WSDEVNTS-In-TCP-Active",
		L"NETDIS-UPnPHost-Out-TCP-Active",
		L"RemoteAssistance-DCOM-In-TCP-NoScope-Active",
		L"EventForwarder-RPCSS-In-TCP",
		L"CoreNet-ICMP6-NDS-In",
		L"NETDIS-WSDEVNT-In-TCP-Active",
		L"RemoteAssistance-SSDPSrv-Out-TCP-Active",
		L"RemoteAssistance-SSDPSrv-Out-UDP-Active",
		L"CoreNet-ICMP6-RS-Out",
		L"NETDIS-FDPHOST-In-UDP-Active",
		L"RemoteAssistance-PnrpSvc-UDP-OUT",
		L"WFDPRINT-SCAN-In-Active",
		L"MSDTC-RPCSS-In-TCP-NoScope",
		L"CDPSvc-Out-TCP",
		L"NETDIS-UPnP-Out-TCP",
		L"MsiScsi-Out-TCP",
		L"NETDIS-SSDPSrv-In-UDP-Teredo",
		L"MSDTC-KTMRM-In-TCP",
		L"CoreNet-DHCPV6-In",
		L"TPMVSCMGR-RPCSS-In-TCP-NoScope",
		L"RRAS-GRE-In",
		L"NETDIS-DAS-In-UDP-Active",
		L"CoreNet-DHCP-In",
		L"CoreNet-ICMP4-DUFRAG-In",
		L"CoreNet-ICMP6-RA-In",
		L"CoreNet-ICMP6-PP-Out",
		L"DeliveryOptimization-UDP-In",
		L"MDNS-Out-UDP-Domain-Active",
		L"SNMPTRAP-In-UDP",
		L"RemoteEventLogSvc-NP-In-TCP",
		L"NETDIS-LLMNR-In-UDP",
		L"CDPSvc-WFD-Out-TCP",
		L"CoreNet-IPv6-In",
		L"RemoteEventLogSvc-In-TCP",
		L"Collab-P2PHost-WSD-In-UDP",
		L"NETDIS-NB_Datagram-Out-UDP",
		L"NETDIS-NB_Name-Out-UDP",
		L"PNRPMNRS-PNRP-In-UDP",
		L"CDPSvc-Out-UDP",
		L"CoreNet-ICMP6-PTB-Out",
		L"RRAS-L2TP-Out-UDP",
		L"MSDTC-Out-TCP",
		L"RemoteAssistance-Out-TCP-Active",
		L"WFDPRINT-SPOOL-In-Active",
		L"RemoteTask-In-TCP-NoScope",
		L"RemoteEventLogSvc-NP-In-TCP-NoScope",
		L"NETDIS-WSDEVNT-Out-TCP-Active",
		L"NETDIS-SSDPSrv-Out-UDP-Active",
		L"RemoteAssistance-Out-TCP",
		L"WFDPRINT-SPOOL-Out-Active",
		L"NETDIS-UPnPHost-Out-TCP",
		NULL
	};

	const wchar_t* ValidPaths[] = {
		L"\\SystemRoot\\System32\\ntoskrnl.exe",
		L"%SystemRoot%\\system32\\msdtc.exe",
		L"%SystemRoot%\\system32\\svchost.exe",
		L"%SystemRoot%\\system32\\sppextcomobj.exe",
		L"%SystemRoot%\\system32\\vds.exe",
		L"%SystemRoot%\\system32\\wudfhost.exe",
		L"%SystemRoot%\\system32\\snmptrap.exe",
		L"%systemroot%\\system32\\wbem\\unsecapp.exe",
		L"%SystemRoot%\\system32\\dashost.exe",
		L"%SystemRoot%\\system32\\vdsldr.exe",
		L"%SystemRoot%\\system32\\spoolsv.exe",
		L"%SystemRoot%\\System32\\lsass.exe",
		L"%SystemRoot%\\system32\\NetEvtFwdr.exe",
		L"%SystemRoot%\\system32\\mdeserver.exe",
		L"%SystemRoot%\\system32\\services.exe",
		L"%SystemRoot%\\system32\\spoolsvworker.exe",
		L"%SystemRoot%\\system32\\RmtTpmVscMgrSvr.exe",
		L"%SystemRoot%\\system32\\msra.exe",
		L"%SystemRoot%\\system32\\RdpSa.exe",
		L"%systemroot%\\system32\\plasrv.exe",
		L"%systemroot%\\system32\\CastSrv.exe",
		L"%systemroot%\\system32\\wininit.exe",
		L"%SystemRoot%\\system32\\proximityuxhost.exe",
		L"%SystemRoot%\\system32\\raserver.exe",
		L"%SystemRoot%\\system32\\omadmclient.exe",
		L"%SystemRoot%\\system32\\deviceenroller.exe",
		L"%SystemRoot%\\system32\\dmcertinst.exe",
		L"%ProgramFiles%\\Windows Media Player\\wmplayer.exe",
		L"%ProgramFiles(x86)%\\Windows Media Player\\wmplayer.exe",
		L"%PROGRAMFILES%\\Windows Media Player\\wmpnetwk.exe",
		NULL
	};

	std::wstring Guid = pFwRule->GetGuidStr();

	if(Guid.empty() || Guid[0] == L'{')
		return false;

	for (const wchar_t** pDefaultRule = DefaultRules; *pDefaultRule; pDefaultRule++)
	{
		if (_wcsnicmp(Guid.c_str(), *pDefaultRule, wcslen(*pDefaultRule)) == 0) 
		{
			//DbgPrint(L"CFirewall::IsDefaultWindowsRule: %s %s is a default Windows rule\n", Guid.c_str(), pFwRule->GetBinaryPath().c_str());
			for (const wchar_t** pValidPath = ValidPaths; *pValidPath; pValidPath++)
			{
				if (_wcsicmp(pFwRule->GetBinaryPath().c_str(), *pValidPath) == 0)
					return true;
			}
			return false;
		}
	}
	return false;
}

bool ends_with(const std::wstring& str, const std::wstring& suffix) {
	if (str.size() < suffix.size()) return false;
	return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

bool CFirewall::IsWindowsStoreRule(const CFirewallRulePtr& pFwRule) const
{
	std::vector<std::wstring> suffixes = {
		L"-In-Allow-ServerCapability",
		L"-Out-Allow-ServerCapability",
		L"-In-Allow-ClientCapability",
		L"-Out-Allow-ClientCapability",
		L"-In-Allow-AllCapabilities",
		L"-Out-Allow-AllCapabilities"
	};

	std::wstring Guid = pFwRule->GetGuidStr();

	if(Guid.empty() || Guid[0] == L'{')
		return false;

	for (auto& suffix : suffixes) {
		if (ends_with(Guid, suffix))
			return true;
	}

	return false;
}

#include <ShellApi.h>

STATUS CFirewall::RestoreDefaultFwRules()
{
	ShellExecuteW(nullptr, NULL, L"netsh", L"advfirewall reset", nullptr, SW_HIDE);
	return OK;
}