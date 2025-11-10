#include "pch.h"
#include "PresetManager.h"
#include "../ServiceCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/Common/Strings.h"
#include "../../Library/Common/FileIO.h"
#include "../../Library/Helpers/MiscHelpers.h"
#include "../../Library/Helpers/WinUtil.h"
#include "../Programs/ProgramManager.h"
#include "../Programs/ProgramRule.h"
#include "../Access/AccessManager.h"
#include "../Access/AccessRule.h"
#include "../Network/NetworkManager.h"
#include "../Network/Firewall/Firewall.h"
#include "../Network/Firewall/FirewallRule.h"
#include "../Tweaks/TweakManager.h"

#define API_DNS_Preset_FILE_NAME L"Presets.dat"
#define API_DNS_Preset_FILE_VERSION 1

CPresetManager::CPresetManager()
{
}


STATUS CPresetManager::Init()
{
	if (!Load())
	{
		CPresetPtr pEntry = std::make_shared<CPreset>();
		pEntry->SetGuid(MkGuid());
		pEntry->SetName(L"Default-Preset");

		m_Presets[pEntry->GetGuid()] = pEntry;
	}

	return OK;
}

STATUS CPresetManager::Load()
{
	CBuffer Buffer;
	if (!ReadFile(theCore->GetDataFolder() + L"\\" API_DNS_Preset_FILE_NAME, Buffer)) {
		theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_DNS_Preset_FILE_NAME L" not found");
		return ERR(STATUS_NOT_FOUND);
	}

	StVariant Data;
	//try {
	auto ret = Data.FromPacket(&Buffer, true);
	//} catch (const CException&) {
	//	return ERR(STATUS_UNSUCCESSFUL);
	//}
	if (ret != StVariant::eErrNone) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_DNS_Preset_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	if (Data[API_S_VERSION].To<uint32>() != API_DNS_Preset_FILE_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_DNS_Preset_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	LoadEntries(Data[API_S_PRESETS]);

	// Build active Presets set from IsActive flags on each Preset
	m_ActivePresets.clear();
	for (const auto& [Guid, pPreset] : m_Presets)
	{
		if (pPreset->IsActive())
			m_ActivePresets.insert(Guid);
	}

	// Load item ownership
	const StVariant& OwnershipData = Data[API_S_ITEM_OWNERSHIP];
	if (OwnershipData.GetType() == VAR_TYPE_LIST)
	{
		for (uint32 i = 0; i < OwnershipData.Count(); i++)
		{
			const StVariant& OwnershipEntry = OwnershipData[i];

			CFlexGuid ItemGuid;
			ItemGuid.FromVariant(OwnershipEntry[API_S_GUID]);

			if (!ItemGuid.IsNull())
			{
				SItemOwnership Ownership;
				Ownership.PresetGuid.FromVariant(OwnershipEntry[API_S_PRESET_GUID]);

				std::string TypeStr = OwnershipEntry[API_S_STATE_TYPE];
				Ownership.Type = ItemTypeFromString(TypeStr.c_str());
				Ownership.bWasEnabled = OwnershipEntry[API_S_STATE_WAS_ENABLED].To<bool>();
				//Ownership.bExisted = OwnershipEntry[API_S_STATE_EXISTED].To<bool>();

				m_ItemOwnership[ItemGuid] = Ownership;
			}
		}
	}

	return OK;
}

STATUS CPresetManager::Store()
{
	SVarWriteOpt Opts;
	Opts.Format = SVarWriteOpt::eIndex;
	Opts.Flags = SVarWriteOpt::eSaveToFile | SVarWriteOpt::eTextGuids;

	StVariant Data;
	Data[API_S_VERSION] = API_DNS_Preset_FILE_VERSION;
	Data[API_S_PRESETS] = SaveEntries(Opts);

	// Save item ownership
	std::shared_lock Lock(m_Mutex);
	
	StVariantWriter OwnershipWriter;
	OwnershipWriter.BeginList();
	for (const auto& [ItemGuid, Ownership] : m_ItemOwnership)
	{
		StVariantWriter ItemWriter;
		ItemWriter.BeginMap();
		ItemWriter.WriteVariant(API_S_GUID, ItemGuid.ToVariant(true));

		ItemWriter.WriteVariant(API_S_PRESET_GUID, Ownership.PresetGuid.ToVariant(true));

		ItemWriter.Write(API_S_STATE_TYPE, ItemTypeToString(Ownership.Type));
		ItemWriter.Write(API_S_STATE_WAS_ENABLED, Ownership.bWasEnabled);
		//ItemWriter.Write(API_S_STATE_EXISTED, Ownership.bExisted);
		OwnershipWriter.WriteVariant(ItemWriter.Finish());
	}
	Data[API_S_ITEM_OWNERSHIP] = OwnershipWriter.Finish();

	Lock.unlock();

	CBuffer Buffer;
	Data.ToPacket(&Buffer);
	WriteFile(theCore->GetDataFolder() + L"\\" API_DNS_Preset_FILE_NAME, Buffer);

	return OK;
}


