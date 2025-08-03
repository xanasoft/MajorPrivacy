#include "pch.h"
#include "Volume.h"
#include "../Library/API/PrivacyAPI.h"

CVolume::CVolume()
{
}

CVolume::~CVolume()
{
}

StVariant CVolume::ToVariant(FW::AbstractMemPool* pMemPool) const
{
	std::shared_lock Lock(m_Mutex);

	StVariantWriter Volume(pMemPool);
	Volume.BeginIndex();

	Volume.Write(API_V_VOL_REF, (uint64)this);
	Volume.WriteEx(API_V_VOL_PATH, m_ImageDosPath);
	Volume.WriteEx(API_V_VOL_DEVICE_PATH, m_DevicePath);
	Volume.WriteEx(API_V_VOL_MOUNT_POINT, m_MountPoint);
	Volume.Write(API_V_VOL_SIZE, m_VolumeSize);

	return Volume.Finish();
}