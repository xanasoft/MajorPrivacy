#pragma once
#include "../Library/Common/StVariant.h"
#include "../Library/API/PrivacyDefs.h"
#include "../Library/Common/FlexGuid.h"
#include "JSEngine/JSEngine.h"

class CEnclave
{
	TRACK_OBJECT(CEnclave)
public:
	CEnclave();
	~CEnclave();

	bool IsEnabled() const					{ std::shared_lock Lock(m_Mutex); return m_bEnabled; }
	void SetEnabled(bool bEnabled)			{ std::unique_lock Lock(m_Mutex); m_bEnabled = bEnabled; }

	EExecDllMode GetDllMode() const			{ std::shared_lock Lock(m_Mutex); return m_DllMode; }

	//bool IsLockdown() const					{ std::shared_lock Lock(m_Mutex); return m_bLockdown; }
	//void SetLockdown(bool bLockdown)		{ std::unique_lock Lock(m_Mutex); m_bLockdown = bLockdown; }

	void SetStrGuid(const std::wstring& Guid) { std::unique_lock Lock(m_Mutex); m_Guid = CFlexGuid(Guid); }
	CFlexGuid GetGuid() const				{ std::shared_lock Lock(m_Mutex); return m_Guid; }

	void SetName(const std::wstring& Name)	{ std::unique_lock Lock(m_Mutex); m_Name = Name; }
	std::wstring GetName() const			{ std::shared_lock Lock(m_Mutex); return m_Name; }
	//std::wstring GetGrouping() const		{ std::shared_lock Lock(m_Mutex); return m_Grouping; }
	std::wstring GetDescription() const		{ std::shared_lock Lock(m_Mutex); return m_Description; }

	void SetVolumeGuid(const CFlexGuid& Guid) { std::unique_lock Lock(m_Mutex); m_VolumeGuid = Guid; }
	CFlexGuid GetVolumeGuid() const			{ std::shared_lock Lock(m_Mutex); return m_VolumeGuid; }

	void Update(const std::shared_ptr<CEnclave>& Enclave);

	StVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	virtual NTSTATUS FromVariant(const StVariant& Rule);

	bool HasScript() const;
	CJSEnginePtr GetScriptEngine() const;

protected:

	virtual void WriteIVariant(StVariantWriter& Enclave, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(StVariantWriter& Enclave, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const StVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	void UpdateScript_NoLock();

	mutable std::shared_mutex m_Mutex;

	CFlexGuid				m_Guid;
	bool					m_bEnabled = true;

	//bool					m_bLockdown = true;

	std::wstring			m_Name;
	//std::wstring			m_Grouping;
	std::wstring			m_Description;

	CFlexGuid				m_VolumeGuid;

	bool					m_bUseScript = false;
	std::string				m_Script;

	EExecDllMode			m_DllMode = EExecDllMode::eDisabled;

	USignatures				m_AllowedSignatures = { 0 };
	std::list<std::wstring>	m_AllowedCollections;
	bool					m_ImageLoadProtection = true;
	bool					m_ImageCoherencyChecking = true;

	EProgramOnSpawn			m_OnTrustedSpawn = EProgramOnSpawn::eAllow;
	EProgramOnSpawn			m_OnSpawn = EProgramOnSpawn::eEject;

	EIntegrityLevel			m_IntegrityLevel = EIntegrityLevel::eUnknown;

	bool					m_AllowDebugging = false;
	bool					m_KeepAlive = false;

	StVariant				m_Data;

	CJSEnginePtr			m_pScript;
};

typedef std::shared_ptr<CEnclave> CEnclavePtr;