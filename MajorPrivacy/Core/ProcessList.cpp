#include "pch.h"
#include "ProcessList.h"
#include "Core/PrivacyCore.h"
#include "Core/Programs/ProgramManager.h"
#include "../Service/ServiceAPI.h"
#include "../Library/Common/XVariant.h"

CProcessList::CProcessList(QObject* parent)
 : QObject(parent)
{
}

void CProcessList::Update()
{
	auto Ret = theCore->GetProcesses();
	if (Ret.IsError())
		return;

	m_SocketCount = 0;

	XVariant& Processes = Ret.GetValue();

	QMap<quint64, CProcessPtr> OldMap = m_List;

	Processes.ReadRawList([&](const CVariant& vData) {
		const XVariant& Process = *(XVariant*)&vData;

		quint64 Pid = Process.Find(SVC_API_PROC_PID);

		CProcessPtr pProcess = OldMap.take(Pid);
		if (pProcess.isNull())
		{
			pProcess = CProcessPtr(new CProcess(Pid));
			m_List.insert(Pid, pProcess);
		}
		
		pProcess->FromVariant(Process);	

		m_SocketCount += pProcess->GetSocketCount();
	});

	foreach(const quint64 & Pid, OldMap.keys())
		m_List.remove(Pid);
}

QMap<quint64, CProcessPtr> CProcessList::GetProcesses(const QSet<quint64>& Pids)
{
	QMap<quint64, CProcessPtr> List;
	foreach(quint64 Pid, Pids) {
		CProcessPtr pProcess = m_List.value(Pid);
		if (pProcess)
			List.insert(Pid, pProcess);
	}
	return List;
}

CProcessPtr CProcessList::GetProcess(quint64 Pid, bool CanUpdate)
{ 
	CProcessPtr pProcess = m_List.value(Pid); 

	if (!pProcess && CanUpdate) 
	{
		auto Ret = theCore->GetProcess(Pid);
		if (!Ret.IsError())
		{
			pProcess = CProcessPtr(new CProcess(Pid));
			m_List.insert(Pid, pProcess);
			pProcess->FromVariant(Ret.GetValue());
		}
	}

	return pProcess;
}