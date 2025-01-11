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
	case eUnmounted:		return tr("Unmounted Volume");
	case eMounted:			return tr("Mounted Volume");
	case eFolder:			return tr("Unprotected Folder");
	case eSecFolder:		return tr("Protected Folder");
	}
	return tr("Unknown");
}

void CVolume::SetImagePath(const QString& ImagePath)
{
	m_ImagePath = ImagePath;
	if (ImagePath.endsWith("\\"))
		m_Status = eFolder;
	else
		m_Status = eUnmounted;
	if(m_Name.isEmpty()) 
		m_Name = m_Status == eFolder ? m_ImagePath : QFileInfo(m_ImagePath).fileName(); 
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
		case API_V_VOL_PATH:		m_ImagePath = Data.AsQStr(); break;
		case API_V_VOL_DEVICE_PATH:	m_DevicePath = Data.AsQStr(); break;
		case API_V_VOL_MOUNT_POINT:	m_MountPoint = Data.AsQStr(); break;
		case API_V_VOL_SIZE:		m_VolumeSize = Data; break;
		}
	});

	if(m_Name.isEmpty()) 
		m_Name = QFileInfo(m_ImagePath).fileName(); 
}