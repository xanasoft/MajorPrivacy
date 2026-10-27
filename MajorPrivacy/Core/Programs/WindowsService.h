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

	virtual QString GetServiceTag() const { QReadLocker Lock(&m_Mutex); return m_ServiceTag; }

	virtual quint64 GetProcessId() const { return m_ProcessId; }

	virtual QString GetPath() const;

	virtual CProgramFilePtr GetProgramFile() const;

	virtual void CountStats();

	QMap<quint64, CProgramFile::SExecutionInfo> GetExecStats();
	quint32 GetExecRevision() const { QReadLocker Lock(&m_Mutex); return m_ExecRevision; }

	QMap<quint64, CProgramFile::SIngressInfo> GetIngressStats();
	quint32 GetIngressRevision() const { QReadLocker Lock(&m_Mutex); return m_IngressRevision; }

	virtual QMap<quint64, SAccessStatsPtr>	GetAccessStats(); // keyed by change sequence, see SAccessLog

	quint64 GetAccessLastActivity() const { return m_AccessLastActivity; }
	quint64 GetAccessLastEvent() const { return m_AccessLastEvent; }
	quint32 GetAccessCount() const { return m_AccessCount; }
	quint32 GetHandleCount() const { return m_HandleCount; }

	virtual QHash<QString, CTrafficEntryPtr> GetTrafficLog(QList<CTrafficEntryPtr>* pChanged = nullptr);
	quint32 GetTrafficLogGeneration() const { return m_TrafficLogGeneration; }

	virtual void ClearAccessLog();
	virtual void ClearProcessLogs();
	virtual void ClearTrafficLog();
	
protected:
	friend class CProgramFile;
	
	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	QString						m_ServiceTag;

	quint64						m_LastExec = 0;

	quint64						m_ProcessId = 0;

	QMap<quint64, CProgramFile::SExecutionInfo> m_ExecStats;
	std::atomic<bool>			m_ExecChanged{ true };
	quint64						m_ExecFetched = 0;
	quint32						m_ExecRevision = 0;

	QMap<quint64, CProgramFile::SIngressInfo> m_Ingress;
	std::atomic<bool>			m_IngressChanged{ true };
	quint64						m_IngressFetched = 0;
	quint32						m_IngressRevision = 0;

	SAccessLog					m_AccessLog;
	quint64						m_AccessLastActivity = 0;
	quint64						m_AccessLastEvent = 0;
	quint32						m_AccessCount = 0;
	quint32 					m_HandleCount = 0;

	QSet<quint64>				m_SocketRefs;

	QHash<QString, CTrafficEntryPtr> m_TrafficLog;
	QHash<QHostAddress, QSet<CTrafficEntryPtr>> m_Unresolved;
	quint64						m_TrafficLogLastActivity = 0;
	quint32						m_TrafficLogGeneration = 0; // incremented on every clear, so views know their aggregates are stale
};

typedef QSharedPointer<CWindowsService> CWindowsServicePtr;
typedef QWeakPointer<CWindowsService> CWindowsServiceRef;