#pragma once
#include "ProgramFile.h"

class CWindowsService: public CProgramItem
{
	Q_OBJECT
public:
	CWindowsService(QObject* parent = nullptr);

	virtual QIcon DefaultIcon() const;
	
	virtual QString GetNameEx() const;

	virtual QString GetSvcTag() const { return m_ServiceId; }

	virtual quint64 GetProcessId() const { return m_ProcessId; }

	virtual CProgramFilePtr GetProgramFile() const;

	virtual void CountStats();

	virtual QMap<QString, CTrafficEntryPtr>	GetTrafficLog();
	
protected:
	
	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	QString						m_ServiceId;

	quint64						m_ProcessId = 0;

	QSet<quint64>				m_SocketRefs;

	QMap<QString, CTrafficEntryPtr> m_TrafficLog;
	quint64						m_TrafficLogLastActivity = 0;
};

typedef QSharedPointer<CWindowsService> CWindowsServicePtr;