STATUS CPresetManager::LoadEntries(const StVariant& Entries)
{
	std::unique_lock lock(m_Mutex);

	m_Presets.clear();

	for (uint32 i = 0; i < Entries.Count(); i++)
	{
		StVariant Entry = Entries[i];

		CPresetPtr pEntry = std::make_shared<CPreset>();
		STATUS Status = pEntry->FromVariant(Entry);
		if (Status.IsError())
			; //  todo log error
		else
		{
			if (pEntry->GetGuid().IsNull())
				pEntry->SetGuid(MkGuid());
			m_Presets[pEntry->GetGuid()] = pEntry;
		}
	}

	return OK;
}

StVariant CPresetManager::SaveEntries(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool)
{
	std::shared_lock lock(m_Mutex);

	StVariantWriter Entries(pMemPool);
	Entries.BeginList();
	for (auto& pEntry : m_Presets)
	{
		// Set the IsActive flag based on whether this Preset is currently active
		bool bIsActive = (m_ActivePresets.find(pEntry.first) != m_ActivePresets.end());
		pEntry.second->SetIsActive(bIsActive);

		Entries.WriteVariant(pEntry.second->ToVariant(Opts, pMemPool));
	}
	return Entries.Finish();
}

STATUS CPresetManager::SetPresets(const StVariant& Presets)
{
	STATUS Status = LoadEntries(Presets);
	if (Status.IsError())
		return Status;

	return Store();
}

CPresetPtr CPresetManager::GetPreset(const CFlexGuid& Guid) const
{
	std::shared_lock lock(m_Mutex);

	auto F = m_Presets.find(Guid);
	if (F != m_Presets.end())
		return F->second;
	return nullptr;
}

RESULT(StVariant) CPresetManager::SetEntry(const StVariant& Entry)
{
	std::unique_lock lock(m_Mutex);

	CFlexGuid Guid;
	Guid.FromVariant(Entry[API_V_GUID]);

	CPresetPtr pEntry;
	if (Guid.IsNull()) // new entry
		Guid.FromWString(MkGuid());
	else {
		auto F = m_Presets.find(Guid);
		if (F != m_Presets.end())
			pEntry = F->second;
	}

	bool bAdded = false;
	if (!pEntry) // not found or new entry
	{
		pEntry = std::make_shared<CPreset>();
		pEntry->SetGuid(Guid);
		m_Presets[Guid] = pEntry;
		bAdded = true;
	}

	// update entry
	STATUS Status = pEntry->FromVariant(Entry);
	if (Status.IsError())
		return Status;
	
	EmitChangeEvent(Guid, bAdded ? EConfigEvent::eAdded : EConfigEvent::eModified);
	RETURN(Guid.ToVariant(false, Entry.Allocator()));
}

RESULT(StVariant) CPresetManager::GetEntry(const std::wstring& EntryId, const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool)
{
	std::shared_lock lock(m_Mutex);

	CFlexGuid Guid(EntryId);
	auto F = m_Presets.find(Guid);
	if (F == m_Presets.end())
		return ERR(STATUS_NOT_FOUND);
		
	RETURN(F->second->ToVariant(Opts, pMemPool));
}

STATUS CPresetManager::DelEntry(const std::wstring& EntryId)
{
	std::unique_lock lock(m_Mutex);

	CFlexGuid Guid(EntryId);
	auto F = m_Presets.find(Guid);
	if (F == m_Presets.end())
		return ERR(STATUS_NOT_FOUND);
	m_Presets.erase(F);

	EmitChangeEvent(Guid, EConfigEvent::eRemoved);
	return OK;
}

