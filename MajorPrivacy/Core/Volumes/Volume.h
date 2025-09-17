#pragma once
#include "./Common/QtFlexGuid.h"
#include "../Library/API/PrivacyDefs.h"

class CVolume : public QObject
{
	Q_OBJECT
public:
	CVolume(QObject* parent = NULL);


	QFlexGuid GetGuid() const						{ return m_Guid; }
	void SetGuid(const QFlexGuid& Guid)				{ m_Guid = Guid; }

	quint64 GetVolumeRef() const					{ return m_VolumeRef; }

	QString GetName() const							{ return m_Name; }
	void SetName(const QString& Name)				{ m_Name = Name; }
	QString GetImagePath() const					{ return m_ImagePath; }
	void SetImagePath(const QString& ImagePath);
	QString GetDevicePath() const					{ return m_DevicePath; }
	void SetDevicePath(const QString& DevicePath)	{ m_DevicePath = DevicePath; }
	QString GetMountPoint() const					{ return m_MountPoint; }
	void SetMountPoint(const QString& MountPoint)	{ m_MountPoint = MountPoint; }
	uint64 GetVolumeSize() const					{ return m_VolumeSize; }
	void SetVolumeSize(uint64 VolumeSize)			{ m_VolumeSize = VolumeSize; }

	enum EStatus
	{
		eUnmounted,
		eMounted,
		eFolder,
		eSecFolder,
	};
	EStatus GetStatus() const						{ return m_Status; }
	QString GetStatusStr() const;
	void SetStatus(EStatus Status)					{ m_Status = Status; }
	bool IsFolder() const							{ return m_Status == eFolder || m_Status == eSecFolder; }
	bool IsMounted() const							{ return m_Status == eMounted; }
	void SetMounted(bool Mounted);

	QtVariant ToVariant(const SVarWriteOpt& Opts) const;
	void FromVariant(const class QtVariant& Volume);

protected:
	friend class CVolumeConfigWnd;


	QFlexGuid					m_Guid;
	quint64						m_VolumeRef = 0;

	QString						m_Name;
	QString						m_ImagePath;
	QString						m_DevicePath;
	QString						m_MountPoint;
	uint64						m_VolumeSize = 0;

	bool						m_bUseScript = false;
	QString						m_Script;

	EStatus						m_Status = eUnmounted;
};

typedef QSharedPointer<CVolume> CVolumePtr;