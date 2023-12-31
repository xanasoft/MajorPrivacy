#pragma once
#include "Process.h"

class CProcessList : public QObject
{
	Q_OBJECT
public:
	CProcessList(QObject* parent);

	void Update();

	QMap<quint64, CProcessPtr> List() { return m_List; }
	QMap<quint64, CProcessPtr> GetProcesses(const QSet<quint64>& Pids);
	CProcessPtr GetProcess(quint64 Pid, bool CanUpdate = false);

	int GetCount() const { return m_List.count(); }
	int GetSocketCount() const { return m_SocketCount; }

protected:
	QMap<quint64, CProcessPtr> m_List;
	int m_SocketCount = 0;
};