#pragma once
#include "../../Library/Common/StVariant.h"
#include "../../Library/Common/FlexGuid.h"
#include "../../Library/API/PrivacyDefs.h"
#include "JSEngine/JSEngine.h"

class CVolume
{
	TRACK_OBJECT(CVolume)
public:
	CVolume();
	~CVolume();

	void Update(const std::shared_ptr<CVolume>& pVolume);

	void SetGuid(const CFlexGuid& Guid) { std::unique_lock Lock(m_Mutex); m_Guid = Guid; }
	CFlexGuid GetGuid() const { std::shared_lock Lock(m_Mutex); return m_Guid; }

	static CFlexGuid GetGuidFromPath(std::wstring Path);

	void SetImageDosPath(const std::wstring& ImageDosPath);
	std::wstring GetImageDosPath() const { std::shared_lock Lock(m_Mutex); return m_ImageDosPath; }

	void SetDevicePath(const std::wstring& DevicePath) { std::unique_lock Lock(m_Mutex); m_DevicePath = DevicePath; }
	bool HasDevicePath() const { std::shared_lock Lock(m_Mutex); return !m_DevicePath.empty(); }
	std::wstring GetDevicePath() const { std::shared_lock Lock(m_Mutex); return m_DevicePath; }

	void SetMountPoint(const std::wstring& MountPoint) { std::unique_lock Lock(m_Mutex); m_MountPoint = MountPoint; }
	std::wstring GetMountPoint() const { std::shared_lock Lock(m_Mutex); return m_MountPoint; }

	void SetVolumeSize(uint64 VolumeSize) { std::unique_lock Lock(m_Mutex); m_VolumeSize = VolumeSize; }
	uint64 GetVolumeSize() const { std::shared_lock Lock(m_Mutex); return m_VolumeSize; }

	void SetProcessHandle(HANDLE hProcess) { std::unique_lock Lock(m_Mutex); m_ProcessHandle = hProcess; }
	HANDLE GetProcessHandle() const { std::shared_lock Lock(m_Mutex); return m_ProcessHandle; }

	void SetProtected(bool bProtected) { std::unique_lock Lock(m_Mutex); m_bProtected = bProtected; }
	bool IsProtected() const { std::shared_lock Lock(m_Mutex); return m_bProtected; }

	void SetLockdownToken(uint64 Token) { std::unique_lock Lock(m_Mutex); m_LockdownToken = Token; }
	uint64 GetLockdownToken() const { std::shared_lock Lock(m_Mutex); return m_LockdownToken; }

	void SetUseScript(bool bUse) { std::unique_lock Lock(m_Mutex); m_bUseScript = bUse; }
	bool IsUseScript() const { std::shared_lock Lock(m_Mutex); return m_bUseScript; }
	void SetScript(const std::string& Script);
	std::string GetScript() const { std::shared_lock Lock(m_Mutex); return m_Script; }

	bool HasScript() const;
	CJSEnginePtr GetScriptEngine() const;

	void SetDataDirty(bool bDirty) { std::unique_lock Lock(m_Mutex); m_bDataDirty = bDirty; }
	bool HasDirtyData() const { std::shared_lock Lock(m_Mutex); return m_bDataDirty; }

	void SetData(StVariant Data) { std::unique_lock Lock(m_Mutex); m_Data = Data; }
	StVariant GetData() const { std::shared_lock Lock(m_Mutex); return m_Data; }

	StVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	NTSTATUS FromVariant(const StVariant& Rule);

protected:

	void UpdateScript_NoLock();

	mutable std::shared_mutex m_Mutex;

	CFlexGuid m_Guid;

	std::wstring m_ImageDosPath;
	std::wstring m_DevicePath;
	std::wstring m_MountPoint;
	uint64 m_VolumeSize = 0;
	HANDLE m_ProcessHandle = NULL;
	bool m_bProtected = false;
	uint64 m_LockdownToken = 0;
	bool m_bUseScript = false;
	std::string m_Script;
	bool m_bDataDirty = false;

	StVariant m_Data;

	CJSEnginePtr m_pScript;
};

typedef std::shared_ptr<CVolume> CVolumePtr;