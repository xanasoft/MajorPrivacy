#include "pch.h"
#include "Volume.h"
#include "../Library/API/PrivacyAPI.h"

CVolume::CVolume(QObject* parent)
	: QObject(parent)
{
}

QString CVolume::GetStatusStr() const
{
	switch (m_Status) {
	case eMounted:			return tr("Mounted");
	case eUnmounted:		return tr("Unmounted");
	}
	return "Unknown";
}

void CVolume::SetImagePath(const QString& ImagePath)
{
	m_ImagePath = ImagePath;
	if(m_Name.isEmpty()) 
		m_Name = QFileInfo(m_ImagePath).fileName(); 
}

void CVolume::SetMounted(bool Mounted)
{
	m_Status = Mounted ? eMounted : eUnmounted;

	if (!Mounted) 
	{
		m_MountPoint.clear();
		m_DevicePath.clear();
		m_VolumeSize = 0;
	}
}

void CVolume::FromVariant(const class XVariant& Volume)
{
	Volume.ReadRawIMap([&](uint32 Index, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;

		switch (Index)
		{
		case API_V_VOL_REF:			m_VolumeRef = Data; break;
		case API_V_VOL_PATH:		SetImagePath(Data.AsQStr()); break;
		case API_V_VOL_DEVICE_PATH:	m_DevicePath = Data.AsQStr(); break;
		case API_V_VOL_MOUNT_POINT:	m_MountPoint = Data.AsQStr(); break;
		case API_V_VOL_SIZE:		m_VolumeSize = Data; break;
		}
	});
}