#include "pch.h"
#include "Volume.h"
#include "../Library/API/PrivacyAPI.h"

CVolume::CVolume()
{
}

CVolume::~CVolume()
{
}

CVariant CVolume::ToVariant() const
{
	std::shared_lock Lock(m_Mutex);

	CVariant Volume;

	Volume.BeginIMap();
	Volume.Write(API_V_VOL_REF, (uint64)this);
	Volume.Write(API_V_VOL_PATH, m_ImageDosPath);
	Volume.Write(API_V_VOL_DEVICE_PATH, m_DevicePath);
	Volume.Write(API_V_VOL_MOUNT_POINT, m_MountPoint);
	Volume.Write(API_V_VOL_SIZE, m_VolumeSize);
	Volume.Finish();

	return Volume;
}