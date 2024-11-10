#include "pch.h"
#include "ProgramFile.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Programs/ProgramManager.h"
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

	if(pProcess)
		m_LastExec = pProcess->GetCreateTimeStamp();
	m_Processes.insert(std::make_pair(pid, pProcess));
}

void CProgramFile::RemoveProcess(const CProcessPtr& pProcess)
{
	uint64 pid = pProcess->GetProcessId();
	uint64 LastActivity = pProcess->GetLastNetActivity();
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

CVariant CProgramFile::StoreLibraries(const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex);

	CVariant List;
	List.BeginList();
	for (auto pItem: m_Libraries)
	{
		CVariant vLib;
		vLib.BeginMap();
		CProgramLibraryPtr pLib = theCore->ProgramManager()->GetLibrary(pItem.first);
		if(!pLib) continue;
		vLib.Write(API_S_FILE_PATH, pLib->GetPath());
		vLib.Write(API_S_LIB_LOAD_TIME, pItem.second.LastLoadTime);
		vLib.Write(API_S_LIB_LOAD_COUNT, pItem.second.TotalLoadCount);
		vLib.Write(API_S_SIGN_INFO, pItem.second.Sign.Data);
		vLib.Write(API_S_SIGN_INFO_AUTH, pItem.second.Sign.Authority);
		vLib.Write(API_S_SIGN_INFO_LEVEL, pItem.second.Sign.Level);
		vLib.Write(API_S_SIGN_INFO_POLICY, pItem.second.Sign.Policy);
		vLib.Write(API_S_LIB_STATUS, (uint32)pItem.second.LastStatus);
		vLib.Finish();
		List.WriteVariant(vLib);
	}
	List.Finish();
	return List;
}

void CProgramFile::LoadLibraries(const CVariant& Data)
{
	std::unique_lock lock(m_Mutex);

	Data.ReadRawList([&](const CVariant& vData) {

		std::wstring Path = vData[API_S_FILE_PATH].AsStr();
		if (Path.empty()) return;
		CProgramLibraryPtr pLib = theCore->ProgramManager()->GetLibrary(Path, true);
		if (!pLib) return;

		uint64 LoadTime = vData[API_S_LIB_LOAD_TIME];
		KPH_VERIFY_AUTHORITY SignAuthority = (KPH_VERIFY_AUTHORITY)vData[API_S_SIGN_INFO_AUTH].To<uint32>();
		uint32 SignLevel = vData[API_S_SIGN_INFO_LEVEL];
		uint32 SignPolicy = vData[API_S_SIGN_INFO_POLICY];
		EEventStatus Status = (EEventStatus)vData[API_S_LIB_STATUS].To<uint32>();

		AddLibrary(pLib, LoadTime, SignAuthority, SignLevel, SignPolicy, Status);
	});
}

CVariant CProgramFile::DumpLibraries() const
{
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
		vLib.Write(API_V_SIGN_INFO_AUTH, pItem.second.Sign.Authority);
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

	m_AccessLog.AddExecActor(pActorService ? pActorService->GetUID() : pActorProgram->GetUID(), CmdLine, CreateTime, bBlocked);
}

void CProgramFile::AddExecTarget(const std::wstring& ActorServiceTag, const std::shared_ptr<CProgramFile>& pProgram, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	CWindowsServicePtr pActorService = GetService(ActorServiceTag);
	if (pActorService) {
		pActorService->AddExecTarget(pProgram, CmdLine, CreateTime, bBlocked);
		return;
	}

	std::unique_lock lock(m_Mutex);

	m_AccessLog.AddExecTarget(pProgram->GetUID(), CmdLine, CreateTime, bBlocked);
}

CVariant CProgramFile::DumpExecStats() const
{
	std::unique_lock lock(m_Mutex);

	SVarWriteOpt Opts;

	CVariant Actors;
	Actors.BeginList();
	m_AccessLog.DumpExecActors(Actors, Opts);
	Actors.Finish();

	CVariant Targets;
	Targets.BeginList();
	m_AccessLog.DumpExecTarget(Targets, Opts);

	for(auto& pNode : m_Nodes)
	{
		CWindowsServicePtr pSvc = std::dynamic_pointer_cast<CWindowsService>(pNode);
		if(!pSvc) continue;
		m_AccessLog.DumpExecTarget(Targets, Opts, pSvc->GetSvcTag());
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

	m_AccessLog.AddIngressActor(pActorService ? pActorService->GetUID() : pActorProgram->GetUID(), bThread, AccessMask, AccessTime, bBlocked);
}

void CProgramFile::AddIngressTarget(const std::wstring& ActorServiceTag, const std::shared_ptr<CProgramFile>& pProgram, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	CWindowsServicePtr pActorService = GetService(ActorServiceTag);
	if (pActorService) {
		pActorService->AddIngressTarget(pProgram, bThread, AccessMask, AccessTime, bBlocked);
		return;
	}

	std::unique_lock lock(m_Mutex);

	m_AccessLog.AddIngressTarget(pProgram->GetUID(), bThread, AccessMask, AccessTime, bBlocked);
}

CVariant CProgramFile::DumpIngress() const
{
	std::unique_lock lock(m_Mutex);

	SVarWriteOpt Opts;

	CVariant Actors;
	Actors.BeginList();
	m_AccessLog.DumpIngressActors(Actors, Opts);
	Actors.Finish();

	CVariant Targets;
	Targets.BeginList();
	m_AccessLog.DumpIngressTargets(Targets, Opts);

	for(auto pNode : m_Nodes)
	{
		CWindowsServicePtr pSvc = std::dynamic_pointer_cast<CWindowsService>(pNode);
		if(!pSvc) continue;
		m_AccessLog.DumpIngressTargets(Targets, Opts, pSvc->GetSvcTag());
	}
	Targets.Finish();
	
	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROC_TARGETS, Targets);
	Data.WriteVariant(API_V_PROC_ACTORS, Actors);
	Data.Finish();
	return Data;
}

