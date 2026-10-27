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
#include <algorithm>


#define API_FIREWALL_FILE_NAME L"Firewall.dat"
#define API_FIREWALL_FILE_VERSION 1

EFwProfiles CFirewall__Profiles[] = { EFwProfiles::Private, EFwProfiles::Public, EFwProfiles::Domain };

CFirewall::CFirewall()
	: m_FwRuleTemplates(&g_DefaultMemPool), m_FwTemplatesTree(&g_DefaultMemPool), m_AutoGuardTree(&g_DefaultMemPool)
{
	//
	// Both trees match whole path elements, a single '*' stays inside one element and '**' is
	// what crosses them. A rule template or a guard entry decides whether a binary is trusted,
	// so a wildcard which silently reached into sub directories would give away more than the
	// user ever wrote down.
	//
	m_FwTemplatesTree.SetSimplePattern(false);
	m_AutoGuardTree.SetSimplePattern(false);

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

STATUS CFirewall::ReLoad()
{
	std::unique_lock Lock(m_RulesMutex);

	// Clear pending reversion queues to prevent stale operations
	m_RulesToRemove.clear();
	m_RulesToRevert.clear();
	// and what was waiting for its twin, the rules it points at are about to be replaced by those from disk
	m_WithdrawnRules.clear();
	m_HeldRules.clear();
	m_SettledRemovals.clear();

	//
	// Phase 1: Load rules from disk into temporary collection
	//

	std::map<CFlexGuid, CFirewallRulePtr> DiskRules;
	CBuffer Buffer;
	if (ReadFile(theCore->GetDataFolder() + L"\\" API_FIREWALL_FILE_NAME, Buffer))
	{
		StVariant Data;
		if (Data.FromPacket(&Buffer, true) == StVariant::eErrNone &&
			Data[API_S_VERSION].To<uint32>() == API_FIREWALL_FILE_VERSION)
		{
			StVariant Entries = Data[API_S_FW_RULES];
			for (uint32 i = 0; i < Entries.Count(); i++)
			{
				CFirewallRulePtr pFwRule = std::make_shared<CFirewallRule>();
				if (pFwRule->FromVariant(Entries[i]))
					DiskRules[pFwRule->GetGuidStr()] = pFwRule;
			}
		}
	}

	//
	// Phase 2: Load current Windows Firewall state
	//

	auto ret = CWindowsFirewall::Instance()->LoadRules();
	if (ret.IsError())
		return ret;

	auto pWinRuleList = ret.GetValue();
	std::map<std::wstring, SWindowsFwRulePtr> WinRules;
	for (auto& pRule : *pWinRuleList)
		WinRules[pRule->Guid] = pRule;

	//
	// Phase 3: Capture old state and clear all collections
	//

	std::map<CFlexGuid, CFirewallRulePtr> OldFwRules = std::move(m_FwRules);

	// Build map of backup rules by their OriginalGuid (to detect external deletions)
	std::map<std::wstring, CFirewallRulePtr> OldBackupsByOriginalGuid;
	for (auto& [Guid, pOldRule] : OldFwRules)
	{
		if (pOldRule->IsBackup() || pOldRule->IsDiverged())
		{
			std::wstring origGuid = pOldRule->GetOriginalGuid();
			if (!origGuid.empty())
				OldBackupsByOriginalGuid[origGuid] = pOldRule;
		}
	}

	m_FwRules.clear();
	m_FileRules.clear();
	m_SvcRules.clear();
	m_AppRules.clear();
	m_FwTemplatesTree.Clear();
	m_FwRuleTemplates.Clear();

	//
	// Phase 4: Process disk rules
	//

	std::set<std::wstring> ProcessedWinGuids;
	std::set<std::wstring> ProcessedBackupOriginalGuids;

	for (auto& [DiskGuid, pDiskRule] : DiskRules)
	{
		std::wstring GuidStr = DiskGuid.ToWString();

		// Templates: just reload, no Windows FW interaction
		if (pDiskRule->IsTemplate())
		{
			AddRuleUnsafe(pDiskRule);
			continue;
		}

		// Backups: keep as-is, don't touch Windows FW
		// (they preserve external changes for manual restoration)
		if (pDiskRule->IsBackup() || pDiskRule->IsDiverged())
		{
			AddRuleUnsafe(pDiskRule);
			continue;
		}

		auto winIt = WinRules.find(GuidStr);

		if (pDiskRule->IsApproved())
		{
			if (winIt == WinRules.end())
			{
				// Rule on disk but NOT in Windows FW
				// Check if there's already a backup for this (external deletion)
				auto backupIt = OldBackupsByOriginalGuid.find(GuidStr);
				if (backupIt != OldBackupsByOriginalGuid.end())
				{
					// External deletion - keep as backup, don't recreate in Windows FW
					// Update backup data from disk (discarding any unsaved modifications)
					CFirewallRulePtr pBackup = backupIt->second;
					ProcessedBackupOriginalGuids.insert(GuidStr);

					// Create updated backup with disk data but preserving backup's GUID and state
					SWindowsFwRulePtr pBackupData = std::make_shared<SWindowsFwRule>(*pDiskRule->GetFwRule());
					pBackupData->Guid = pBackup->GetGuidStr(); // Keep backup's GUID

					CFirewallRulePtr pUpdatedBackup = std::make_shared<CFirewallRule>(pBackupData);
					//pUpdatedBackup->m_State = EFwRuleState::eBackup;
					pUpdatedBackup->m_OriginalGuid = pBackup->GetOriginalGuid();
					pUpdatedBackup->m_Source = pDiskRule->GetSource();

					AddRuleUnsafe(pUpdatedBackup);
				}
				else
				{
					// Our tool deleted it - recreate in Windows FW
					SWindowsFwRulePtr pData = pDiskRule->GetFwRule();
					STATUS Status = CWindowsFirewall::Instance()->UpdateRule(pData);
					if (Status.IsError())
						theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, 1,
							L"ReLoad: Failed to recreate rule: %s", pDiskRule->GetName().c_str());
					AddRuleUnsafe(pDiskRule);
				}
			}
			else
			{
				ProcessedWinGuids.insert(GuidStr);
				SWindowsFwRulePtr pWinRule = winIt->second;

				if (!pDiskRule->Match(pWinRule))
				{
					// Rule differs - check if we should revert
					auto oldIt = OldFwRules.find(DiskGuid);
					EFwRuleSource oldSource = (oldIt != OldFwRules.end())
						? oldIt->second->GetSource() : EFwRuleSource::eUnknown;

					// Only revert if our tool modified it
					if (oldSource == EFwRuleSource::eMajorPrivacy ||
						oldSource == EFwRuleSource::eAutoTemplate)
					{
						// Our modification - revert Windows FW to disk version
						SWindowsFwRulePtr pData = pDiskRule->GetFwRule();
						STATUS Status = CWindowsFirewall::Instance()->UpdateRule(pData);
						if (Status.IsError())
							theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, 1,
								L"ReLoad: Failed to revert rule: %s", pDiskRule->GetName().c_str());
						AddRuleUnsafe(pDiskRule);
					}
					else
					{
						// External modification since last save - create backup
						// The approved disk rule becomes a backup, Windows FW rule becomes current
						pDiskRule->SetAsBackup(false, !MatchProgramID(pWinRule, pDiskRule));
						AddRuleUnsafe(pDiskRule); // Add backup with new GUID

						// Add Windows FW rule as new unapproved
						CFirewallRulePtr pNewRule = std::make_shared<CFirewallRule>(pWinRule);
						AddRuleUnsafe(pNewRule);
					}
				}
				else
				{
					// Matches - just add
					AddRuleUnsafe(pDiskRule);
				}
			}
		}
		else // Unapproved on disk
		{
			if (winIt != WinRules.end())
			{
				ProcessedWinGuids.insert(GuidStr);
				pDiskRule->Update(winIt->second); // Update from current Windows state
			}
			AddRuleUnsafe(pDiskRule);
		}
	}

	//
	// Phase 5: Handle Windows FW rules not on disk
	//

	for (auto& [WinGuid, pWinRule] : WinRules)
	{
		if (ProcessedWinGuids.count(WinGuid))
			continue;

		CFlexGuid guid(WinGuid);
		auto oldIt = OldFwRules.find(guid);

		if (oldIt != OldFwRules.end())
		{
			CFirewallRulePtr pOldRule = oldIt->second;
			EFwRuleSource oldSource = pOldRule->GetSource();

			if (oldSource == EFwRuleSource::eMajorPrivacy ||
				oldSource == EFwRuleSource::eAutoTemplate)
			{
				// Rule created by our tool but not saved - remove from Windows FW
				STATUS Status = CWindowsFirewall::Instance()->RemoveRule(WinGuid);
				if (Status.IsError())
					theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, 1,
						L"ReLoad: Failed to remove unsaved rule: %s", pOldRule->GetName().c_str());
				continue; // Don't add to our collections
			}

			// Unapproved external rule - keep as is
			CFirewallRulePtr pFwRule = std::make_shared<CFirewallRule>(pWinRule);
			pFwRule->Update(pOldRule); // Preserve state info
			AddRuleUnsafe(pFwRule);
		}
		else
		{
			// Completely new rule we don't know about - add as unapproved
			CFirewallRulePtr pFwRule = std::make_shared<CFirewallRule>(pWinRule);
			if (IsDefaultWindowsRule(pFwRule))
				pFwRule->SetSource(EFwRuleSource::eWindowsDefault);
			else if (IsWindowsStoreRule(pFwRule))
				pFwRule->SetSource(EFwRuleSource::eWindowsStore);
			AddRuleUnsafe(pFwRule);
		}
	}

	//
	// Phase 6: Preserve backups whose original rule wasn't on disk
	// (these are unsaved backups for external deletions where disk doesn't have the original rule)
	//

	for (auto& [OrigGuid, pBackup] : OldBackupsByOriginalGuid)
	{
		if (ProcessedBackupOriginalGuids.count(OrigGuid))
			continue; // Already processed from disk approved rule

		// Check if this backup already exists (e.g., loaded from disk as a backup)
		if (m_FwRules.find(pBackup->GetGuidStr()) != m_FwRules.end())
			continue; // Already loaded from disk

		// This backup's original rule wasn't on disk - preserve the backup as-is
		AddRuleUnsafe(pBackup);
	}

	//
	// Phase 7: Cleanup old program references
	//

	for (auto& [Guid, pOldRule] : OldFwRules)
	{
		if (m_FwRules.find(Guid) == m_FwRules.end())
			theCore->ProgramManager()->RemoveFwRule(pOldRule);
	}

	// Update flags
	m_UpdateAllRules = false;
	m_UpdateDefaultProfiles = 1;

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
	WriteFileSafe(theCore->GetDataFolder() + L"\\" API_FIREWALL_FILE_NAME, Buffer);

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

	AskAboutWithdrawnRules(); // first, so a removal which ran out of time is reverted in this same pass
	ReleaseHeldRules(); // and an addition turned down

	RevertChanges();
}