void CPresetManager::EmitChangeEvent(const CFlexGuid& Guid, enum class EConfigEvent Event)
{
	StVariant vEvent;
	vEvent[API_V_GUID] = Guid.ToVariant(false);
	//vEvent[API_V_NAME] = ;
	vEvent[API_V_EVENT_TYPE] = (uint32)Event;

	theCore->BroadcastMessage(SVC_API_EVENT_PRESET_CHANGED, vEvent);
}

STATUS CPresetManager::DeactivatePresetInternal(const CFlexGuid& Preset, uint32 CallerPID)
{
	for (auto I = m_ItemOwnership.begin(); I != m_ItemOwnership.end(); )
	{
		const CFlexGuid& ItemGuid = I->first;
		const SItemOwnership& OriginalState = I->second;

		if (OriginalState.PresetGuid != Preset) {
			++I;
			continue;
		}
			
		switch (OriginalState.Type)
		{
		case EItemType::eExecRule: // Program/Execution Rule
			if (CProgramRulePtr pRule = theCore->ProgramManager()->GetRule(ItemGuid)) {
				pRule->SetEnabled(OriginalState.bWasEnabled);
				auto Result = theCore->ProgramManager()->AddRule(pRule);
				if (Result.IsError())
					theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to revert program rule %s", ItemGuid.ToWString().c_str());
			}
			break;

		case EItemType::eResRule: // Resource/Access Rule
			if (CAccessRulePtr pRule = theCore->AccessManager()->GetRule(ItemGuid)) {
				pRule->SetEnabled(OriginalState.bWasEnabled);
				auto Result = theCore->AccessManager()->AddRule(pRule);
				if (Result.IsError())
					theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to revert access rule %s", ItemGuid.ToWString().c_str());
			}
			break;

		case EItemType::eFwRule: // Firewall Rule
			if (CFirewallRulePtr pRule = theCore->NetworkManager()->Firewall()->GetRule(ItemGuid)) {
				pRule->SetEnabled(OriginalState.bWasEnabled);
				STATUS Status = theCore->NetworkManager()->Firewall()->SetRule(pRule);
				if (Status.IsError())
					theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to revert firewall rule %s", ItemGuid.ToWString().c_str());

				if (pRule->IsTemplate())
				{
					auto DerivedRules = theCore->NetworkManager()->Firewall()->GetRulesFromTemplate(ItemGuid);
					for (auto& pDerivedRule : DerivedRules)
					{
						pDerivedRule->SetEnabled(OriginalState.bWasEnabled);
						STATUS DerivedStatus = theCore->NetworkManager()->Firewall()->SetRule(pDerivedRule);
						if (DerivedStatus.IsError())
							theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to revert firewall rule %s (derived from template %s)", pDerivedRule->GetGuidStr().c_str(), ItemGuid.ToWString().c_str());
					}
				}
			}
			break;

		case EItemType::eTweak: // System Tweak
			if (OriginalState.bWasEnabled)
				theCore->TweakManager()->ApplyTweak(ItemGuid.ToWString(), CallerPID);
			else
				theCore->TweakManager()->UndoTweak(ItemGuid.ToWString(), CallerPID);
			break;
		}

		I = m_ItemOwnership.erase(I);
	}

	return OK;
}

std::vector<std::pair<CFlexGuid, CFlexGuid>> CPresetManager::CheckPresetConflicts(const CPresetPtr& pPreset)
{
	std::vector<std::pair<CFlexGuid, CFlexGuid>> Conflicts;
	for (const auto& [ItemGuid, ItemPreset] : pPreset->GetItems())
	{
		if (ItemPreset.Activate == SItemPreset::EActivate::eUndefined)
			continue;

		auto OwnerIt = m_ItemOwnership.find(ItemGuid);
		if (OwnerIt != m_ItemOwnership.end() && OwnerIt->second.PresetGuid != pPreset->GetGuid())
			Conflicts.push_back(std::make_pair(ItemGuid, OwnerIt->second.PresetGuid));
	}
	return Conflicts;
}

