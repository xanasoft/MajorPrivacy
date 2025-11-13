#include "pch.h"
#include "PresetManager.h"
#include "../PrivacyCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../MajorPrivacy.h"

CPresetManager::CPresetManager(QObject* parent)
	: QObject(parent)
{
}

void CPresetManager::Update()
{
	auto Ret = theCore->GetAllPresets();
	if (Ret.IsError())
		return;

	QtVariant& Presets = Ret.GetValue();

	QMap<QFlexGuid, CPresetPtr> OldPresets = m_Presets;
	m_ActivePresets.clear();

	for (int i = 0; i < Presets.Count(); i++)
	{
		const QtVariant& Preset = Presets[i];

		QFlexGuid Guid;
		Guid.FromVariant(Preset[API_V_GUID]);

		CPresetPtr pPreset = OldPresets.value(Guid);
		if (pPreset)
			OldPresets.remove(Guid);

		bool bAdd = false;
		if (!pPreset) {
			pPreset = CPresetPtr(new CPreset());
			bAdd = true;
		}

		pPreset->FromVariant(Preset);

		if (pPreset->IsActive())
			m_ActivePresets.insert(pPreset->GetGuid());

		if(bAdd)
			m_Presets.insert(pPreset->GetGuid(), pPreset);
	}

	foreach(const QFlexGuid& Guid, OldPresets.keys())
		m_Presets.remove(Guid);
}

RESULT(QFlexGuid) CPresetManager::SetPreset(const CPresetPtr& pPreset)
{
	SVarWriteOpt Opts;
	//Opts.Flags = SVarWriteOpt::eTextGuids;

	STATUS Status = theCore->SetPreset(pPreset->ToVariant(Opts));
	if (Status) {
		Update();
		emit PresetsChanged();
	}
	return Status;
}

CPresetPtr CPresetManager::GetPreset(const QFlexGuid& Guid) const
{
	return m_Presets.value(Guid);
}

//RESULT(CPresetPtr) CPresetManager::GetPreset(const QFlexGuid& Guid)
//{
//	auto Ret = theCore->GetPreset(Guid);
//	if (Ret.IsError())
//		return ERR(Ret.GetStatus());
//	CPresetPtr pPreset(new CPreset());
//	pPreset->FromVariant(Ret.GetValue());
//	RETURN(pPreset);
//}

STATUS CPresetManager::DelPreset(const QFlexGuid& Guid)
{
	STATUS Status = theCore->DelPreset(Guid);
	if (Status) {
		m_Presets.remove(Guid);
		emit PresetsChanged();
	}
	return Status;
}

STATUS CPresetManager::ActivatePreset(const QFlexGuid& Preset, bool bForce)
{
	STATUS Status = theCore->ActivatePreset(Preset, bForce);
	if (Status)
		Update();
	emit PresetsChanged();
	return Status;
}

STATUS CPresetManager::DeactivatePreset(const QFlexGuid& Preset)
{
	STATUS Status = theCore->DeactivatePreset(Preset);
	if (Status)
		Update();
	emit PresetsChanged();
	return Status;
}
