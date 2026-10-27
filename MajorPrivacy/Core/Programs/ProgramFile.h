#pragma once
#include <atomic>
#include "ProgramGroup.h"

#include "../Processes/Process.h"
#include "../TraceLogEntry.h"
#include "../Network/TrafficEntry.h"
#include "../Processes/ExecLogEntry.h"
#include "../Access/AccessEntry.h"

#define PROG_STATS_REFETCH_INTERVAL 1000 // ms, a list marked changed is fetched again at most this often
#define PROG_VIEW_REBUILD_INTERVAL 5000 // ms, views rebuild at least this often even when no list changed

class CProgramFile: public CProgramSet
{
	Q_OBJECT
public:
	CProgramFile(QObject* parent = nullptr);

	virtual EProgramType GetType() const { return EProgramType::eProgramFile; }

	virtual QString GetNameEx() const;
	virtual QString GetPublisher() const;
	virtual void SetPath(const QString& Path)		{ QWriteLocker Lock(&m_Mutex); m_Path = Path; }
	virtual QString GetPath() const					{ QReadLocker Lock(&m_Mutex); return m_Path; }
	virtual QString GetIconFile() const				{ QReadLocker Lock(&m_Mutex); return m_IconFile.isEmpty() ? m_Path : m_IconFile; }

	virtual CImageSignInfo GetSignInfo() const		{ QReadLocker Lock(&m_Mutex); return m_SignInfo; }

	virtual QSet<quint64> GetProcessPids() const	{ QReadLocker Lock(&m_Mutex); return m_ProcessPids; }

	virtual void CountStats();

	QMultiMap<quint64, SLibraryInfo> GetLibraries();
	void SetLibrariesChanged() { m_LibrariesChanged = true; }
	quint32 GetLibrariesRevision() const { QReadLocker Lock(&m_Mutex); return m_LibrariesRevision; }

	static QString GetSignatureInfoStr(const CImageSignInfo& SignInfo);

	struct SInfo
	{
		EExecLogRole				Role = EExecLogRole::eUndefined;
		QFlexGuid					EnclaveGuid;
		uint64						SubjectUID = 0;
		uint64						SubjectPID = 0; // this is not the actual PIC but the PUID
		QFlexGuid					SubjectEnclave;
		QString						ActorSvcTag;
	};

	struct SExecutionInfo: SInfo
	{
		uint64						LastExecTime = 0;
		bool						bBlocked = false;
		QString						CommandLine;
	};

	QMap<quint64, SExecutionInfo> GetExecStats(); // keys are only positions, the entries are identified by SubjectUID and SubjectPID
	quint32 GetExecRevision() const { QReadLocker Lock(&m_Mutex); return m_ExecRevision; }

	struct SIngressInfo: SInfo
	{
		uint64						LastAccessTime = 0;
		bool						bBlocked = false;
		uint32						ThreadAccessMask = 0;
		uint32						ProcessAccessMask = 0;
	};

	QMap<quint64, SIngressInfo> GetIngressStats(); // keys are only positions, the entries are identified by SubjectUID and SubjectPID
	quint32 GetIngressRevision() const { QReadLocker Lock(&m_Mutex); return m_IngressRevision; }

	virtual QMap<quint64, SAccessStatsPtr>	GetAccessStats(); // keyed by change sequence, see SAccessLog

	quint64 GetAccessLastActivity() const { return m_AccessLastActivity; }
	quint64 GetAccessLastEvent() const { return m_AccessLastEvent; }
	quint32 GetAccessCount() const { return m_AccessCount; }
	quint32 GetHandleCount() const { return m_HandleCount; }

	virtual QHash<QString, CTrafficEntryPtr> GetTrafficLog(QList<CTrafficEntryPtr>* pChanged = nullptr);
	quint32 GetTrafficLogGeneration() const { return m_TrafficLogGeneration; }

	virtual void TraceLogAdd(ETraceLogs Log, const CLogEntryPtr& pEntry, quint64 Index);
	virtual STraceLogList* GetTraceLog(ETraceLogs Log);
	virtual void ClearTraceLog(ETraceLogs Log);

	virtual void ClearAccessLog();
	virtual void ClearProcessLogs();
	virtual void ClearTrafficLog();

protected:
	friend class CProgramManager;

	virtual QSharedPointer<class CWindowsService> GetService(const QString& SvcTag) const;

	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	QString						m_Path;

	CImageSignInfo				m_SignInfo;

	quint64						m_LastExec = 0;

	QSet<quint64>				m_ProcessPids;

	//
	// The *Changed flags are set from the event handlers, the *Revision counters go up whenever
	// the list actually got refetched (or cleared), so views can skip rebuilding when nothing changed.
	//

	QMultiMap<quint64, SLibraryInfo> m_Libraries;
	std::atomic<bool>			m_LibrariesChanged{ true };
	quint64						m_LibrariesFetched = 0;
	quint32						m_LibrariesRevision = 0;

	QMap<quint64, SExecutionInfo> m_ExecStats;
	std::atomic<bool>			m_ExecChanged{ true };
	quint64						m_ExecFetched = 0;
	quint32						m_ExecRevision = 0;

	QMap<quint64, SIngressInfo>	m_Ingress;
	std::atomic<bool>			m_IngressChanged{ true };
	quint64						m_IngressFetched = 0;
	quint32						m_IngressRevision = 0;

	SAccessLog					m_AccessLog;
	quint64						m_AccessLastActivity = 0;
	quint64						m_AccessLastEvent = 0;
	quint32						m_AccessCount = 0;
	quint32						m_HandleCount = 0;

	QSet<quint64>				m_SocketRefs;

	QHash<QString, CTrafficEntryPtr> m_TrafficLog;
	QHash<QHostAddress, QSet<CTrafficEntryPtr>> m_Unresolved;
	quint64						m_TrafficLogLastActivity = 0;
	quint32						m_TrafficLogGeneration = 0; // incremented on every clear, so views know their aggregates are stale

	STraceLogList				m_Logs[(int)ETraceLogs::eLogMax];

};

typedef QSharedPointer<CProgramFile> CProgramFilePtr;
typedef QWeakPointer<CProgramFile> CProgramFileRef;