STATUS CPresetManager::ApplyPresetItems(const CPresetPtr& pPreset, uint32 CallerPID, bool bForce)
{
	for (auto& [ItemGuid, ItemPreset] : pPreset->GetItems())
	{
		if (ItemPreset.Activate == SItemPreset::EActivate::eUndefined)
			continue;

		// Check if item is already owned by this Preset
		auto OwnerIt = m_ItemOwnership.find(ItemGuid);
		if (OwnerIt != m_ItemOwnership.end())
		{
			if (OwnerIt->second.PresetGuid == pPreset->GetGuid())
			{
				// Same Preset, just update the state
				// Continue to apply the state below
			}
			else
			{
				// Different Preset owns this item - this is handled by conflict check
				// If we're here with force=false, CheckPresetConflicts should have caught it
				// If force=true, take ownership from the other Preset
				if (bForce)
				{
					// Transfer ownership to this Preset, but keep the original state
					OwnerIt->second.PresetGuid = pPreset->GetGuid();
				}
				else
				{
					// This shouldn't happen if CheckPresetConflicts was called
					theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Unexpected ownership conflict for item %s", ItemGuid.ToWString().c_str());
					continue;
				}
			}
		}
		else // Item not owned yet - save original state and take ownership
		{
			SItemOwnership Ownership;
			Ownership.PresetGuid = pPreset->GetGuid();

			Ownership.Type = ItemPreset.Type;
			Ownership.bWasEnabled = false;

			switch (ItemPreset.Type)
			{
			case EItemType::eExecRule: // Program/Execution Rule
				if (CProgramRulePtr pRule = theCore->ProgramManager()->GetRule(ItemGuid))
					Ownership.bWasEnabled = pRule->IsEnabled();
				break;

			case EItemType::eResRule: // Resource/Access Rule
				if (CAccessRulePtr pRule = theCore->AccessManager()->GetRule(ItemGuid))
					Ownership.bWasEnabled = pRule->IsEnabled();
				break;

			case EItemType::eFwRule: // Firewall Rule
				if (CFirewallRulePtr pRule = theCore->NetworkManager()->Firewall()->GetRule(ItemGuid))
					Ownership.bWasEnabled = pRule->IsEnabled();
				break;

			case EItemType::eTweak: // System Tweak
				if (CTweakPtr pTweak = theCore->TweakManager()->GetTweakById(ItemGuid.ToWString())) {
					auto Status = pTweak->GetStatus(CallerPID);
					Ownership.bWasEnabled = Status == ETweakStatus::eSet;
				}
				break;
			}

			m_ItemOwnership[ItemGuid] = Ownership;
		}

		switch (ItemPreset.Type)
		{
		case EItemType::eExecRule: // Program/Execution Rule
			if (CProgramRulePtr pRule = theCore->ProgramManager()->GetRule(ItemGuid)) {
				switch (ItemPreset.Activate) {
				case SItemPreset::EActivate::eEnable: pRule->SetEnabled(true); break;
				case SItemPreset::EActivate::eDisable: pRule->SetEnabled(false); break;
				}
				auto Result = theCore->ProgramManager()->AddRule(pRule);
				if (Result.IsError())
					theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to apply program rule %s", ItemGuid.ToWString().c_str());
			}
			break;

		case EItemType::eResRule: // Resource/Access Rule
			if (CAccessRulePtr pRule = theCore->AccessManager()->GetRule(ItemGuid)) {
				switch (ItemPreset.Activate) {
				case SItemPreset::EActivate::eEnable: pRule->SetEnabled(true); break;
				case SItemPreset::EActivate::eDisable: pRule->SetEnabled(false); break;
				}
				auto Result = theCore->AccessManager()->AddRule(pRule);
				if (Result.IsError())
					theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to apply access rule %s", ItemGuid.ToWString().c_str());
			}
			break;

		case EItemType::eFwRule: // Firewall Rule
			if (CFirewallRulePtr pRule = theCore->NetworkManager()->Firewall()->GetRule(ItemGuid)) {
				switch (ItemPreset.Activate) {
				case SItemPreset::EActivate::eEnable: pRule->SetEnabled(true); break;
				case SItemPreset::EActivate::eDisable: pRule->SetEnabled(false); break;
				}
				STATUS Status = theCore->NetworkManager()->Firewall()->SetRule(pRule);
				if (Status.IsError())
					theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to apply firewall rule %s", ItemGuid.ToWString().c_str());

				if (pRule->IsTemplate())
				{
					auto DerivedRules = theCore->NetworkManager()->Firewall()->GetRulesFromTemplate(ItemGuid);
					for (auto& pDerivedRule : DerivedRules)
					{
						switch (ItemPreset.Activate) {
						case SItemPreset::EActivate::eEnable: pDerivedRule->SetEnabled(true); break;
						case SItemPreset::EActivate::eDisable: pDerivedRule->SetEnabled(false); break;
						}
						STATUS DerivedStatus = theCore->NetworkManager()->Firewall()->SetRule(pDerivedRule);
						if (DerivedStatus.IsError())
							theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to apply firewall rule %s (derived from template %s)", pDerivedRule->GetGuidStr().c_str(), ItemGuid.ToWString().c_str());
					}
				}
			}
			break;

		case EItemType::eTweak: // System Tweak
			STATUS Status;
			switch (ItemPreset.Activate) {
			case SItemPreset::EActivate::eEnable: Status = theCore->TweakManager()->ApplyTweak(ItemGuid.ToWString(), CallerPID); break;
			case SItemPreset::EActivate::eDisable: Status = theCore->TweakManager()->UndoTweak(ItemGuid.ToWString(), CallerPID); break;
			}
			if (Status.IsError())
				theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to apply tweak %s", ItemGuid.ToWString().c_str());
			break;
		}
	}

	return OK;
}

