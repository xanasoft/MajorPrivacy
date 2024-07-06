#include "pch.h"
#include "ProgramFile.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/AppUtil.h"
#include "../ServiceCore.h"
#include "WindowsService.h"

CProgramFile::CProgramFile(const std::wstring& FileName)
{
	ASSERT(!FileName.empty());
	if (!FileName.empty()) {
		m_ID.Set(EProgramType::eProgramFile, FileName);
		SImageVersionInfoPtr pInfo = GetImageVersionInfoNt(FileName);
		m_Name = pInfo ? pInfo->FileDescription : FileName;
		//m_IconFile = NtPathToDosPath(FileName);
		m_IconFile = FileName;
	}
	m_Path = FileName;
}

void CProgramFile::AddProcess(const CProcessPtr& pProcess)
{
	uint64 pid = pProcess->GetProcessId();

	std::unique_lock lock(m_Mutex);

	m_Processes.insert(std::make_pair(pid, pProcess));
}

void CProgramFile::RemoveProcess(const CProcessPtr& pProcess)
{
	uint64 pid = pProcess->GetProcessId();
	uint64 LastActivity = pProcess->GetLastActivity();
	uint64 Uploaded = pProcess->GetUploaded();
	uint64 Downloaded = pProcess->GetDownloaded();

	std::unique_lock lock(m_Mutex);

	m_Processes.erase(pid);
}

CAppPackagePtr CProgramFile::GetAppPackage() const
{
	std::unique_lock lock(m_Mutex); 

	CAppPackagePtr pApp;
	for (auto pGroup : m_Groups) {
		pApp = std::dynamic_pointer_cast<CAppPackage>(pGroup.second.lock());
		if (pApp) // a program file should never belong to more than one app package
			break; 
	}
	return pApp;
}

std::list<CWindowsServicePtr> CProgramFile::GetAllServices() const
{
	std::unique_lock lock(m_Mutex);

	std::list<CWindowsServicePtr> Svcs;
	for (auto pNode : m_Nodes) {
		CWindowsServicePtr pSvc = std::dynamic_pointer_cast<CWindowsService>(pNode);
		if (pSvc)
			Svcs.push_back(pSvc);
	}
	return Svcs;
}

CWindowsServicePtr CProgramFile::GetService(const std::wstring& SvcTag) const
{
	std::unique_lock lock(m_Mutex);

	for (auto pNode : m_Nodes) {
		CWindowsServicePtr pSvc = std::dynamic_pointer_cast<CWindowsService>(pNode);
		if (pSvc && pSvc->GetSvcTag() == SvcTag)
			return pSvc;
	}
	return nullptr;
}

void CProgramFile::UpdateSignInfo(KPH_VERIFY_AUTHORITY SignAuthority, uint32 SignLevel, uint32 SignPolicy)
{
	std::unique_lock lock(m_Mutex);

	m_SignInfo.Authority = (uint8)SignAuthority;
	if(SignLevel) m_SignInfo.Level = (uint8)SignLevel;
	if(SignPolicy) m_SignInfo.Policy = SignPolicy;
}

void CProgramFile::AddLibrary(const CProgramLibraryPtr& pLibrary, uint64 LoadTime, KPH_VERIFY_AUTHORITY SignAuthority, uint32 SignLevel, uint32 SignPolicy, EEventStatus Status)
{
	std::unique_lock lock(m_Mutex);

	SLibraryInfo& Info = m_Libraries[pLibrary->GetUID()];
	Info.LastLoadTime = LoadTime;
	Info.TotalLoadCount++;
	Info.Sign.Authority = (uint8)SignAuthority;
	if(SignLevel) Info.Sign.Level = (uint8)SignLevel;
	if(SignPolicy) Info.Sign.Policy = SignPolicy;
	Info.LastStatus = Status;
}

