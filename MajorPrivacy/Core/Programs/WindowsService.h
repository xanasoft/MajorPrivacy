#pragma once
#include "ProgramFile.h"

class CWindowsService: public CProgramItem
{
	Q_OBJECT
public:
	CWindowsService(QObject* parent = nullptr);

	virtual EProgramType GetType() const { return EProgramType::eWindowsService; }

	virtual QIcon DefaultIcon() const;
	
	virtual QString GetNameEx() const;

	virtual QString GetSvcTag() const { return m_ServiceId; }

	virtual quint64 GetProcessId() const { return m_ProcessId; }

	virtual CProgramFilePtr GetProgramFile() const;

	virtual void CountStats();

	virtual QMap<quint64, SAccessStatsPtr>	GetAccessStats();

	virtual QMap<QString, CTrafficEntryPtr>	GetTrafficLog();
	
protected:
	
	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	QString						m_ServiceId;

	quint64						m_ProcessId = 0;

	QMap<quint64, SAccessStatsPtr> m_AccessStats;
	quint64						m_AccessStatsLastActivity = 0;

	QSet<quint64>				m_SocketRefs;

	QMap<QString, CTrafficEntryPtr> m_TrafficLog;
	quint64						m_TrafficLogLastActivity = 0;
};

typedef QSharedPointer<CWindowsService> CWindowsServicePtr;