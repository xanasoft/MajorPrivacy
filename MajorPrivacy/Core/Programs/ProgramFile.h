#pragma once
#include "ProgramGroup.h"

#include "../Processes/Process.h"
#include "../TraceLogEntry.h"
#include "../Network/TrafficEntry.h"
#include "../Processes/ExecLogEntry.h"
#include "../Access/AccessEntry.h"

class CProgramFile: public CProgramSet
{
	Q_OBJECT
public:
	CProgramFile(QObject* parent = nullptr);

	virtual EProgramType GetType() const { return EProgramType::eProgramFile; }

	virtual QString GetNameEx() const;
	virtual void SetPath(const QString& Path, EPathType Type) { m_Path.Set(Path, Type); }
	virtual QString GetPath(EPathType Type) const	{ return m_Path.Get(Type); }

	virtual SLibraryInfo::USign GetSignInfo() const	{ return m_SignInfo; }

	virtual QSet<quint64> GetProcessPids() const	{ return m_ProcessPids; }

	virtual void CountStats();

	QMap<quint64, SLibraryInfo> GetLibraries();

	static QString GetLibraryStatusStr(EEventStatus Status);
	static QString GetSignatureInfoStr(SLibraryInfo::USign SignInfo);

	struct SInfo
	{
		enum {
			eNone = 0,
			eTarget,
			eActor,
		}							eRole = eNone;
		uint64						ProgramUID = 0;
		QString						ActorSvcTag;
	};

	struct SExecutionInfo: SInfo
	{
		uint64						LastExecTime = 0;
		bool						bBlocked = false;
		QString						CommandLine;
	};

	QMap<quint64, SExecutionInfo> GetExecStats();

	struct SIngressInfo: SInfo
	{
		uint64						LastAccessTime = 0;
		bool						bBlocked = false;
		uint32						ThreadAccessMask = 0;
		uint32						ProcessAccessMask = 0;
	};

	QMap<quint64, SIngressInfo> GetIngressStats();

	virtual QMap<quint64, SAccessStatsPtr>	GetAccessStats();

	quint64 GetAccessLastActivity() const { return m_AccessLastActivity; }
	quint64 GetAccessLastEvent() const { return m_AccessLastEvent; }
	quint32 GetAccessCount() const { return m_AccessCount; }

	virtual QMap<QString, CTrafficEntryPtr>	GetTrafficLog();

	virtual void TraceLogAdd(ETraceLogs Log, const CLogEntryPtr& pEntry, quint64 Index);
	virtual STraceLogList* GetTraceLog(ETraceLogs Log);
	virtual void ClearTraceLog(ETraceLogs Log);

	virtual void ClearLogs(ETraceLogs Log);

	virtual void ClearAccessLog();
	virtual void ClearProcessLogs();
	virtual void ClearTrafficLog();

protected:
	friend class CProgramManager;

	virtual QSharedPointer<class CWindowsService> GetService(const QString& SvcTag) const;

	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	CFilePath					m_Path;

	SLibraryInfo::USign			m_SignInfo;

	quint64						m_LastExec = 0;

	QSet<quint64>				m_ProcessPids;

	QMap<quint64, SLibraryInfo>	m_Libraries;
	bool						m_LibrariesChanged = true;

	QMap<quint64, SExecutionInfo> m_ExecStats;
	bool						m_ExecChanged = true;

	QMap<quint64, SIngressInfo>	m_Ingress;
	bool						m_IngressChanged = true;

	QMap<quint64, SAccessStatsPtr> m_AccessStats;
	quint64						m_AccessLastActivity = 0;
	quint64						m_AccessLastEvent = 0;
	quint32						m_AccessCount = 0;

	QSet<quint64>				m_SocketRefs;

	QMap<QString, CTrafficEntryPtr> m_TrafficLog;
	quint64						m_TrafficLogLastActivity = 0;

	STraceLogList				m_Logs[(int)ETraceLogs::eLogMax];

};

typedef QSharedPointer<CProgramFile> CProgramFilePtr;
typedef QWeakPointer<CProgramFile> CProgramFileRef;