CVariant CProgramFile::DumpLibraries() const
{
	std::unique_lock lock(m_Mutex);

	CVariant List;
	List.BeginList();
	for (auto pItem: m_Libraries)
	{
		CVariant vLib;
		vLib.BeginIMap();
		vLib.Write(API_V_LIB_REF, pItem.first);
		vLib.Write(API_V_LIB_LOAD_TIME, pItem.second.LastLoadTime);
		vLib.Write(API_V_LIB_LOAD_COUNT, pItem.second.TotalLoadCount);
		vLib.Write(API_V_SIGN_INFO, pItem.second.Sign.Data);
		//vLib.Write(API_V_SIGN_INFO_AUTH, pItem.second.Sign.Authority);
		//vLib.Write(API_V_SIGN_INFO_LEVEL, pItem.second.Sign.Level);
		//vLib.Write(API_V_SIGN_INFO_POLICY, pItem.second.Sign.Policy);
		vLib.Write(API_V_LIB_STATUS, (uint32)pItem.second.LastStatus);
		vLib.Finish();
		List.WriteVariant(vLib);
	}
	List.Finish();
	return List;
}

void CProgramFile::AddExecActor(const std::shared_ptr<CProgramFile>& pActorProgram, const std::wstring& ActorServiceTag, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	CWindowsServicePtr pActorService = pActorProgram->GetService(ActorServiceTag);

	std::unique_lock lock(m_Mutex);

	SExecInfo& Info = m_ExecActors[pActorService ? pActorService->GetUID() : pActorProgram->GetUID()];
	Info.bBlocked = bBlocked;
	Info.LastExecTime = CreateTime;
	Info.CommandLine = CmdLine;
}

void CProgramFile::AddExecTarget(const std::wstring& ActorServiceTag, const std::shared_ptr<CProgramFile>& pProgram, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	CWindowsServicePtr pActorService = GetService(ActorServiceTag);
	if (pActorService) {
		pActorService->AddExecTarget(pProgram, CmdLine, CreateTime, bBlocked);
		return;
	}

	std::unique_lock lock(m_Mutex);

	SExecInfo& Info = m_ExecTargets[pProgram->GetUID()];
	Info.bBlocked = bBlocked;
	Info.LastExecTime = CreateTime;
	Info.CommandLine = CmdLine;
}

CVariant CProgramFile::DumpExecStats() const
{
	std::unique_lock lock(m_Mutex);

	CVariant Actors;
	Actors.BeginList();
	for (auto& pItem : m_ExecActors)
	{
		CVariant vActor;
		vActor.BeginIMap();
		vActor.Write(API_V_PROC_REF, (uint64)&pItem.second);
		vActor.Write(API_V_PROC_EVENT_ACTOR, pItem.first);
		vActor.Write(API_V_PROC_EVENT_LAST_EXEC, pItem.second.LastExecTime);
		vActor.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
		vActor.Write(API_V_PROC_EVENT_CMD_LINE, pItem.second.CommandLine);
		vActor.Finish();
		Actors.WriteVariant(vActor);
	}
	Actors.Finish();

	CVariant Targets;
	Targets.BeginList();
	for (auto& pItem : m_ExecTargets)
	{
		CVariant vTarget;
		vTarget.BeginIMap();
		vTarget.Write(API_V_PROC_REF, (uint64)&pItem.second);
		vTarget.Write(API_V_PROC_EVENT_TARGET, pItem.first);
		vTarget.Write(API_V_PROC_EVENT_LAST_EXEC, pItem.second.LastExecTime);
		vTarget.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
		vTarget.Write(API_V_PROC_EVENT_CMD_LINE, pItem.second.CommandLine);
		vTarget.Finish();
		Targets.WriteVariant(vTarget);
	}

	for(auto& pNode : m_Nodes)
	{
		CWindowsServicePtr pSvc = std::dynamic_pointer_cast<CWindowsService>(pNode);
		if(!pSvc) continue;
		for (auto pItem : pSvc->GetExecTargets())
		{
			CVariant vTarget;
			vTarget.BeginIMap();
			vTarget.Write(API_V_PROC_REF, (uint64)&pItem.second);
			vTarget.Write(API_V_PROG_SVC_TAG, pSvc->GetSvcTag()); // actor service tag
			vTarget.Write(API_V_PROC_EVENT_TARGET, pItem.first);
			vTarget.Write(API_V_PROC_EVENT_LAST_EXEC, pItem.second.LastExecTime);
			vTarget.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
			vTarget.Write(API_V_PROC_EVENT_CMD_LINE, pItem.second.CommandLine);
			vTarget.Finish();
			Targets.WriteVariant(vTarget);
		}
	}
	Targets.Finish();

	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROC_TARGETS, Targets);
	Data.WriteVariant(API_V_PROC_ACTORS, Actors);
	Data.Finish();
	return Data;
}

