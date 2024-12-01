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

	QMap<quint64, CProgramFile::SExecutionInfo> GetExecStats();

	QMap<quint64, CProgramFile::SIngressInfo> GetIngressStats();

	virtual QMap<quint64, SAccessStatsPtr>	GetAccessStats();

	quint64 GetAccessLastActivity() const { return m_AccessLastActivity; }
	quint64 GetAccessLastEvent() const { return m_AccessLastEvent; }
	quint32 GetAccessCount() const { return m_AccessCount; }

	virtual QMap<QString, CTrafficEntryPtr>	GetTrafficLog();

	virtual void ClearLogs(ETraceLogs Log);

	virtual void ClearAccessLog();
	virtual void ClearProcessLogs();
	virtual void ClearTrafficLog();
	
protected:
	friend class CProgramFile;
	
	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	QString						m_ServiceId;

	quint64						m_LastExec = 0;

	quint64						m_ProcessId = 0;

	QMap<quint64, CProgramFile::SExecutionInfo> m_ExecStats;
	bool						m_ExecChanged = true;

	QMap<quint64, CProgramFile::SIngressInfo> m_Ingress;
	bool						m_IngressChanged = true;

	QMap<quint64, SAccessStatsPtr> m_AccessStats;
	quint64						m_AccessLastActivity = 0;
	quint64						m_AccessLastEvent = 0;
	quint32						m_AccessCount = 0;

	QSet<quint64>				m_SocketRefs;

	QMap<QString, CTrafficEntryPtr> m_TrafficLog;
	quint64						m_TrafficLogLastActivity = 0;
};

typedef QSharedPointer<CWindowsService> CWindowsServicePtr;