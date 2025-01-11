#pragma once
#include "../Library/Status.h"
#include "Volume.h"
#include "Access/AccessRule.h"

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

	STATUS MountImage(const std::wstring& Path, const std::wstring& MountPoint, const std::wstring& Password, bool bProtect);
	STATUS DismountVolume(const std::wstring& DevicePath);
	STATUS DismountAll();
	
	//STATUS SetVolumeData(const std::wstring& Path, const std::wstring& Password, const CBuffer& Buffer);
	//STATUS GetVolumeData(const std::wstring& Path, const std::wstring& Password, CBuffer& Buffer);

	RESULT(std::vector<std::wstring>) GetVolumeList();
	RESULT(std::vector<CVolumePtr>) GetAllVolumes();
	RESULT(CVolumePtr) GetVolumeInfo(const std::wstring& DevicePath);

protected:
	friend class CAccessManager;

	STATUS DismountVolume(const std::shared_ptr<CVolume>& pMount);

	STATUS ProtectVolume(const std::wstring& Path, const std::wstring& DevicePath);
	STATUS UnProtectVolume(const std::wstring& DevicePath);
	STATUS TryAddRule(const CAccessRulePtr& pRule, const std::wstring& Path, const std::wstring& DevicePath);

	void UpdateRule(const CAccessRulePtr& pRule, enum class EConfigEvent Event, uint64 PID);

	STATUS LoadVolumeRules(const std::shared_ptr<CVolume>& pMount);
	STATUS SaveVolumeRules(const std::shared_ptr<CVolume>& pMount);

	std::mutex m_Mutex;
	std::map<std::wstring, CVolumePtr> m_Volumes; // by DevicePath
	std::map<std::wstring, CVolumePtr> m_VolumesByPath;
};