void CProgramFile::AddIngressActor(const std::shared_ptr<CProgramFile>& pActorProgram, const std::wstring& ActorServiceTag, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	CWindowsServicePtr pActorService = pActorProgram->GetService(ActorServiceTag);

	std::unique_lock lock(m_Mutex);

	SAccessInfo& Info = m_IngressActors[pActorService ? pActorService->GetUID() : pActorProgram->GetUID()];
	Info.bBlocked = bBlocked;
	Info.LastAccessTime = AccessTime;
	if(bThread) Info.ThreadAccessMask |= AccessMask;
	else Info.ProcessAccessMask |= AccessMask;
}

void CProgramFile::AddIngressTarget(const std::wstring& ActorServiceTag, const std::shared_ptr<CProgramFile>& pProgram, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	CWindowsServicePtr pActorService = GetService(ActorServiceTag);
	if (pActorService) {
		pActorService->AddIngressTarget(pProgram, bThread, AccessMask, AccessTime, bBlocked);
		return;
	}

	std::unique_lock lock(m_Mutex);

	SAccessInfo& Info = m_IngressTargets[pProgram->GetUID()];
	Info.bBlocked = bBlocked;
	Info.LastAccessTime = AccessTime;
	if(bThread) Info.ThreadAccessMask |= AccessMask;
	else Info.ProcessAccessMask |= AccessMask;
}

CVariant CProgramFile::DumpIngress() const
{
	std::unique_lock lock(m_Mutex);

	CVariant Actors;
	Actors.BeginList();
	for (auto& pItem : m_IngressActors)
	{
		CVariant vActor;
		vActor.BeginIMap();
		vActor.Write(API_V_PROC_REF, (uint64)&pItem.second);
		vActor.Write(API_V_PROC_EVENT_ACTOR, pItem.first);
		vActor.Write(API_V_THREAD_ACCESS_MASK, pItem.second.ThreadAccessMask);
		vActor.Write(API_V_PROCESS_ACCESS_MASK, pItem.second.ProcessAccessMask);
		vActor.Write(API_V_PROC_EVENT_LAST_ACCESS, pItem.second.LastAccessTime);
		vActor.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
		vActor.Finish();
		Actors.WriteVariant(vActor);
	}
	Actors.Finish();

	CVariant Targets;
	Targets.BeginList();
	for (auto& pItem : m_IngressTargets)
	{
		CVariant vTarget;
		vTarget.BeginIMap();
		vTarget.Write(API_V_PROC_REF, (uint64)&pItem.second);
		vTarget.Write(API_V_PROC_EVENT_TARGET, pItem.first);
		vTarget.Write(API_V_THREAD_ACCESS_MASK, pItem.second.ThreadAccessMask);
		vTarget.Write(API_V_PROCESS_ACCESS_MASK, pItem.second.ProcessAccessMask);
		vTarget.Write(API_V_PROC_EVENT_LAST_ACCESS, pItem.second.LastAccessTime);
		vTarget.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
		vTarget.Finish();
		Targets.WriteVariant(vTarget);
	}

	for(auto pNode : m_Nodes)
	{
		CWindowsServicePtr pSvc = std::dynamic_pointer_cast<CWindowsService>(pNode);
		if(!pSvc) continue;
		for (auto& pItem : pSvc->GetIngressTargets())
		{
			CVariant vTarget;
			vTarget.BeginIMap();
			vTarget.Write(API_V_PROC_REF, (uint64)&pItem.second);
			vTarget.Write(API_V_PROG_SVC_TAG, pSvc->GetSvcTag()); // actor service tag
			vTarget.Write(API_V_PROC_EVENT_TARGET, pItem.first);
			vTarget.Write(API_V_THREAD_ACCESS_MASK, pItem.second.ThreadAccessMask);
			vTarget.Write(API_V_PROCESS_ACCESS_MASK, pItem.second.ProcessAccessMask);
			vTarget.Write(API_V_PROC_EVENT_LAST_ACCESS, pItem.second.LastAccessTime);
			vTarget.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
			vTarget.Finish();
			Targets.WriteVariant(vTarget);
		}
	}
	Targets.Finish();
	
	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROC_TARGETS, Targets);
	Data.WriteVariant(API_V_PROC_ACTORS, Actors);
	Data.Finish();
	return Data;
}

