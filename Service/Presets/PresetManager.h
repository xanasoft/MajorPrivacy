#pragma once
#include "Preset.h"

class CPresetManager
{
	TRACK_OBJECT(CPresetManager)
public:
	CPresetManager();

	STATUS Init();

	STATUS Load();
	STATUS Store();

	STATUS LoadEntries(const StVariant& Entries);
	StVariant SaveEntries(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr);

	CPresetPtr GetPreset(const CFlexGuid& Guid) const;

	STATUS SetPresets(const StVariant& Presets);
	RESULT(StVariant) SetEntry(const StVariant& Entry);
	RESULT(StVariant) GetEntry(const std::wstring& EntryId, const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr);
	STATUS DelEntry(const std::wstring& EntryId);

	STATUS ActivatePreset(const CFlexGuid& Preset, uint32 CallerPID, bool bForce = false);
	STATUS DeactivatePreset(const CFlexGuid& Preset, uint32 CallerPID);
	STATUS DeactivateAllPresets(uint32 CallerPID);
	std::set<CFlexGuid> GetActivePresets() const { std::shared_lock Lock(m_Mutex); return m_ActivePresets; }

protected:
	void EmitChangeEvent(const CFlexGuid& Guid, enum class EConfigEvent Event);

	STATUS DeactivatePresetInternal(const CFlexGuid& Preset, uint32 CallerPID);
	STATUS ApplyPresetItems(const CPresetPtr& pPreset, uint32 CallerPID, bool bForce = false);
	std::vector<std::pair<CFlexGuid, CFlexGuid>> CheckPresetConflicts(const CPresetPtr& pPreset);

	struct SItemOwnership
	{
		CFlexGuid PresetGuid;

		EItemType Type = EItemType::eUnknown;
		bool bWasEnabled = false;
	};

	mutable std::shared_mutex m_Mutex;
	std::map<CFlexGuid, CPresetPtr> m_Presets;
	std::set<CFlexGuid> m_ActivePresets;

	std::map<CFlexGuid, SItemOwnership> m_ItemOwnership;
};