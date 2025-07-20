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
	virtual QString GetPublisher() const;
	virtual void SetPath(const QString& Path)		{ m_Path = Path; }
	virtual QString GetPath() const					{ return m_Path; }
	virtual QString GetIconFile() const				{ return m_IconFile.isEmpty() ? m_Path : m_IconFile; }

	virtual const CImageSignInfo& GetSignInfo() const{ return m_SignInfo; }

	virtual QSet<quint64> GetProcessPids() const	{ return m_ProcessPids; }

	virtual void CountStats();

	QMultiMap<quint64, SLibraryInfo> GetLibraries();
	void SetLibrariesChanged() { m_LibrariesChanged = true; }

	static QString GetSignatureInfoStr(UCISignInfo SignInfo);

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

	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	QString						m_Path;

	CImageSignInfo				m_SignInfo;

	quint64						m_LastExec = 0;

	QSet<quint64>				m_ProcessPids;

	QMultiMap<quint64, SLibraryInfo> m_Libraries;
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