//
// How long a rule which went away is kept quiet, waiting to see whether the same one comes back.
// A program which replaces a rule by an identical one does so in one go, so this only has to
// outlast the gap between the two halves, not any real decision the user might be making.
//
#define FW_RECREATE_GRACE_MS 5000

//
// A rule has just turned up. If it is the one a removal is being held back for, down to the last
// field, then saying yes to the removal and yes to this leaves the firewall exactly as it was.
// There is nothing in that worth asking about, so the pair is settled and the backup handed back
// so the caller can let go of it.
//
CFirewallRulePtr CFirewall::TakeWithdrawnTwinUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule)
{
	for (auto I = m_WithdrawnRules.begin(); I != m_WithdrawnRules.end(); ++I)
	{
		std::wstring Diff = CFirewallRule::MatchDiff(I->pBackup->GetFwRule(), pRule);
		if (Diff.empty())
		{
			CFirewallRulePtr pBackup = I->pBackup;
			m_WithdrawnRules.erase(I);
			return pBackup;
		}

		//
		// The same rule by name, but not the same rule. That is the difference between a program
		// putting back what it took and one changing something while it was at it, and the user
		// is about to be asked about both halves, so it is worth saying which field made it so.
		//
		if (I->pBackup->GetName() == pRule->Name)
			theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, 1,
				L"Firewall Rule '%s' came back differing in %s, so it is not the one that went away",
				pRule->Name.c_str(), Diff.c_str());
	}
	return CFirewallRulePtr();
}

//
// The other way round: a rule is going away and an identical one already stands in its place.
// Whether that twin has been agreed to yet makes no difference, because both halves are settled
// together below; what must not happen is settling the removal and leaving the twin undecided,
// as turning that down afterwards would leave neither.
//
CFirewallRulePtr CFirewall::FindLiveTwinUnsafe(const CFirewallRulePtr& pGoing)
{
	for (auto I : m_FwRules)
	{
		const CFirewallRulePtr& pOther = I.second;
		if (pOther == pGoing || pOther->IsBackup() || pOther->IsTemplate())
			continue;
		if (pOther->Match(pGoing->GetFwRule()))
			return pOther;
	}
	return CFirewallRulePtr();
}

//
// A rule has just turned up identical to one which still stands. That is the first half of a
// replacement done the other way round, the copy going in before the original goes out. The
// copy grants nothing the original does not already grant, so it can wait for the removal as
// safely as a removal can wait for its replacement.
//
CFirewallRulePtr CFirewall::FindLiveTwinUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pComing)
{
	for (auto I : m_FwRules)
	{
		const CFirewallRulePtr& pOther = I.second;
		if (pOther->IsBackup() || pOther->IsTemplate())
			continue;
		if (pOther->Match(pComing))
			return pOther;
	}
	return CFirewallRulePtr();
}

//
// A rule has just turned up identical to one whose removal was already let through, by a guard
// entry or the removal option. Nothing was held back then, as there was nothing to ask, but the
// two are still one replacement. Without this the copy was a rule of its own: asked about, or
// under the blanket guard turned down, which left the program with neither.
//
CFirewallRulePtr CFirewall::TakeSettledTwinUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule)
{
	uint64 uNow = GetTickCount64();
	for (auto I = m_SettledRemovals.begin(); I != m_SettledRemovals.end(); ++I)
	{
		if (I->uRemoved + FW_RECREATE_GRACE_MS <= uNow)
			continue; // too late to be the same replacement, ReleaseHeldRules clears it out
		if (CFirewallRule::MatchDiff(I->pRule->GetFwRule(), pRule).empty())
		{
			CFirewallRulePtr pGone = I->pRule;
			m_SettledRemovals.erase(I);
			return pGone;
		}
	}
	return CFirewallRulePtr();
}

bool CFirewall::TakeHeldRuleUnsafe(const CFirewallRulePtr& pRule)
{
	for (auto I = m_HeldRules.begin(); I != m_HeldRules.end(); ++I) {
		if (I->pRule == pRule) {
			m_HeldRules.erase(I);
			return true;
		}
	}
	return false;
}

