#pragma once
#include "Process.h"
#include "../Library/Status.h"

class CProcessList : public QObject
{
	Q_OBJECT
public:
	CProcessList(QObject* parent);

	STATUS Update();

	QHash<quint64, CProcessPtr> List() { return m_List; }
	QHash<quint64, CProcessPtr> GetProcesses(const QSet<quint64>& Pids);
	CProcessPtr GetProcess(quint64 Pid, bool CanUpdate = false);

	STATUS TerminateProcess(const CProcessPtr& pProcess);

	int GetCount() const { return m_List.count(); }
	int GetSocketCount() const { return m_SocketCount; }

protected:
	QHash<quint64, CProcessPtr> m_List;
	int m_SocketCount = 0;
};