#pragma once
#include "Process.h"
#include "../Library/Status.h"

class CProcessList : public QObject
{
	Q_OBJECT
public:
	CProcessList(QObject* parent);

	STATUS Update();

	//
	// Keyed by PUID, not by pid - see CProcessList::Update.
	//
	QHash<quint64, CProcessPtr> List() { QReadLocker Lock(&m_Mutex); return m_List; }
	QHash<quint64, CProcessPtr> GetProcesses(const QSet<quint64>& Pids);
	CProcessPtr GetProcess(quint64 Pid, bool CanUpdate = false);

	STATUS TerminateProcess(const CProcessPtr& pProcess);

	int GetCount() const { QReadLocker Lock(&m_Mutex); return m_List.count(); }
	int GetSocketCount() const { return m_SocketCount; }
	int GetHandleCount() const { return m_HandleCount; }

protected:
	mutable QReadWriteLock m_Mutex;
	QHash<quint64, CProcessPtr> m_List;			// by PUID
	QHash<quint64, CProcessPtr> m_ProcessByPID;	// by pid, for lookups only
	int m_SocketCount = 0;
	int m_HandleCount = 0;
};