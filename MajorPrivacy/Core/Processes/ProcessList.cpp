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
	//uint64 uStart = GetTickCount64();

	auto Ret = theCore->GetProcesses();

	//DbgPrint("theCore->GetProcesses() took %llu ms\n", GetTickCount64() - uStart);
	
	if (Ret.IsError())
		return Ret.GetStatus();

	m_SocketCount = 0;

	QtVariant& Processes = Ret.GetValue();

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

STATUS CProcessList::TerminateProcess(const CProcessPtr& pProcess)
{
	auto Ret = theCore->TerminateProcess(pProcess->GetProcessId());
	if (Ret.IsError())
		return Ret.GetStatus();
	//m_List.remove(pProcess->GetProcessId());
	return OK;
}