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

	QString NormImagePath = ImagePath.toLower();

	QByteArray Hash = QCryptographicHash::hash(QByteArrayView((char*)NormImagePath.utf16(), NormImagePath.length()*sizeof(char16_t)), QCryptographicHash::Sha256);
	Q_ASSERT(Hash.size() == sizeof(SGuid) * 2);

	SGuid Guid;
	((__int64*)&Guid)[0] = ((__int64*)Hash.constData())[0] ^ ((__int64*)Hash.constData())[2];
	((__int64*)&Guid)[1] = ((__int64*)Hash.constData())[1] ^ ((__int64*)Hash.constData())[3];
	m_Guid.SetRegularGuid(&Guid);
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


QtVariant CVolume::ToVariant(const SVarWriteOpt& Opts) const
{
	QtVariantWriter Volume;
	Volume.BeginIndex();

	Volume.WriteVariant(API_V_GUID, m_Guid.ToVariant(false));
	Volume.Write(API_V_VOL_REF, m_VolumeRef);
	//Volume.WriteEx(API_V_VOL_PATH, m_ImagePath);
	//Volume.WriteEx(API_V_VOL_DEVICE_PATH, m_DevicePath);
	//Volume.WriteEx(API_V_VOL_MOUNT_POINT, m_MountPoint);
	//Volume.Write(API_V_VOL_SIZE, m_VolumeSize);
	Volume.Write(API_V_USE_SCRIPT, m_bUseScript);
	Volume.WriteEx(API_V_SCRIPT, m_Script);

	return Volume.Finish();

}

void CVolume::FromVariant(const class QtVariant& Volume)
{
	QtVariantReader(Volume).ReadRawIndex([&](uint32 Index, const FW::CVariant& vData) {
		const QtVariant& Data = *(QtVariant*)&vData;

		switch (Index)
		{
		case API_V_GUID:			m_Guid.FromVariant(Data); break;
		case API_V_VOL_REF:			m_VolumeRef = Data; break;
		case API_V_VOL_PATH:		m_ImagePath = Data.AsQStr(); break;
		case API_V_VOL_DEVICE_PATH:	m_DevicePath = Data.AsQStr(); break;
		case API_V_VOL_MOUNT_POINT:	m_MountPoint = Data.AsQStr(); break;
		case API_V_VOL_SIZE:		m_VolumeSize = Data; break;
		case API_V_USE_SCRIPT:		m_bUseScript = Data; break;
		case API_V_SCRIPT:			m_Script = Data.AsQStr(); break;
		}
	});

	if(m_Name.isEmpty()) 
		m_Name = QFileInfo(m_ImagePath).fileName(); 
}