//
// The original stayed, so the copy was not a replacement after all but a rule of its own, and it
// is dealt with as it would have been on arrival: asked about, or turned down by the blanket guard.
//
void CFirewall::ReleaseHeldRules()
{
	std::vector<SHeldRule> Expired;

	{
		std::unique_lock Lock(m_RulesMutex);

		uint64 uNow = GetTickCount64();
		for (auto I = m_HeldRules.begin(); I != m_HeldRules.end(); )
		{
			if (I->uHeld + FW_RECREATE_GRACE_MS > uNow) {
				++I;
				continue;
			}
			if (I->bReject)
				m_RulesToRemove.insert(I->pRule); // what the blanket guard would have done straight away
			else
				Expired.push_back(*I);
			I = m_HeldRules.erase(I);
		}

		// nothing came back for these, there is nothing to do about them either
		m_SettledRemovals.erase(std::remove_if(m_SettledRemovals.begin(), m_SettledRemovals.end(),
			[&](const SSettledRemoval& Entry) { return Entry.uRemoved + FW_RECREATE_GRACE_MS <= uNow; }), m_SettledRemovals.end());
	}

	for (const SHeldRule& Entry : Expired)
		EmitChangeEvent(Entry.pRule->GetGuidStr(), Entry.pRule->GetName(), EConfigEvent::eAdded, false, true);
}

