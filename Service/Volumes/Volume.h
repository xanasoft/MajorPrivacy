#pragma once
#include "../Library/Common/Variant.h"

class CVolume
{
	TRACK_OBJECT(CVolume)
public:
	CVolume();
	~CVolume();

	std::wstring ImageDosPath() const { std::shared_lock Lock(m_Mutex); return m_ImageDosPath; }
	std::wstring DevicePath() const { std::shared_lock Lock(m_Mutex); return m_DevicePath; }
	std::wstring MountPoint() const { std::shared_lock Lock(m_Mutex); return m_MountPoint; }
	uint64 VolumeSize() const { std::shared_lock Lock(m_Mutex); return m_VolumeSize; }

	CVariant ToVariant() const;

//protected:
	mutable std::shared_mutex m_Mutex;

	std::wstring m_ImageDosPath;
	std::wstring m_DevicePath;
	std::wstring m_MountPoint;
	uint64 m_VolumeSize = 0;
	HANDLE m_ProcessHandle = NULL;
	bool m_bProtected = false;
	bool m_bDataDirty = false;

	CVariant m_Data;
};

typedef std::shared_ptr<CVolume> CVolumePtr;