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

	QtVariant& Reply = Ret.GetValue();

	const QtVariant& Processes = Reply.Get(API_V_PROCESSES);

	//
	// The reply is a delta: an entry that is absent has not changed, and a field
	// that is absent has not changed either - so this merges, it does not replace.
	// What went away is named in API_V_OLD_PROCESSES and nowhere else.
	//
	// Except when the round was a complete list, which the service sends whenever
	// its cache was empty - a fresh connection, or a service restarted underneath
	// us - and which is also what a service built without the caches sends every
	// time. Then anything it did not mention is gone, and that is what puts the two
	// sides back in step without either of them tracking reconnects.
	//
	// Told apart by the removed list simply not being there: a complete round has
	// nothing removed to report, so its absence says complete and its presence -
	// empty or not - says delta.
	//
	bool bIsFull = !Reply.Has(API_V_OLD_PROCESSES);

	QWriteLocker Lock(&m_Mutex);

	QHash<quint64, CProcessPtr> OldMap;
	if (bIsFull)
		OldMap = m_List;

	QtVariantReader(Processes).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Process = *(QtVariant*)&vData;

		//
		// Keyed by the PUID and not by the pid: pids get recycled, and as this list
		// also carries recently terminated processes a new process could otherwise be
		// merged into the entry of a dead one that happens to have had its number.
		//
		QtVariant vPuid(Process.Allocator());
		FW::CVariantReader::Find(Process, API_V_PUID, vPuid);
		quint64 Puid = vPuid;

		QtVariant vPid(Process.Allocator());
		FW::CVariantReader::Find(Process, API_V_PID, vPid);
		quint64 Pid = vPid;

		//
		// Looked up in the list and not taken out of OldMap: on a delta round OldMap
		// is empty, and taking from it would make every entry look new and throw away
		// everything merged into it so far. OldMap only tracks what went unmentioned.
		//
		CProcessPtr pProcess = m_List.value(Puid);
		bool bAdded;
		if (bAdded = pProcess.isNull()) {
			pProcess = CProcessPtr(new CProcess(Pid));
			m_List.insert(Puid, pProcess);
		}
		else
			OldMap.remove(Puid);
		
		pProcess->FromVariant(Process, bIsFull);	

		if (pProcess->IsProtected() && !pProcess->GetEnclave()) {
			CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(pProcess->GetEnclaveGuid(), true);
			if (pEnclave) {
				pProcess->SetEnclave(pEnclave);
				pEnclave->AddProcess(pProcess);
			}
		}

	});

	QtVariantReader(Reply.Get(API_V_OLD_PROCESSES)).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Data = *(QtVariant*)&vData;
		OldMap.insert(Data.To<quint64>(0), CProcessPtr());
	});

	foreach(const quint64 & Puid, OldMap.keys()) {
		CProcessPtr pProcess = m_List.take(Puid);
		if (pProcess.isNull())
			continue;
		CEnclavePtr pEnclave = pProcess->GetEnclave();
		if(pEnclave) 
			pEnclave->RemoveProcess(pProcess);
	}

	//
	// Summed over the whole list and not over what this round mentioned, most of
	// which is not mentioned any more.
	//
	m_SocketCount = 0;
	m_HandleCount = 0;
	foreach(const CProcessPtr & pProcess, m_List) {
		m_SocketCount += pProcess->GetSocketCount();
		m_HandleCount += pProcess->GetHandleCount();
	}

	//
	// The pid index is rebuilt rather than maintained, because a pid is not unique
	// here: a terminated process is kept in the list for a while and a new process
	// may already carry its number. A lookup by pid wants the live one.
	//
	m_ProcessByPID.clear();
	foreach(const CProcessPtr & pProcess, m_List) {
		CProcessPtr& pSlot = m_ProcessByPID[pProcess->GetProcessId()];
		if (pSlot.isNull())
			pSlot = pProcess;
		else if (pSlot->GetStatus() & MP_PROCESS_FLAG_TERMINATED) {
			// a running one wins, among terminated ones the newest
			if (!(pProcess->GetStatus() & MP_PROCESS_FLAG_TERMINATED) || pSlot->GetCreationTime() < pProcess->GetCreationTime())
				pSlot = pProcess;
		}
	}

	//DbgPrint("CProcessList::Update() took %llu ms\n", GetTickCount64() - uStart);

	return OK;
}

QHash<quint64, CProcessPtr> CProcessList::GetProcesses(const QSet<quint64>& Pids)
{
	QReadLocker Lock(&m_Mutex);

	QHash<quint64, CProcessPtr> List;
	foreach(quint64 Pid, Pids) {
		CProcessPtr pProcess = m_ProcessByPID.value(Pid);
		if (pProcess)
			List.insert(Pid, pProcess);
	}
	return List;
}

CProcessPtr CProcessList::GetProcess(quint64 Pid, bool CanUpdate)
{ 
	QReadLocker Lock(&m_Mutex);

	CProcessPtr pProcess = m_ProcessByPID.value(Pid); 

	if (!pProcess && CanUpdate) 
	{
		Lock.unlock();

		auto Ret = theCore->GetProcess(Pid);
		if (!Ret.IsError())
		{
			QWriteLocker Lock(&m_Mutex);

			//
			// Read before enlisting, the PUID is the key and it only becomes known
			// once the reply has been parsed.
			//
			pProcess = CProcessPtr(new CProcess(Pid));
			pProcess->FromVariant(Ret.GetValue());

			CProcessPtr pExisting = m_List.value(pProcess->GetProcessUID());
			if (pExisting)
				pProcess = pExisting;
			else
				m_List.insert(pProcess->GetProcessUID(), pProcess);
			m_ProcessByPID.insert(Pid, pProcess);
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