//
// Nothing came back in time, so the removals held back until now are real after all and the user
// is asked about them, exactly as they would have been without the wait. Under the blanket guard
// they are reverted instead, which is likewise what would have happened without the wait.
//
void CFirewall::AskAboutWithdrawnRules()
{
	std::vector<SWithdrawnRule> Expired;

	{
		std::unique_lock Lock(m_RulesMutex);

		uint64 uNow = GetTickCount64();
		for (auto I = m_WithdrawnRules.begin(); I != m_WithdrawnRules.end(); )
		{
			if (I->uWithdrawn + FW_RECREATE_GRACE_MS > uNow) {
				++I;
				continue;
			}
			if (I->bRevert)
				m_RulesToRevert.insert(I->pBackup); // what the blanket guard would have done straight away
			else
				Expired.push_back(*I);
			I = m_WithdrawnRules.erase(I);
		}
	}

	// the original guid, that is what the rule was known by and what the UI finds the backup with
	for (const SWithdrawnRule& Entry : Expired)
		EmitChangeEvent(Entry.pBackup->GetOriginalGuid(), Entry.pBackup->GetName(), EConfigEvent::eRemoved, false, true);
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

	//
	// Gathered first: a template takes the rules made from it along, so deleting while walking the
	// list would pull entries out from under the walk. Those rules are left out here for the same
	// reason, their template sees to them.
	//
	std::vector<CFirewallRulePtr> Expired;
	std::set<std::wstring> ExpiredTemplates;
	for (const auto& [Guid, pRule] : m_FwRules) {
		if (All ? pRule->IsTemporary() : pRule->IsExpired()) {
			Expired.push_back(pRule);
			if (pRule->IsTemplate())
				ExpiredTemplates.insert(pRule->GetGuidStr());
		}
	}

	for (const CFirewallRulePtr& pRule : Expired) {
		std::wstring TemplateGuid = pRule->GetTemplateGuid();
		if (!pRule->IsTemplate() && !TemplateGuid.empty() && ExpiredTemplates.count(TemplateGuid))
			continue;
		theCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, 1, StrLine(L"Removed expired Firewall Rule: %s", pRule->GetFwRule()->Name.c_str()).c_str());
		DelRule(pRule->GetGuidStr(), true);
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
		theCore->EmitEvent(Status.IsError() ? ELogLevels::eError : ELogLevels::eInfo, eLogFwRuleRejected, Data); // the guard doing its job is no error, it failing at it is
	}
	m_RulesToRemove.clear();

	for (auto& pFwRuleBackup : m_RulesToRevert)
	{
		if(pFwRuleBackup->GetSetErrorCount() > 0)
			continue;

		STATUS Status = RestoreRule(pFwRuleBackup, false);

		StVariant Data;
		Data[API_V_GUID] = pFwRuleBackup->GetOriginalGuid();
		Data[API_V_NAME] = pFwRuleBackup->GetName(); 
		Data[API_V_ID] = pFwRuleBackup->GetProgramID().ToVariant(SVarWriteOpt());
		Data[API_V_STATUS] = Status.GetStatus();
		theCore->EmitEvent(Status.IsError() ? ELogLevels::eError : ELogLevels::eInfo, eLogFwRuleRestored, Data);
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

//
// bApprove is for the user putting a rule back, which says yes to what comes back. The guard
// putting it back only undoes a change, and leaves the rule as approved, or not, as it was.
//
STATUS CFirewall::RestoreRule(const CFirewallRulePtr& pFwRuleBackup, bool bApprove)
{
	SWindowsFwRulePtr pData = std::make_shared<SWindowsFwRule>(*pFwRuleBackup->GetFwRule());
	auto pFwRule = std::make_shared<CFirewallRule>(pData);
	pFwRule->Update(pFwRuleBackup);

	if (bApprove)
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

			//RemoveRuleUnsafe(pFwRuleBackup);
			ClearBackups(pFwRule->GetGuidStr());

			theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, 1, L"Reverted Firewall Rule: %s", pFwRule->GetFwRule()->Name.c_str());
		}
		else // set rule is not what we expect
		{
			pFwRuleBackup->IncrSetErrorCount();

			EmitChangeEvent(pFwRuleBackup->GetGuidStr(), pFwRuleBackup->GetFwRule()->Name, EConfigEvent::eModified, true, true); // reset backup rule in ui

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

void CFirewall::ClearBackups(const std::wstring& Guid)
{
	for (auto I = m_FwRules.begin(); I != m_FwRules.end(); ) {
		auto Cur = (I++)->second;
		if (Cur->GetOriginalGuid() == Guid)
			RemoveRuleUnsafe(Cur);
	}
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

STATUS CFirewall::SetRule(const CFirewallRulePtr& pFwRule, const std::vector<std::wstring>& TakeOver)
{
	if (pFwRule->IsTemplate())
	{
		SetTempalte(pFwRule, TakeOver);
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
			ClearBackups(pCurRule->GetGuidStr());
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

STATUS CFirewall::DelRule(const CFlexGuid& Guid, bool bWithDerived)
{
	if (TryRemoveTemplate(Guid, bWithDerived))
		return OK;

	std::unique_lock Lock(m_RulesMutex);

	//
	// A backup is only a record of what a rule looked like before something changed it, it was
	// never written to the windows firewall and there is nothing there to take away. Asking the
	// firewall to remove it fails, which used to leave the backup in the list for good with no
	// way of getting rid of it.
	//
	auto F = m_FwRules.find(Guid);
	bool bBackup = (F != m_FwRules.end() && F->second->IsBackup());

	if (!bBackup) {
		STATUS Status = CWindowsFirewall::Instance()->RemoveRule(Guid.ToWString());
		if (!Status)
			return Status;
	}

	CFirewallRulePtr pFwRule = UpdateFWRuleUnsafe(NULL, Guid);

	if(pFwRule)
		EmitChangeEvent(pFwRule->GetGuidStr(), pFwRule->GetName(), EConfigEvent::eRemoved, true);

	return OK;
}

//
// The rule a template makes for one binary: everything the template says, for this one path and,
// where the template points both ways, for the one direction at hand.
//
SWindowsFwRulePtr CFirewall::MakeDerivedRuleData(const SWindowsFwRulePtr& pTemplate, const std::wstring& BinaryPath, EFwDirections Direction)
{
	SWindowsFwRulePtr pData = std::make_shared<struct SWindowsFwRule>(*pTemplate); // copy the ruledata

	pData->Guid.clear(); // clear the guid
	pData->BinaryPath = BinaryPath; // set exact pact
	pData->Name = StrReplaceAll(pData->Name, L"MajorPrivacy-Template", L"MajorPrivacy-Rule");
	if (pData->Direction == EFwDirections::Bidirectional)
		pData->Direction = Direction;
	return pData;
}

std::vector<CFirewallRulePtr> CFirewall::GetDerivedRulesUnsafe(const std::wstring& TemplateGuid)
{
	std::vector<CFirewallRulePtr> Derived;
	for (const auto& [Guid, pRule] : m_FwRules) {
		if (!pRule->IsTemplate() && !pRule->IsBackup() && pRule->GetTemplateGuid() == TemplateGuid)
			Derived.push_back(pRule);
	}
	return Derived;
}

bool CFirewall::RemoveDerivedRuleUnsafe(const CFirewallRulePtr& pRule)
{
	std::wstring Guid = pRule->GetGuidStr();
	STATUS Status = CWindowsFirewall::Instance()->RemoveRule(Guid);
	if (Status.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to remove the template rule '%s': %s", pRule->GetName().c_str(), Status.GetMessageText());
		return false;
	}
	UpdateFWRuleUnsafe(NULL, Guid);
	return true;
}

//
// A template stands for a wildcard rule the windows firewall does not have, the rules made from it
// are how it takes effect. So what the template says, they say: its action, its scope, its ports
// and addresses, whether it is on, and how long it lasts, all but the one path each was made for.
// A rule the template no longer reaches, by direction or by pattern, has nothing behind it any
// more and goes, the same way a windows rule would stop applying.
//
void CFirewall::SyncDerivedRulesUnsafe(const CFirewallRulePtr& pTemplateRule, std::vector<SRuleChange>& Changes)
{
	SWindowsFwRulePtr pTemplate = pTemplateRule->GetFwRule();

	uint64 uTimeOut;
	bool bTemporary;
	{
		std::shared_lock Lock(pTemplateRule->m_Mutex);
		uTimeOut = pTemplateRule->m_uTimeOut;
		bTemporary = pTemplateRule->m_bTemporary;
	}

	for (const CFirewallRulePtr& pDerived : GetDerivedRulesUnsafe(pTemplateRule->GetGuidStr()))
	{
		SWindowsFwRulePtr pCur = pDerived->GetFwRule();

		bool bCovered = pTemplate->Direction == EFwDirections::Bidirectional || pTemplate->Direction == pCur->Direction;
		if (bCovered && pTemplateRule->IsEnabled()) // a switched off template is not in the tree, and its pattern did not change for being switched off
		{
			//
			// Matched by the program's path, not the rule's: a rule an installer wrote often spells
			// its binary "%ProgramFiles%\..." where the tree, like the rules a template makes, has
			// the path as the program knows it.
			//
			std::wstring Path = pDerived->GetProgramID().GetFilePath();
			bCovered = false;
			for (auto Found : m_FwTemplatesTree.GetEntries(FW::StringW(&g_DefaultMemPool, Path.c_str()))) {
				auto pEntry = Found.pEntry.Cast<CFwRuleTemplate>();
				if (pEntry && pEntry->GetRule() == pTemplateRule) {
					bCovered = true;
					break;
				}
			}
		}

		if (!bCovered)
		{
			//
			// One the template made goes, there is nothing behind it any more. One it took over was
			// there before it and stays, only no longer under it: the template letting go of a rule
			// must not delete what the user or a program had put there.
			//
			if (pDerived->GetSource() != EFwRuleSource::eAutoTemplate) {
				pDerived->SetTemplateGuid(L"");
				Changes.push_back({ pCur->Guid, pCur->Name, EConfigEvent::eModified });
			}
			else if (RemoveDerivedRuleUnsafe(pDerived))
				Changes.push_back({ pCur->Guid, pCur->Name, EConfigEvent::eRemoved });
			continue;
		}

		SWindowsFwRulePtr pData = MakeDerivedRuleData(pTemplate, pCur->BinaryPath, pCur->Direction);
		pData->Guid = pCur->Guid;
		// what a rule is called is its own, one taken over keeps the name it came with and a made one the name it was made with
		pData->Name = pCur->Name;
		pData->Description = pCur->Description;

		CFirewallRulePtr pRule = pDerived;
		if (!CFirewallRule::Match(pCur, pData))
		{
			STATUS Status = UpdateRuleAndTest(pData);
			if (Status.GetStatus() == STATUS_ERR_RULE_DIVERGENT)
			{
				// written, but not quite as asked; what we keep has to be what windows holds, or the guard takes it for a change
				std::vector<std::wstring> RuleIds;
				RuleIds.push_back(pData->Guid);
				auto ret = CWindowsFirewall::Instance()->LoadRules(RuleIds);
				SWindowsFwRulePtr pWritten = ret.GetValue() ? (*ret.GetValue())[pData->Guid] : SWindowsFwRulePtr();
				if (!pWritten)
					continue;
				theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Template rule '%s' was only partly updated", pCur->Name.c_str());
				pData = pWritten;
			}
			else if (Status.IsError()) {
				theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to update the template rule '%s': %s", pCur->Name.c_str(), Status.GetMessageText());
				continue;
			}
			pRule = UpdateFWRuleUnsafe(pData, pData->Guid);
			Changes.push_back({ pData->Guid, pData->Name, EConfigEvent::eModified });
		}

		std::unique_lock Lock(pRule->m_Mutex);
		pRule->m_uTimeOut = uTimeOut;
		pRule->m_bTemporary = bTemporary;
	}
}

void CFirewall::SetTempalte(const CFirewallRulePtr& pFwRule, const std::vector<std::wstring>& TakeOver)
{
	SWindowsFwRulePtr pData = pFwRule->GetFwRule();

	std::vector<SRuleChange> Changes;

	std::unique_lock Lock(m_RulesMutex);

	// the UI may name a new template itself, so that it knows it by that name before it exists
	if (pData->Guid.empty())
		pData->Guid = MkGuid();
	bool bAdded = m_FwRules.find(pData->Guid) == m_FwRules.end();

	CFirewallRulePtr pRule = UpdateFWRuleUnsafe(pData, pData->Guid);
	pRule->Update(pFwRule);

	//
	// Rules which already exist for programs the template covers can be put under it, as though it
	// had made them. From then on they follow it like the rules it makes. Only what it could have
	// made qualifies: a rule for a binary by path, of its action, in a direction it covers. A rule
	// for a service or an app package says more than a path pattern can carry, and following the
	// template would lose that.
	//
	int TakenOver = 0;
	for (const std::wstring& Guid : TakeOver)
	{
		auto F = m_FwRules.find(Guid);
		if (F == m_FwRules.end())
			continue;
		CFirewallRulePtr pTaken = F->second;
		if (pTaken->IsTemplate() || pTaken->IsBackup())
			continue;
		SWindowsFwRulePtr pTakenData = pTaken->GetFwRule();
		if (!pTakenData->ServiceTag.empty() || !pTakenData->AppContainerSid.empty() || !pTakenData->PackageFamilyName.empty())
			continue;
		if (pTakenData->Action != pData->Action)
			continue;
		if (pData->Direction != EFwDirections::Bidirectional && pData->Direction != pTakenData->Direction)
			continue;
		pTaken->SetTemplateGuid(pRule->GetGuidStr());
		TakenOver++;
		// reported even if the sync below finds nothing else to change, or the UI never learns of the link
		Changes.push_back({ pTakenData->Guid, pTakenData->Name, EConfigEvent::eModified });
	}

	// the sync also drops any taken over rule whose path the pattern does not cover after all
	if (!bAdded || TakenOver > 0)
		SyncDerivedRulesUnsafe(pRule, Changes);
	Lock.unlock();

	EmitChangeEvent(pData->Guid, pData->Name, bAdded ? EConfigEvent::eAdded : EConfigEvent::eModified, true);
	for (const SRuleChange& Change : Changes)
		EmitChangeEvent(Change.Guid, Change.Name, Change.Event, true);
}

//
// Removing a template leaves the rules made from it either gone with it or standing on their
// own. Standing on their own they no longer have a template behind them, so the link is cut, and
// they are rules like any other from then on.
//
bool CFirewall::TryRemoveTemplate(const CFlexGuid& Guid, bool bWithDerived)
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

	std::vector<SRuleChange> Changes;
	for (const CFirewallRulePtr& pDerived : GetDerivedRulesUnsafe(pFwRule->GetGuidStr()))
	{
		if (!bWithDerived) {
			pDerived->SetTemplateGuid(L"");
			Changes.push_back({ pDerived->GetGuidStr(), pDerived->GetName(), EConfigEvent::eModified }); // the link is gone, the UI has to hear of it
		}
		else if (RemoveDerivedRuleUnsafe(pDerived))
			Changes.push_back({ pDerived->GetGuidStr(), pDerived->GetName(), EConfigEvent::eRemoved });
	}

	RemoveRuleUnsafe(pFwRule);

	Lock.unlock();

	EmitChangeEvent(pData->Guid, pData->Name, EConfigEvent::eRemoved, true);
	for (const SRuleChange& Change : Changes)
		EmitChangeEvent(Change.Guid, Change.Name, Change.Event, true);

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

static bool CFirewall__IsNumber(const std::wstring& Value)
{
	if (Value.empty())
		return false;
	for (auto Char : Value) {
		if (Char < L'0' || Char > L'9')
			return false;
	}
	return true;
}

bool CFirewall::ListCoversList(const std::vector<std::wstring>& Template, const std::vector<std::wstring>& Rule, bool bPorts)
{
	if (IsEmptyOrStar(Template))
		return true; // the template puts no limit here, so nothing can step outside of it
	if (IsEmptyOrStar(Rule))
		return false; // the rule however does not either, hence it reaches further

	for (auto& Entry : Rule)
	{
		//
		// A plain single value can be held against the template with the same matcher the rule
		// evaluation uses. Ranges, subnets and keywords are not worth guessing at, those have to
		// be spelled the same way on both sides for us to call it covered.
		//
		bool bCovered = false;
		if (bPorts) {
			if (CFirewall__IsNumber(Entry))
				bCovered = MatchPort((uint16)_wtoi(Entry.c_str()), Template);
		}
		else if (Entry.find_first_of(L"-/") == std::wstring::npos) {
			CAddress Address(Entry);
			if (!Address.IsNull())
				bCovered = MatchAddress(Address, Template);
		}

		if (!bCovered) {
			for (auto& Allowed : Template) {
				if (_wcsicmp(Allowed.c_str(), Entry.c_str()) == 0) {
					bCovered = true;
					break;
				}
			}
		}

		if (!bCovered)
			return false;
	}
	return true;
}

//
// Everything except the action. Whether the action then means approve or reject is for
// AskTemplatesUnsafe to work out, this only answers whether the template reaches the rule at all.
//
bool CFirewall::TemplateAppliesToRule(const SWindowsFwRulePtr& pTemplate, const SWindowsFwRulePtr& pRule)
{
	if (!pTemplate || !pRule)
		return false;

	if (pTemplate->Direction != EFwDirections::Bidirectional && pTemplate->Direction != pRule->Direction)
		return false;
	if (pTemplate->Protocol != EFwKnownProtocols::Any && pTemplate->Protocol != pRule->Protocol)
		return false;

	// everything the rule reaches has to lie inside what the template stands for
	if ((pRule->Profile & ~pTemplate->Profile) != 0)
		return false;
	if (pTemplate->Interface != (int)EFwInterfaces::All && pTemplate->Interface != pRule->Interface)
		return false;

	if (!ListCoversList(pTemplate->RemoteAddresses, pRule->RemoteAddresses, false))
		return false;
	if (!ListCoversList(pTemplate->LocalAddresses, pRule->LocalAddresses, false))
		return false;
	if (!ListCoversList(pTemplate->RemotePorts, pRule->RemotePorts, true))
		return false;
	if (!ListCoversList(pTemplate->LocalPorts, pRule->LocalPorts, true))
		return false;

	return true;
}

CFirewall::ETemplateVerdict CFirewall::AskTemplatesUnsafe(const std::wstring& DosPath, const SWindowsFwRulePtr& pRule, FW::SharedPtr<CFwRuleTemplate>* ppTemplate)
{
	if (ppTemplate) *ppTemplate = FW::SharedPtr<CFwRuleTemplate>();
	if (DosPath.empty() || !pRule)
		return ETemplateVerdict::eUnknown;

	//
	// All matching templates are asked, not just the closest one by path, as a template which
	// sits nearer to the binary may well have nothing to say about this particular rule.
	//
	ETemplateVerdict Verdict = ETemplateVerdict::eUnknown;
	FW::SharedPtr<CFwRuleTemplate> pFound;

	auto Entries = m_FwTemplatesTree.GetEntries(FW::StringW(&g_DefaultMemPool, DosPath.c_str()));
	for (auto Found : Entries)
	{
		auto pTemplate = Found.pEntry.Cast<CFwRuleTemplate>();
		if (!pTemplate)
			continue;
		CFirewallRulePtr pTemplateRule = pTemplate->GetRule();
		if (!pTemplateRule || !pTemplateRule->IsEnabled())
			continue;

		SWindowsFwRulePtr pTemplateData = pTemplateRule->GetFwRule();
		if (!TemplateAppliesToRule(pTemplateData, pRule))
			continue;

		if (pTemplateData->Action == pRule->Action)
		{
			if (Verdict == ETemplateVerdict::eUnknown) {
				Verdict = ETemplateVerdict::eApprove;
				pFound = pTemplate;
			}
			continue;
		}

		//
		// The template forbids what this rule would allow. Block takes precedence over allow in
		// the firewall itself, and it does so here as well, so this ends the search.
		//
		if (pTemplateData->Action == EFwActions::Block && pRule->Action == EFwActions::Allow)
		{
			if (ppTemplate) *ppTemplate = pTemplate;
			return ETemplateVerdict::eReject;
		}

		//
		// An allow template says nothing about a block rule. Reverting that would take protection
		// away rather than grant it, which is not a call to make without asking.
		//
	}

	if (ppTemplate) *ppTemplate = pFound;
	return Verdict;
}

//
// Decides whether a change to the windows firewall needs no asking, because the user has already
// made that very decision in the shape of a rule template, or has asked for allow rule removals
// to go through unattended.
//
CFwAutoGuardEntry::EMode CFirewall::DecideRuleChangeUnsafe(const SWindowsFwRulePtr& pRule, const CFirewallRulePtr& pFwRule, bool bProgramChanged, const std::wstring& OldPath, const std::wstring& NewPath)
{
	if (!pRule) // the rule is going away
	{
		//
		// Taking an allow rule away only ever removes access, so where the user asked for it we
		// let that happen quietly. Together with an allow template this is what carries a store
		// update through, which drops the rule of the old version and adds one for the new path.
		//
		if (pFwRule && pFwRule->GetFwRule() && pFwRule->GetFwRule()->Action == EFwActions::Allow
		 && theCore->Config()->GetBool("Service", "FwAutoApproveAllowRemoval", false))
			return CFwAutoGuardEntry::eApprove;
		return CFwAutoGuardEntry::eNone;
	}

	if (!theCore->Config()->GetBool("Service", "UseFwRuleTemplates", true)
	 || !theCore->Config()->GetBool("Service", "FwTemplateGuard", true))
		return CFwAutoGuardEntry::eNone;

	FW::SharedPtr<CFwRuleTemplate> pTemplate;
	ETemplateVerdict Verdict = AskTemplatesUnsafe(NewPath, pRule, &pTemplate);

	//
	// A template which forbids what this rule would allow is an answer in its own right. Where the
	// rule came from does not change that, so unlike an approval this needs no further checking.
	//
	if (Verdict == ETemplateVerdict::eReject)
		return CFwAutoGuardEntry::eReject;

	if (Verdict != ETemplateVerdict::eApprove)
		return CFwAutoGuardEntry::eNone;

	if (!bProgramChanged)
		return CFwAutoGuardEntry::eApprove; // a new rule, or one which stays with its program

	//
	// The rule was moved to a different binary. Letting that pass on the strength of the new path
	// alone would hand the trust of one program over to another, so the path it comes from has to
	// be covered as well, and by the very same template, or the move leaves what the template means.
	//
	FW::SharedPtr<CFwRuleTemplate> pOldTemplate;
	ETemplateVerdict OldVerdict = AskTemplatesUnsafe(OldPath, pFwRule ? pFwRule->GetFwRule() : nullptr, &pOldTemplate);
	if (OldVerdict == ETemplateVerdict::eApprove && pOldTemplate && pTemplate && pOldTemplate->GetRule() == pTemplate->GetRule())
		return CFwAutoGuardEntry::eApprove;

	return CFwAutoGuardEntry::eNone;
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

	//
	// Which program the rule belonged to before, and which one it belongs to now. On a plain add
	// or remove only one of the two exists, and when a rule is moved from one binary to another,
	// as a store update does, the two differ and both sides have to be accounted for.
	//
	std::wstring OldPath, NewPath;
	if (pFwRule)
		OldPath = pFwRule->GetProgramID().GetFilePath();
	if (pRule)
	{
		std::wstring BinaryPath = pRule->BinaryPath;
		if (_wcsicmp(BinaryPath.c_str(), L"system") == 0)
			BinaryPath = CProcess::NtOsKernel_exe;

		//std::wstring AppContainerSid = pRule->AppContainerSid;
		//if(AppContainerSid.empty() && !pRule->PackageFamilyName.empty())
		//	AppContainerSid = GetAppContainerSidFromName(pRule->PackageFamilyName);

		NewPath = CProgramID::FromFw(BinaryPath, pRule->ServiceTag, L"").GetFilePath();
	}

	FW::SharedPtr<CFwAutoGuardEntry> pGuardEntry;

	if (!bProgramChanged)
	{
		// TODO: add support for not only path based macthing

		const std::wstring& DosPath = pFwRule ? OldPath : NewPath;
		pGuardEntry = m_AutoGuardTree.GetBestEntry(FW::StringW(&g_DefaultMemPool, DosPath.c_str())).Cast<CFwAutoGuardEntry>();
	}

	CFwAutoGuardEntry::EMode GuardAction = CFwAutoGuardEntry::eNone;
	if (pGuardEntry)
		GuardAction = pGuardEntry->GetMode(); // an entry the user wrote for this very path has the final say
	else
		GuardAction = DecideRuleChangeUnsafe(pRule, pFwRule, bProgramChanged, OldPath, NewPath);

	//
	// A rule the guard had already turned down, but which was not yet disabled or deleted, is no
	// longer there to act on in that form. And if it is now gone altogether, that is the very
	// outcome the guard was after, so putting it back would be the wrong way round.
	//
	// The same goes for one it did disable. Nothing else remembers that it was turned down, so
	// without this its removal was reverted like any other, and the rejected rule came back.
	// This outranks a guard entry as well: rejecting a rule and then reverting its removal is
	// the same entry contradicting itself.
	//
	bool bTurnedDown = pFwRule && m_RulesToRemove.erase(pFwRule) > 0;
	if (pFwRule && pFwRule->IsTurnedDown())
		bTurnedDown = true;

	//
	// A copy held back for its original to go, which went or changed first itself. Nothing was
	// decided about it yet, and it going away leaves the firewall as it was before it came.
	//
	bool bWasHeld = pFwRule && TakeHeldRuleUnsafe(pFwRule);

	if (!pRule && (bTurnedDown || bWasHeld))
		GuardAction = CFwAutoGuardEntry::eApprove;

	//
	// A rule replaced by one which is the same in every field is not a change at all, it only
	// looks like two of them. Saying yes to both halves is the one answer which leaves the
	// firewall as it stands, so it is given here instead of being asked for twice.
	//
	// This comes before the blanket guard: reverting both halves would put back the rule which
	// went away and take away the identical one which replaced it, every time the program does it.
	//
	// Either half may come first, so either one waits a moment for the other: a removal with
	// nothing standing in for it yet (bHoldBack), and an addition while its original still
	// stands (bHoldAdd). Waiting is what lets a service tick fall between the two halves.
	//
	bool bGuardAll = theCore->Config()->GetBool("Service", "GuardFwRules", false);
	CFirewallRulePtr pTwin;
	CFirewallRulePtr pSettled; // the original of this addition, whose removal went through already
	bool bHoldBack = false;
	bool bHoldAdd = false;
	if (GuardAction == CFwAutoGuardEntry::eNone && !bProgramChanged
	 && theCore->Config()->GetBool("Service", "FwAutoApproveRecreate", true))
	{
		if (pRule)
			pTwin = TakeWithdrawnTwinUnsafe(pRule); // the one a removal is being held back for
		else if (pFwRule && !pFwRule->IsBackup())
			pTwin = FindLiveTwinUnsafe(pFwRule); // an identical one already stands in for it

		if (pTwin)
			GuardAction = CFwAutoGuardEntry::eApprove;
		else if (pRule && !pFwRule && (pSettled = TakeSettledTwinUnsafe(pRule)))
			GuardAction = CFwAutoGuardEntry::eApprove;
		else if (pRule)
			bHoldAdd = !pFwRule && FindLiveTwinUnsafe(pRule); // a plain addition, its original still there
		else
			bHoldBack = pFwRule && !pFwRule->IsBackup();
	}

	//
	// A removal let through on other grounds, by a guard entry or the removal option, can still be
	// the second half of a replacement. A copy held back for it is settled with it, rather than
	// asked about once its time runs out as though the original had stayed.
	//
	if (!pRule && !pTwin && pFwRule && !pFwRule->IsBackup() && GuardAction == CFwAutoGuardEntry::eApprove)
	{
		for (const SHeldRule& Held : m_HeldRules) {
			if (Held.pRule->Match(pFwRule->GetFwRule())) {
				pTwin = Held.pRule;
				break;
			}
		}
	}

	if (GuardAction == CFwAutoGuardEntry::eNone && bGuardAll && !bHoldBack && !bHoldAdd)
		GuardAction = CFwAutoGuardEntry::eReject;

	if (pFwRule && pFwRule->IsBackup())
	{
		// we arrive here only from UpdateRules()
		bool bWithdrawn = std::any_of(m_WithdrawnRules.begin(), m_WithdrawnRules.end(),
			[&](const SWithdrawnRule& Entry) { return Entry.pBackup == pFwRule; }); // still waiting for its twin
		if (GuardAction == CFwAutoGuardEntry::eReject && !bWithdrawn)
			m_RulesToRevert.insert(pFwRule);
		return false; // nothing changed
	}

	bool bExpected = false;
	if (pFwRule)  // modified or removed
	{
		ASSERT(!pFwRule->IsTemplate()); // should not happen

		if (/*!pFwRule->IsApproved() ||*/ GuardAction == CFwAutoGuardEntry::eApprove)
		{
			RemoveRuleUnsafe(pFwRule); // a non approved rule chaneg or the rule is to be auto approved

			if (!pRule) // a removal is settled right here, there is no new rule to report it on
			{
				bExpected = true;

				// paired with a twin nothing was decided, the rule only changed its guid, and an unapproved one is not reported approved for it
				if (!pTwin || pFwRule->IsApproved())
				{
					StVariant Data;
					Data[API_V_GUID] = pFwRule->GetGuidStr();
					Data[API_V_NAME] = pFwRule->GetName();
					Data[API_V_ID] = pFwRule->GetProgramID().ToVariant(SVarWriteOpt());
					theCore->EmitEvent(ELogLevels::eInfo, eLogFwRuleApproved, Data);
				}

				// let through without a twin, so the twin may yet follow; a held copy was never a rule anyone decided about
				if (!pTwin && !bWasHeld)
				{
					SSettledRemoval Settled;
					Settled.pRule = pFwRule;
					Settled.uRemoved = GetTickCount64();
					m_SettledRemovals.push_back(Settled);
				}
			}
		}
		else
		{
			RemoveRuleUnsafeImpl(pFwRule);

			pFwRule->SetAsBackup(!pRule, bProgramChanged);

			m_FwRules.insert(std::make_pair(pFwRule->GetGuidStr(), pFwRule));

			EmitChangeEvent(pFwRule->GetGuidStr(), pFwRule->GetFwRule()->Name, EConfigEvent::eAdded, true, true); // event for the backup rule

			if (GuardAction == CFwAutoGuardEntry::eReject) 
			{
				m_RulesToRevert.insert(pFwRule);
				bExpected = true;
			}
			else if (bHoldBack)
			{
				// not settled, only kept quiet, the question (or the revert) follows if nothing takes its place
				SWithdrawnRule Withdrawn;
				Withdrawn.pBackup = pFwRule;
				Withdrawn.uWithdrawn = GetTickCount64();
				Withdrawn.bRevert = bGuardAll;
				m_WithdrawnRules.push_back(Withdrawn);
				bExpected = true;
			}
		}
	}

	if (!pRule && pTwin)
	{
		//
		// The removal is being let through because this twin says the same thing, so the twin has
		// to be settled in the same breath. Leaving it pending would mean one answer could still
		// undo what the other was allowed on the strength of.
		//
		// Settled means it takes the place of the original, verdict included: approved if that
		// was, unapproved if that was. It used to be approved either way, so a program deleting
		// and recreating a rule nobody had looked at got it approved for nothing. One which was
		// approved in its own right, by a template say, stays so.
		//
		bool bWasApproved = pTwin->IsApproved();
		if (!bWasApproved)
			pTwin->TakeOver(pFwRule);

		// it was waiting for this very removal, or the blanket guard turned it down on arrival and that has not been carried out yet
		TakeHeldRuleUnsafe(pTwin);
		m_RulesToRemove.erase(pTwin);

		if (!bWasApproved && pTwin->IsApproved())
		{
			StVariant Data;
			Data[API_V_GUID] = pTwin->GetGuidStr();
			Data[API_V_NAME] = pTwin->GetName();
			Data[API_V_ID] = pTwin->GetProgramID().ToVariant(SVarWriteOpt());
			theCore->EmitEvent(ELogLevels::eInfo, eLogFwRuleApproved, Data);
		}
	}

	if (pRule && pTwin) // the replacement for a rule a removal was being held back for
	{
		//
		// The removal was never put to the user and now it does not have to be: what it took away
		// is exactly what is being put back. The backup goes, and the new rule takes over what was
		// known about the old one below.
		//
		RemoveRuleUnsafe(pTwin);
		EmitChangeEvent(pTwin->GetGuidStr(), pTwin->GetName(), EConfigEvent::eRemoved, true, true);

		if (pTwin->IsApproved())
		{
			StVariant Data;
			Data[API_V_GUID] = pTwin->GetOriginalGuid();
			Data[API_V_NAME] = pTwin->GetName();
			Data[API_V_ID] = pTwin->GetProgramID().ToVariant(SVarWriteOpt());
			theCore->EmitEvent(ELogLevels::eInfo, eLogFwRuleApproved, Data);
		}
	}

	if (pRule) // added or modified
	{
		CFirewallRulePtr pNewRule = std::make_shared<CFirewallRule>(pRule);
		AddRuleUnsafe(pNewRule);
		if (pTwin || pSettled)
		{
			pNewRule->TakeOver(pTwin ? pTwin : pSettled); // the backup, or the rule gone, carries the verdict the original had
			bExpected = true;

			if (pNewRule->IsApproved())
			{
				StVariant Data;
				Data[API_V_GUID] = pNewRule->GetOriginalGuid();
				Data[API_V_NAME] = pNewRule->GetName();
				Data[API_V_ID] = pNewRule->GetProgramID().ToVariant(SVarWriteOpt());
				theCore->EmitEvent(ELogLevels::eInfo, eLogFwRuleApproved, Data);
			}
		}
		else if (GuardAction == CFwAutoGuardEntry::eApprove)
		{
			pNewRule->SetApproved(true);
			bExpected = true;

			StVariant Data;
			Data[API_V_GUID] = pNewRule->GetOriginalGuid();
			Data[API_V_NAME] = pNewRule->GetName();
			Data[API_V_ID] = pNewRule->GetProgramID().ToVariant(SVarWriteOpt());
			theCore->EmitEvent(ELogLevels::eInfo, eLogFwRuleApproved, Data);
		}
		else if (bHoldAdd)
		{
			// not settled, only kept quiet, the question (or the reject) follows if the original stays
			SHeldRule Held;
			Held.pRule = pNewRule;
			Held.uHeld = GetTickCount64();
			Held.bReject = bGuardAll;
			m_HeldRules.push_back(Held);
			bExpected = true;
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
	EConfigEvent Type = pEvent->Type;

	SWindowsFwRulePtr pRule;
	if (Type != EConfigEvent::eRemoved) {
		std::vector<std::wstring> RuleIds;
		RuleIds.push_back(pEvent->RuleId);
		auto ret = CWindowsFirewall::Instance()->LoadRules(RuleIds);
		if(ret.GetValue()) pRule = (*ret.GetValue())[pEvent->RuleId]; // CWindowsFirewall::Instance()->GetRule(pEvent->RuleId);
		if (!pRule) Type = EConfigEvent::eRemoved;
	}

	std::unique_lock Lock(m_RulesMutex);

	CFirewallRulePtr pFwRule;
	if (!pEvent->RuleId.empty()) {
		auto F = m_FwRules.find(pEvent->RuleId);
		if (F != m_FwRules.end())
			pFwRule = F->second;
	}

	std::wstring Name;
	if(pRule)
		Name = pRule->Name;
	else if (pFwRule)
		Name = pFwRule->GetName();
	else
		return 0;

	bool bExpected = false;
	bool bChanged = FWRuleChangedUnsafe(pRule, pFwRule, &bExpected);

	Lock.unlock();

	if (bChanged) {

		StVariant Data;
		Data[API_V_GUID] = pEvent->RuleId;
		Data[API_V_NAME] = Name; 
		if(pFwRule) 
			Data[API_V_ID] = pFwRule->GetProgramID().ToVariant(SVarWriteOpt());
		switch (Type)
		{
		case EConfigEvent::eAdded:		theCore->EmitEvent(ELogLevels::eWarning, eLogFwRuleAdded, Data); break;
		case EConfigEvent::eModified:	theCore->EmitEvent(ELogLevels::eWarning, eLogFwRuleModified, Data); break;
		case EConfigEvent::eRemoved:	theCore->EmitEvent(ELogLevels::eWarning, eLogFwRuleRemoved, Data); break;
		}
	}

	EmitChangeEvent(pEvent->RuleId, Name, Type, !bChanged || bExpected, bChanged);

	return 0;
}

uint32 CFirewall::OnFwLogEvent(const SWinFwLogEvent* pEvent)
{
	CSocketPtr pSocket = theCore->NetworkManager()->SocketList()->OnFwLogEvent(pEvent);

	ProcessFwEvent(pEvent, pSocket.get());

	return 0;
}

void CFirewall::EmitChangeEvent(const CFlexGuid& Guid, const std::wstring& Name, enum class EConfigEvent Event, bool bExpected, bool bNoConfig)
{
	StVariant vEvent;
	vEvent[API_V_GUID] = Guid.ToVariant(false);
	vEvent[API_V_NAME] = Name;
	vEvent[API_V_EVENT_TYPE] = (uint32)Event;
	vEvent[API_V_EVENT_EXPECTED] = bExpected;

	theCore->BroadcastMessage(SVC_API_EVENT_FW_RULE_CHANGED, vEvent);

	if(bNoConfig) 
		return;

	StVariant Data;
	Data[API_V_GUID] = Guid.ToVariant(false);
	Data[API_V_NAME] = Name; 
	switch (Event)
	{
	case EConfigEvent::eAdded:		theCore->EmitEvent(ELogLevels::eNone, eLogFwRuleAdded, Data); break;
	case EConfigEvent::eModified:	theCore->EmitEvent(ELogLevels::eNone, eLogFwRuleModified, Data); break;
	case EConfigEvent::eRemoved:	theCore->EmitEvent(ELogLevels::eNone, eLogFwRuleRemoved, Data); break;
	}
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
		pProcess = pSocket ? pSocket->GetProcess() : theCore->ProcessList()->GetProcess(pEvent->ProcessId, pEvent->ProcessFileName, true, FILETIME2ms(pEvent->TimeStamp)); // todo socket should always have a process

		//
		// The event is read from the log seconds after the fact and names its image. A process
		// found by pid that runs something else was handed the pid since; then nothing it says
		// (program, package, services) applies and the program is taken from the path below.
		//
		if (pProcess && !pProcess->IsSameImage(pEvent->ProcessFileName))
			pProcess.reset();

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
		pProcess->UpdateLastNetActivity(FILETIME2ms(pEvent->TimeStamp)); // m_LastNetActivity is kept in ms, see CProcess::AddNetworkIO

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

		SRuleMatch AllMatch = MatchRulesWithEvent(theCore->ProgramManager()->GetAllItem()->GetFwRules(), pEvent, pProcess, AllowRules, BlockRules, pNicInfo);
		RuleAction.TestAndSetOrClear(AllMatch, pEvent);

		SRuleMatch FileMatch = MatchRulesWithEvent(pProgram->GetFwRules(), pEvent, pProcess, AllowRules, BlockRules, pNicInfo);
		RuleAction.TestAndSetOrClear(FileMatch, pEvent);

		SRuleMatch AppMatch;
		if (pApp) AppMatch = MatchRulesWithEvent(pApp->GetFwRules(), pEvent, pProcess, AllowRules, BlockRules, pNicInfo);
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

			SRuleMatch SvcMatch = MatchRulesWithEvent(pSvc->GetFwRules(), pEvent, pProcess, AllowRules, BlockRules, pNicInfo);
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

	//
	// A rule made from a template is the template at work for one binary, so a hit on it is a hit
	// on the template as well. That is where the user looks to see whether a template is in use.
	//
	if (!AllowRules.empty() || !BlockRules.empty())
	{
		std::unique_lock Lock(m_RulesMutex);
		auto CountForTemplate = [&](const CFlexGuid& Guid) {
			auto F = m_FwRules.find(Guid);
			if (F == m_FwRules.end())
				return;
			std::wstring TemplateGuid = F->second->GetTemplateGuid();
			if (TemplateGuid.empty())
				return;
			auto T = m_FwRules.find(TemplateGuid);
			if (T != m_FwRules.end() && T->second->IsTemplate())
				T->second->IncrHitCount();
		};
		for (const CFlexGuid& Guid : AllowRules) CountForTemplate(Guid);
		for (const CFlexGuid& Guid : BlockRules) CountForTemplate(Guid);
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
				SWindowsFwRulePtr pData = MakeDerivedRuleData(pTemplate, DosPath, pEvent->Direction);

				pFwRule->IncrHitCount(); // the template decided this connection, the rule it makes now only carries that out

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

	theCore->QueueBroadcast(SVC_API_EVENT_NET_ACTIVITY, Event); //, pProgram); - always bradcast network activity wven if the process is not actively watched
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

CFirewall::SRuleMatch CFirewall::MatchRulesWithEvent(const std::set<CFirewallRulePtr>& Rules, const struct SWinFwLogEvent* pEvent, const CProcessPtr& pProcess, std::vector<CFlexGuid>& AllowRules, std::vector<CFlexGuid>& BlockRules, std::shared_ptr<struct SAdapterInfo> pNicInfo)
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

		if (pRule->HasPrincipalRestriction()) {
			if (!pProcess || !MatchUserAuthorization(pRule, pProcess->GetUserSid(), pProcess->GetUserGroups()))
				continue;
		}

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

//bool CFirewall::MatchUserAuthorization(const std::wstring& Sddl, const std::wstring& UserSid, const std::map<std::wstring, SGroupInfo>& UserGroups)
//{
//	// SDDL format: "O:LSD:(A;;CC;;;SID)(D;;CC;;;SID)..."
//	// ACE format: (type;flags;rights;object_guid;inherit_object_guid;account_sid)
//	// We need to check Allow (A) and Deny (D) ACEs against user SID and enabled groups
//
//	auto MatchesSid = [&](const std::wstring& AceSid) -> bool {
//		if (_wcsicmp(AceSid.c_str(), UserSid.c_str()) == 0)
//			return true;
//		auto it = UserGroups.find(AceSid);
//		return it != UserGroups.end() && it->second.Enabled;
//	};
//
//	// Find DACL section (after "D:")
//	size_t daclPos = Sddl.find(L"D:");
//	if (daclPos == std::wstring::npos)
//		return true; // No DACL means no restrictions
//
//	std::wstring dacl = Sddl.substr(daclPos + 2);
//
//	bool hasAllowMatch = false;
//	bool hasApplyEntries = false;
//	size_t pos = 0;
//
//	while ((pos = dacl.find(L'(', pos)) != std::wstring::npos) {
//		size_t end = dacl.find(L')', pos);
//		if (end == std::wstring::npos)
//			break;
//
//		std::wstring ace = dacl.substr(pos + 1, end - pos - 1);
//		pos = end + 1;
//
//		// Parse ACE: type;flags;rights;object_guid;inherit_object_guid;account_sid
//		auto parts = SplitStr(ace, L";");
//		if (parts.size() < 6)
//			continue;
//
//		const std::wstring& aceType = parts[0];
//		const std::wstring& aceSid = parts[5];
//
//		if (aceType == L"D" && MatchesSid(aceSid))
//			return false; // Explicit deny takes precedence
//
//		if (aceType == L"A") {
//			hasApplyEntries = true;
//			if (MatchesSid(aceSid))
//				hasAllowMatch = true;
//		}
//	}
//
//	// If there are Apply entries, user must match one; otherwise rule applies to all non-exempt
//	return hasApplyEntries ? hasAllowMatch : true;
//}

bool CFirewall::MatchUserAuthorization(const CFirewallRulePtr& pRule, const std::wstring& UserSid, const std::map<std::wstring, SGroupInfo>& UserGroups)
{
	auto principalSids = pRule->GetPrincipalSids();
	if (!principalSids || principalSids->empty())
		return true; // No restrictions

	bool bHasApply = false;
	bool bHasExempt = false;

	// Convert user SID to uppercase for lookup
	std::wstring userSidUpper = UserSid;
	std::transform(userSidUpper.begin(), userSidUpper.end(), userSidUpper.begin(), ::towupper);

	// Check user SID
	auto it = principalSids->find(userSidUpper);
	if (it != principalSids->end()) {
		if (it->second.Type == ESidEntryType::eExempt)
			bHasExempt = true; // User is explicitly exempt
		else if (it->second.Type == ESidEntryType::eApply)
			bHasApply = true; // User is explicitly allowed
	}

	// Check group SIDs
	for (const auto& group : UserGroups) {
		if (!group.second.Enabled)
			continue;

		std::wstring groupSidUpper = group.first;
		std::transform(groupSidUpper.begin(), groupSidUpper.end(), groupSidUpper.begin(), ::towupper);

		auto git = principalSids->find(groupSidUpper);
		if (git != principalSids->end()) {
			if (git->second.Type == ESidEntryType::eExempt)
				bHasExempt = true; // Group is explicitly exempt
			else if (git->second.Type == ESidEntryType::eApply)
				bHasApply = true; // Group is explicitly allowed
		}
	}

	// User is exempt - rule does not apply
	if (bHasExempt)
		return false;
	// If there are Apply entries, user must match one
	if (pRule->HasPrincipalApplyEntries() && !bHasApply)
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