STATUS CPresetManager::DeactivateAllPresets(uint32 CallerPID)
{
	std::unique_lock Lock(m_Mutex);

	for (const auto& ActivePreset : m_ActivePresets)
	{
		STATUS Status = DeactivatePresetInternal(ActivePreset, CallerPID);
		if (Status.IsError())
			theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to deactivate Preset %s", ActivePreset.ToWString().c_str());

		auto F = m_Presets.find(ActivePreset);
		CPresetPtr pPreset = F != m_Presets.end() ? F->second : CPresetPtr();

		Lock.unlock();

		if (pPreset && pPreset->HasScript())
		{
			auto pScript = pPreset->GetScriptEngine();
			if (pScript)
				pScript->RunDeactivationScript(CallerPID);
		}

		Lock.lock();
	}
	m_ActivePresets.clear();

	Lock.unlock();

	Store();

	EmitChangeEvent(CFlexGuid(), EConfigEvent::eAllChanged);
	return OK;
}

STATUS CPresetManager::DeactivatePreset(const CFlexGuid& Preset, uint32 CallerPID)
{
	if (Preset.IsNull())
		return DeactivateAllPresets(CallerPID);

	std::unique_lock Lock(m_Mutex);

	if (m_ActivePresets.find(Preset) == m_ActivePresets.end())
		return ERR(STATUS_NOT_COMMITTED);

	STATUS Status = DeactivatePresetInternal(Preset, CallerPID);
	if (Status.IsError())
		return Status;

	m_ActivePresets.erase(Preset);

	auto F = m_Presets.find(Preset);
	CPresetPtr pPreset = F != m_Presets.end() ? F->second : CPresetPtr();

	Lock.unlock();

	if (pPreset && pPreset->HasScript())
	{
		auto pScript = pPreset->GetScriptEngine();
		if (pScript)
			pScript->RunDeactivationScript(CallerPID);
	}

	Store();

	EmitChangeEvent(Preset, EConfigEvent::eModified);
	return OK;
}

STATUS CPresetManager::ActivatePreset(const CFlexGuid& Preset, uint32 CallerPID, bool bForce)
{
	if (Preset.IsNull())
		return DeactivateAllPresets(CallerPID);

	std::unique_lock Lock(m_Mutex);

	auto F = m_Presets.find(Preset);
	if (F == m_Presets.end())
		return ERR(STATUS_NOT_FOUND);

	CPresetPtr pPreset = F->second;

	if (!bForce && m_ActivePresets.find(Preset) == m_ActivePresets.end()) // not forced and not already active
	{
		std::vector<std::pair<CFlexGuid, CFlexGuid>> Conflicts = CheckPresetConflicts(pPreset);
		if (!Conflicts.empty())
		{
			theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Preset activation conflicts with %d item(s) managed by other Presets", (int)Conflicts.size());
			return ERR(STATUS_OBJECT_NAME_COLLISION);
		}
	}

	STATUS Status = ApplyPresetItems(pPreset, CallerPID, bForce);
	if (Status.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to apply Preset items");
		return Status;
	}

	m_ActivePresets.insert(Preset);

	Lock.unlock();

	if (pPreset->HasScript())
	{
		auto pScript = pPreset->GetScriptEngine();
		if (pScript)
			pScript->RunActivationScript(CallerPID);
	}

	Store();

	EmitChangeEvent(Preset, EConfigEvent::eModified);
	return OK;
}
