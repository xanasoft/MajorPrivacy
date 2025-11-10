#pragma once
#include "../../Library/Status.h"
#include "../../Library/Common/StVariant.h"
#include "../../Library/Common/FlexGuid.h"
#include "../../Library/API/PrivacyDefs.h"
#include "JSEngine/JSEngine.h"

///////////////////////////////////////////////////////////////////////////////////////
// CPreset

class CPreset
{
	TRACK_OBJECT(CPreset)
public:
	CPreset();

	void SetGuid(const CFlexGuid& Guid) { std::unique_lock Lock(m_Mutex); m_Guid = Guid; }
	CFlexGuid GetGuid() const { std::shared_lock Lock(m_Mutex); return m_Guid; }

	void SetName(const std::wstring& Name) { std::unique_lock Lock(m_Mutex); m_Name = Name; }
	std::wstring GetName() const { std::shared_lock Lock(m_Mutex); return m_Name; }

	void SetIsActive(bool bActive) { std::unique_lock Lock(m_Mutex); m_bIsActive = bActive; }
	bool IsActive() const { std::shared_lock Lock(m_Mutex); return m_bIsActive; }

	std::map<CFlexGuid, SItemPreset> GetItems() const { std::shared_lock Lock(m_Mutex); return m_Items; }

	virtual StVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const;
	virtual NTSTATUS FromVariant(const class StVariant& Preset);

	bool HasScript() const;
	CJSEnginePtr GetScriptEngine() const;

protected:
	mutable std::shared_mutex m_Mutex;

	virtual void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const StVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	void UpdateScript_NoLock();

	CFlexGuid m_Guid;
	std::wstring m_Name;
	std::wstring m_Description;

	bool m_bIsActive = false;
	
	bool m_bUseScript = false;
	std::string m_Script;

	std::map<CFlexGuid, SItemPreset> m_Items;

	StVariant m_Data;

	CJSEnginePtr m_pScript;
};

typedef std::shared_ptr<CPreset> CPresetPtr;
