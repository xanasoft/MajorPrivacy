#pragma once
#include "../Library/Status.h"
#include "Volume.h"
#include "Access/AccessRule.h"
#include "Programs/ProgramRule.h"
#include "Enclaves/Enclave.h"
#include "HashDB/HashEntry.h"

class CVolumeManager
{
	TRACK_OBJECT(CVolumeManager)
public:
	CVolumeManager();
	virtual ~CVolumeManager();

	STATUS Init();

	STATUS Update();

	STATUS CreateImage(const std::wstring& Path, const std::wstring& Password, uint64 uSize = 0, const std::wstring& Cipher = L"AES");
	STATUS ChangeImagePassword(const std::wstring& Path, const std::wstring& OldPassword, const std::wstring& NewPassword);
	//STATUS DeleteImage(const std::wstring& Path);

	STATUS MountImage(const std::wstring& Path, const std::wstring& MountPoint, const std::wstring& Password, bool bProtect, bool bLockdown);
	STATUS DismountVolume(const std::wstring& DevicePath);
	STATUS DismountAll();
	
	//STATUS SetVolumeData(const std::wstring& Path, const std::wstring& Password, const CBuffer& Buffer);
	//STATUS GetVolumeData(const std::wstring& Path, const std::wstring& Password, CBuffer& Buffer);

	RESULT(std::vector<std::wstring>) GetVolumeList();
	RESULT(std::vector<CVolumePtr>) GetAllVolumes();
	RESULT(CVolumePtr) GetVolumeInfo(const std::wstring& DevicePath);

	CVolumePtr GetVolume(const CFlexGuid& Guid);
	STATUS SetVolume(const CVolumePtr& pVolume);

	STATUS SetVolumeLockdown(const std::wstring& DevicePath, const CFlexGuid& Guid, uint64 Token);

protected:
	friend class CAccessManager;
	friend class CProgramManager;
	friend class CEnclaveManager;
	friend class CHashDB;

	STATUS DismountVolume(const std::shared_ptr<CVolume>& pMount);

	void CleanUpVolume(const std::shared_ptr<CVolume>& pMount);
	STATUS TryAddRule(const CAccessRulePtr& pRule, const std::wstring& Path, const std::wstring& DevicePath);

	void UpdateAccessRule(const CAccessRulePtr& pRule, enum class EConfigEvent Event, uint64 PID);
	void UpdateProgramRule(const CProgramRulePtr& pRule, enum class EConfigEvent Event, uint64 PID);
	void UpdateEnclave(const CEnclavePtr& pEnclave, enum class EConfigEvent Event, uint64 PID);
	void UpdateHashEntry(const CHashPtr& pEntry, enum class EConfigEvent Event, uint64 PID);

	STATUS LoadVolumeData(const std::shared_ptr<CVolume>& pMount, bool bFull);
	STATUS SaveVolumeData(const std::shared_ptr<CVolume>& pMount);

	std::mutex m_Mutex;
	std::map<std::wstring, CVolumePtr> m_Volumes; // by DevicePath
	std::map<CFlexGuid, CVolumePtr> m_VolumesByGuid;
	bool m_NoHibernation = false;
};