void CProgramFile::AddAccess(const std::wstring& ActorServiceTag, const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked)
{
	if (!ActorServiceTag.empty()) {
		CWindowsServicePtr pActorService = GetService(ActorServiceTag);
		if (pActorService) {
			pActorService->AddAccess(Path, AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked);
			return;
		}
	}

	std::unique_lock lock(m_Mutex); 

	m_AccessTree.Add(Path, AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked);
}

CVariant CProgramFile::StoreAccess(const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex); 

	CVariant Data;
	Data[API_V_PROG_ID] = m_ID.ToVariant(Opts);
	Data[API_V_PROG_RESOURCE_ACCESS] = m_AccessTree.StoreTree(Opts);
	Data[API_V_PROG_EXEC_ACTORS] = m_AccessLog.StoreExecActors(Opts);
	Data[API_V_PROG_EXEC_TARGETS] = m_AccessLog.StoreExecTargets(Opts);
	Data[API_V_PROG_INGRESS_ACTORS] = m_AccessLog.StoreIngressActors(Opts);
	Data[API_V_PROG_INGRESS_TARGETS] = m_AccessLog.StoreIngressTargets(Opts);
	return Data;
}

void CProgramFile::LoadAccess(const CVariant& Data)
{
	std::unique_lock lock(m_Mutex);

	m_AccessTree.LoadTree(Data[API_V_PROG_RESOURCE_ACCESS]);
	m_AccessLog.LoadExecActors(Data[API_V_PROG_EXEC_ACTORS]);
	m_AccessLog.LoadExecTargets(Data[API_V_PROG_EXEC_TARGETS]);
	m_AccessLog.LoadIngressActors(Data[API_V_PROG_INGRESS_ACTORS]);
	m_AccessLog.LoadIngressTargets(Data[API_V_PROG_INGRESS_TARGETS]);
}

CVariant CProgramFile::DumpAccess(uint64 LastActivity) const
{
	std::unique_lock lock(m_Mutex); 

	return m_AccessTree.DumpTree(LastActivity);
}

CVariant CProgramFile::StoreTraffic(const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex);

	CVariant Data;
	Data[API_V_PROG_ID] = m_ID.ToVariant(Opts);
	Data[API_V_PROG_TRAFFIC] = m_TrafficLog.StoreTraffic(Opts);
	return Data;

}

void CProgramFile::LoadTraffic(const CVariant& Data)
{
	std::unique_lock lock(m_Mutex);

	m_TrafficLog.LoadTraffic(Data[API_V_PROG_TRAFFIC]);
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

void CProgramFile::ClearTraceLog()
{
	std::unique_lock lock(m_Mutex);

	for (int i = 0; i < (int)ETraceLogs::eLogMax; i++)
	{
		STraceLog& TraceLog = m_TraceLogs[i];

		TraceLog.IndexOffset = 0;
		TraceLog.Entries.clear();
	}
}

void CProgramFile::UpdateLastFwActivity(uint64 TimeStamp, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	if (bBlocked) {
		if (TimeStamp > m_LastFwBlocked)
			m_LastFwBlocked = TimeStamp;
	}
	else {
		if (TimeStamp > m_LastFwAllowed)
			m_LastFwAllowed = TimeStamp;
	}
}

void CProgramFile::CollectStats(SStats& Stats) const
{
	Stats.LastFwAllowed = m_LastFwAllowed;
	Stats.LastFwBlocked = m_LastFwBlocked;
	Stats.LastNetActivity = m_TrafficLog.GetLastActivity();
	Stats.Uploaded = m_TrafficLog.GetUploaded();
	Stats.Downloaded = m_TrafficLog.GetDownloaded();
	for (auto I : m_Processes) 
	{
		Stats.Pids.insert(I.first);

		if (I.second->GetLastNetActivity() > Stats.LastNetActivity)
			Stats.LastNetActivity = I.second->GetLastNetActivity();
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

void CProgramFile::ClearLogs()
{
	ClearTraceLog();

	m_TrafficLog.Clear();

	std::unique_lock lock(m_Mutex);

	m_AccessLog.Clear();

	m_AccessTree.Clear();
}