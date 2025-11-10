#include "pch.h"
#include "Preset.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Programs/ProgramID.h"

CPreset::CPreset()
{

}

StVariant CPreset::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	std::shared_lock Lock(m_Mutex);

	StVariantWriter Preset(pMemPool);
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Preset.BeginIndex();
		WriteIVariant(Preset, Opts);
	} else {  
		Preset.BeginMap();
		WriteMVariant(Preset, Opts);
	}
	return Preset.Finish();
}

NTSTATUS CPreset::FromVariant(const class StVariant& Data)
{
	std::unique_lock Lock(m_Mutex);

	if (Data.GetType() == VAR_TYPE_MAP)		StVariantReader(Data).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
	else if (Data.GetType() == VAR_TYPE_INDEX)	StVariantReader(Data).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;
	UpdateScript_NoLock();
	return STATUS_SUCCESS;
}

void CPreset::UpdateScript_NoLock()
{
	m_pScript.reset();

	if (!m_Script.empty()) 
	{
		m_pScript = std::make_shared<CJSEngine>(m_Script, m_Guid, EItemType::eEnclave);
		m_pScript->RunScript();
	}
}

bool CPreset::HasScript() const
{
	std::shared_lock Lock(m_Mutex);

	return m_bUseScript && !!m_pScript.get();
}

CJSEnginePtr CPreset::GetScriptEngine() const 
{ 
	std::shared_lock Lock(m_Mutex); 

	return m_pScript; 
}

