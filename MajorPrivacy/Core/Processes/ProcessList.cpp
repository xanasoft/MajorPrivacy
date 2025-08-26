#include "pch.h"
#include "ProcessList.h"
#include "../Enclaves/EnclaveManager.h"
#include "Core/PrivacyCore.h"
#include "Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "./Common/QtVariant.h"

CProcessList::CProcessList(QObject* parent)
 : QObject(parent)
{
}

STATUS CProcessList::Update()
{
	//////////////////////////////////////////////////////////
	// WARING This is called from a differnt thread
	//////////////////////////////////////////////////////////
	
	//uint64 uStart = GetTickCount64();

	auto Ret = theCore->GetProcesses();

	//DbgPrint("theCore->GetProcesses() took %llu ms\n", GetTickCount64() - uStart);
	
	if (Ret.IsError())
		return Ret.GetStatus();

	m_SocketCount = 0;
	m_HandleCount = 0;

	QtVariant& Processes = Ret.GetValue();

	QWriteLocker Lock(&m_Mutex);

	QHash<quint64, CProcessPtr> OldMap = m_List;

	QtVariantReader(Processes).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Process = *(QtVariant*)&vData;

		QtVariant vPid(Process.Allocator());
		FW::CVariantReader::Find(Process, API_V_PID, vPid);
		quint64 Pid = vPid;

		CProcessPtr pProcess = OldMap.take(Pid);
		bool bAdded;
		if (bAdded = pProcess.isNull()) {
			pProcess = CProcessPtr(new CProcess(Pid));
			m_List.insert(Pid, pProcess);
		}
		
		pProcess->FromVariant(Process);	

		if (pProcess->IsProtected() && !pProcess->GetEnclave()) {
			CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(pProcess->GetEnclaveGuid(), true);
			if (pEnclave) {
				pProcess->SetEnclave(pEnclave);
				pEnclave->AddProcess(pProcess);
			}
		}

		m_SocketCount += pProcess->GetSocketCount();
		m_HandleCount += pProcess->GetHandleCount();
	});

	foreach(const quint64 & Pid, OldMap.keys()) {
		CProcessPtr pProcess = m_List.take(Pid);
		CEnclavePtr pEnclave = pProcess->GetEnclave();
		if(pEnclave) 
			pEnclave->RemoveProcess(pProcess);
	}

	//DbgPrint("CProcessList::Update() took %llu ms\n", GetTickCount64() - uStart);

	return OK;
}

QHash<quint64, CProcessPtr> CProcessList::GetProcesses(const QSet<quint64>& Pids)
{
	QReadLocker Lock(&m_Mutex);

	QHash<quint64, CProcessPtr> List;
	foreach(quint64 Pid, Pids) {
		CProcessPtr pProcess = m_List.value(Pid);
		if (pProcess)
			List.insert(Pid, pProcess);
	}
	return List;
}

CProcessPtr CProcessList::GetProcess(quint64 Pid, bool CanUpdate)
{ 
	QReadLocker Lock(&m_Mutex);

	CProcessPtr pProcess = m_List.value(Pid); 

	if (!pProcess && CanUpdate) 
	{
		Lock.unlock();

		auto Ret = theCore->GetProcess(Pid);
		if (!Ret.IsError())
		{
			QWriteLocker Lock(&m_Mutex);

			pProcess = CProcessPtr(new CProcess(Pid));
			m_List.insert(Pid, pProcess);
			pProcess->FromVariant(Ret.GetValue());
		}
	}

	return pProcess;
}

STATUS CProcessList::TerminateProcess(const CProcessPtr& pProcess)
{
	auto Ret = theCore->TerminateProcess(pProcess->GetProcessId());
	if (Ret.IsError())
		return Ret.GetStatus();
	return OK;
}