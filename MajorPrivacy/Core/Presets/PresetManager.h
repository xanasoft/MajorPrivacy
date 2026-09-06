#pragma once
#include "Preset.h"
#include "../Library/Status.h"

struct SItemOwnerInfo
{
	QFlexGuid PresetGuid;
	QString PresetName;
	EItemType Type = EItemType::eUnknown;
	bool bWasEnabled = false;
};

class CPresetManager : public QObject
{
	Q_OBJECT
public:
	CPresetManager(QObject* parent = nullptr);

	void Update();

	QMap<QFlexGuid, CPresetPtr> GetPresets() const { return m_Presets; }

	RESULT(QFlexGuid) SetPreset(const CPresetPtr& pPreset);
	CPresetPtr GetPreset(const QFlexGuid& Guid) const;
	//RESULT(CPresetPtr) GetPreset(const QFlexGuid& Guid);
	STATUS DelPreset(const QFlexGuid& Guid);

	STATUS ActivatePreset(const QFlexGuid& Preset, bool bForce = false);
	STATUS DeactivatePreset(const QFlexGuid& Preset);
	QSet<QFlexGuid> GetActivePresets() const { return m_ActivePresets; }
	bool IsPresetActive(const QFlexGuid& Preset) const { return m_ActivePresets.contains(Preset); }

	SItemOwnerInfo GetItemOwner(const QFlexGuid& ItemGuid) const;
	QMap<QFlexGuid, SItemOwnerInfo> GetItemOwnership() const { return m_ItemOwnership; }

signals:
	void PresetsChanged();

protected:
	void UpdateItemOwnership();

	QMap<QFlexGuid, CPresetPtr> m_Presets;
	QSet<QFlexGuid> m_ActivePresets;
	QMap<QFlexGuid, SItemOwnerInfo> m_ItemOwnership;
};