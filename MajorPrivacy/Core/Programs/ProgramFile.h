#pragma once
#include "ProgramGroup.h"

#include "../Process.h"
#include "../TraceLogEntry.h"
#include "../Network/TrafficEntry.h"

class CProgramFile: public CProgramSet
{
	Q_OBJECT
public:
	CProgramFile(QObject* parent = nullptr);

	virtual QString GetNameEx() const;
	virtual QString GetPath() const					{ return m_FileName; }

	virtual QSet<quint64> GetProcessPids() const	{ return m_ProcessPids; }

	virtual void CountStats();

	virtual QMap<QString, CTrafficEntryPtr>	GetTrafficLog();

	virtual void TraceLogAdd(ETraceLogs Log, const CLogEntryPtr& pEntry, quint32 Index);
	virtual STraceLogList* GetTraceLog(ETraceLogs Log);

protected:
	friend class CProgramManager;

	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	QString m_FileName;

	QSet<quint64>				m_ProcessPids;
	
	QSet<quint64>				m_SocketRefs;

	QMap<QString, CTrafficEntryPtr> m_TrafficLog;
	quint64						m_TrafficLogLastActivity = 0;

	STraceLogList				m_Logs[(int)ETraceLogs::eLogMax];

};

typedef QSharedPointer<CProgramFile> CProgramFilePtr;
typedef QWeakPointer<CProgramFile> CProgramFileRef;