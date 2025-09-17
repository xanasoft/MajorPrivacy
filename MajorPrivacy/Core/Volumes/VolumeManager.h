#pragma once
#include "Volume.h"
#include "../Library/Status.h"

class CVolumeManager : public QObject
{
	Q_OBJECT
public:
	CVolumeManager(QObject* parent);
	~CVolumeManager();

	void Update();

	QList<CVolumePtr> List() { return m_List.values(); }
	
	STATUS AddVolume(const QString& Path);
	STATUS RemoveVolume(const QString& Path);

	STATUS SetVolume(const CVolumePtr& pVolume);
	CVolumePtr GetVolume(const QFlexGuid& Guid);

	bool LoadVolumes();
	bool SaveVolumes();

public slots:
	void UpdateProtectedFolders();

protected:
	QMap<QString, CVolumePtr> m_List;
};