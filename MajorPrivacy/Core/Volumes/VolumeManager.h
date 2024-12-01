#pragma once
#include "Volume.h"

class CVolumeManager : public QObject
{
	Q_OBJECT
public:
	CVolumeManager(QObject* parent);
	~CVolumeManager();

	void Update();

	QList<CVolumePtr> List() { return m_List.values(); }
	
	void AddVolume(const QString& Path);
	void RemoveVolume(const QString& Path);

	bool LoadVolumes();
	bool SaveVolumes();

protected:
	QMap<QString, CVolumePtr> m_List;
};