void CProgramFile::AddAccess(const std::wstring& ActorServiceTag, const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	if (!ActorServiceTag.empty()) {
		CWindowsServicePtr pActorService = GetService(ActorServiceTag);
		if (pActorService) {
			pActorService->AddAccess(Path, AccessMask, AccessTime, bBlocked);
			return;
		}
	}

	std::unique_lock lock(m_Mutex); 

	m_AccessTree.Add(Path, AccessMask, AccessTime, bBlocked);
}

CVariant CProgramFile::DumpAccess() const
{
	std::unique_lock lock(m_Mutex); 

	return m_AccessTree.DumpTree();
}

uint64 CProgramFile::AddTraceLogEntry(const CTraceLogEntryPtr& pLogEntry, ETraceLogs Log)
{
	std::unique_lock lock(m_Mutex); 

	STraceLog& TraceLog = m_TraceLogs[(int)Log];
	TraceLog.Entries.push_back(pLogEntry);
	return TraceLog.IndexOffset + TraceLog.Entries.size() - 1;
}

CProgramFile::STraceLog CProgramFile::GetTraceLog(ETraceLogs Log) const
{ 
	std::unique_lock lock(m_Mutex); 

	return m_TraceLogs[(int)Log];
}

void CProgramFile::CleanupTraceLog()
{
	std::unique_lock lock(m_Mutex); 

	uint64 CleanupCount = theCore->Config()->GetUInt64("Service", "TraceLogRetentionCount", 1000); // keep last 1000 entries by default
	//uint64 CleanupData = GetCurrentTimeAsFileTime() - theCore->Config()->GetUInt64("Service", "TraceLogRetentionMinutes", 60) * 60 * 10000000ULL; // default 1 hour

	for (int i = 0; i < (int)ETraceLogs::eLogMax; i++)
	{
		STraceLog& TraceLog = m_TraceLogs[i];

		if(TraceLog.Entries.size() <= CleanupCount)
			continue;
#ifdef _DEBUG
		//DbgPrint("Cleaned up %d log (%d) entries from %S\n", TraceLog.Entries.size() - CleanupCount, i, m_FileName.c_str());
#endif
		auto it = TraceLog.Entries.begin() + (TraceLog.Entries.size() - CleanupCount);

		/*auto it = std::lower_bound(TraceLog.Entries.begin(), TraceLog.Entries.end(), CleanupData, [](const CTraceLogEntryPtr& entry, uint64 CleanupData) {
			return entry->GetTimeStamp() < CleanupData;
		});*/

		if (it != TraceLog.Entries.begin()) {
			TraceLog.IndexOffset += std::distance(TraceLog.Entries.begin(), it);
			TraceLog.Entries.erase(TraceLog.Entries.begin(), it);
		}
	}
}

void CProgramFile::CollectStats(SStats& Stats) const
{
	Stats.LastActivity = m_TrafficLog.GetLastActivity();
	Stats.Uploaded = m_TrafficLog.GetUploaded();
	Stats.Downloaded = m_TrafficLog.GetDownloaded();
	for (auto I : m_Processes) 
	{
		Stats.Pids.insert(I.first);

		if (I.second->GetLastActivity() > Stats.LastActivity)
			Stats.LastActivity = I.second->GetLastActivity();
		Stats.Upload += I.second->GetUpload();
		Stats.Download += I.second->GetDownload();
		// Note: this is not the same as the sum of the sockets' upload/download, as it includes data from closed sockets
		//Stats.Uploaded += I.second->GetUploaded();
		//Stats.Downloaded += I.second->GetDownloaded();

		std::set<CSocketPtr> Sockets = I.second->GetSocketList();
		for (auto pSocket : Sockets) 
		{
			Stats.SocketRefs.insert((uint64)pSocket.get());

			//if (pSocket->GetLastActivity() > LastActivity)
			//	Stats.LastActivity = pSocket->GetLastActivity();
			//Stats.Upload += pSocket->GetUpload();
			//Stats.Download += pSocket->GetDownload();
			Stats.Uploaded += pSocket->GetUploaded();
			Stats.Downloaded += pSocket->GetDownloaded();
		}
	}
}