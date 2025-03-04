#include "pch.h"
#include "ProgramFile.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "../Library/Helpers/NtIo.h"
#include "../Library/Helpers/NtObj.h"
#include "../Library/Common/Strings.h"
#include "../Programs/ProgramManager.h"
#include "../ServiceCore.h"
#include "WindowsService.h"

CProgramFile::CProgramFile(const std::wstring& FileName)
{
	ASSERT(!FileName.empty());

	m_Path = FileName;
	m_ID = CProgramID(MkLower(m_Path), EProgramType::eProgramFile);

	SImageVersionInfoPtr pInfo = GetImageVersionInfo(m_Path);
	m_Name = pInfo ? pInfo->FileDescription : FileName;
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

std::wstring CProgramFile::GetNameEx() const
{ 
	std::unique_lock lock(m_Mutex); 
	if (!m_Name.empty())
		return m_Path + L" (" + m_Name + L")";
	return m_Path;
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
		if (pSvc && pSvc->GetServiceTag() == SvcTag)
			return pSvc;
	}
	return nullptr;
}

void CProgramFile::UpdateSignInfo(const struct SVerifierInfo* pVerifyInfo, bool bUpdateProcesses)
{
	std::unique_lock lock(m_Mutex);

	m_SignInfo.Update(pVerifyInfo);

	if (bUpdateProcesses) {
		for (auto& IProcess : m_Processes)
			IProcess.second->UpdateSignInfo(pVerifyInfo);
	}
}

bool CProgramFile::HashInfoUnknown() const
{
	std::unique_lock lock(m_Mutex);

	return m_SignInfo.GetHashStatus() == EHashStatus::eHashUnknown;
}

bool CProgramFile::HashInfoUnknown(const CProgramLibraryPtr& pLibrary) const
{
	std::unique_lock lock(m_Mutex);

	auto F = m_Libraries.find(pLibrary->GetUID());
	if (F == m_Libraries.end())
		return true;
	return F->second.SignInfo.GetHashStatus() == EHashStatus::eHashUnknown;
}

void CProgramFile__UpdateLibraryInfo(SLibraryInfo& Info, uint64 LoadTime, const struct SVerifierInfo* pVerifyInfo, EEventStatus Status)
{
	if (LoadTime) {
		Info.LastLoadTime = LoadTime;
		Info.TotalLoadCount++;
	}
	if (pVerifyInfo)
		Info.SignInfo.Update(pVerifyInfo);
	if (Status != EEventStatus::eUndefined)
		Info.LastStatus = Status;
}

void CProgramFile::AddLibrary(const CFlexGuid& EnclaveGuid, const CProgramLibraryPtr& pLibrary, uint64 LoadTime, const struct SVerifierInfo* pVerifyInfo, EEventStatus Status)
{
	std::unique_lock lock(m_Mutex);
	SLibraryInfo& Info = m_Libraries[pLibrary->GetUID()];
	CProgramFile__UpdateLibraryInfo(Info, LoadTime, pVerifyInfo, Status);

	if (!EnclaveGuid.IsNull()) {
		SEnclaveRecordPtr pRecord = GetEnclaveRecord(EnclaveGuid);
		SLibraryInfo& Info = pRecord->Libraries[pLibrary->GetUID()];
		CProgramFile__UpdateLibraryInfo(Info, LoadTime, pVerifyInfo, Status);
	}
}

CVariant CProgramFile::DumpLibraries() const
{
	std::unique_lock lock(m_Mutex);

	auto DumpLibrary = [](CVariant& List, uint64 Ref, const SLibraryInfo& Info, const CFlexGuid& EnclaveGuid) {
		CVariant vLib;
		vLib.BeginIMap();
		vLib.Write(API_V_LIB_REF, Ref);
		if (!EnclaveGuid.IsNull())
			vLib.WriteVariant(API_V_ENCLAVE, EnclaveGuid.ToVariant(false));
		vLib.Write(API_V_LIB_LOAD_TIME, Info.LastLoadTime);
		vLib.Write(API_V_LIB_LOAD_COUNT, Info.TotalLoadCount);
		vLib.Write(API_V_LIB_STATUS, (uint32)Info.LastStatus);
		vLib.WriteVariant(API_V_SIGN_INFO, Info.SignInfo.ToVariant(SVarWriteOpt()));
		vLib.Finish();
		List.WriteVariant(vLib);
	};



	CVariant List;
	List.BeginList();

	for (auto& Lib : m_Libraries)
		DumpLibrary(List, Lib.first, Lib.second, CFlexGuid());

	for (auto& Record : m_EnclaveRecord) {
		for (auto& Lib : Record.second->Libraries) 
			DumpLibrary(List, Lib.first, Lib.second, Record.first);
	}

	List.Finish();
	return List;
}

CVariant CProgramFile::StoreLibraries(const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex);

	std::map<uint64, std::list<std::pair<CFlexGuid, SLibraryInfo>>> Libraries;

	for (auto& Lib : m_Libraries) {
		Libraries[Lib.first].push_back(std::make_pair(CFlexGuid(), Lib.second));
	}

	for (auto& Enclave : m_EnclaveRecord)
	{
		for (auto& Lib : Enclave.second->Libraries) {
			Libraries[Lib.first].push_back(std::make_pair(Enclave.first, Lib.second));
		}
	}

	lock.unlock();

	CVariant List;
	List.BeginList();
	for (auto& Lib : Libraries) 
	{
		CVariant vLib;
		vLib.BeginMap();
		CProgramLibraryPtr pLib = theCore->ProgramManager()->GetLibrary(Lib.first);
		if(!pLib) continue;
		vLib.Write(API_S_FILE_PATH, pLib->GetPath());
		
		CVariant vLog;
		vLog.BeginList();

		for (auto& Entry : Lib.second)
		{
			CVariant vEntry;
			vEntry.BeginMap();
			if (!Entry.first.IsNull())
				vEntry.WriteVariant(API_S_ENCLAVE, Entry.first.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));
			vEntry.Write(API_S_LIB_LOAD_TIME, Entry.second.LastLoadTime);
			vEntry.Write(API_S_LIB_LOAD_COUNT, Entry.second.TotalLoadCount);
			vEntry.Write(API_S_LIB_STATUS, (uint32)Entry.second.LastStatus);
			vEntry.WriteVariant(API_S_SIGN_INFO, Entry.second.SignInfo.ToVariant(Opts));
			vEntry.Finish();
			vLog.WriteVariant(vEntry);
		}

		vLog.Finish();

		vLib.WriteVariant(API_S_LIB_LOAD_LOG, vLog);

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

		CVariant vLog = vData[API_S_LIB_LOAD_LOG];
		vLog.ReadRawList([&](const CVariant& vEntry) {

			CFlexGuid EnclaveGuid;
			EnclaveGuid.FromVariant(vEntry[API_S_ENCLAVE]);

			SLibraryInfo* pInfo;
			if (!EnclaveGuid.IsNull()){
				SEnclaveRecordPtr pRecord = GetEnclaveRecord(EnclaveGuid);
				pInfo = &pRecord->Libraries[pLib->GetUID()];
			} else
				pInfo = &m_Libraries[pLib->GetUID()];

			pInfo->LastLoadTime = vEntry[API_S_LIB_LOAD_TIME];
			pInfo->TotalLoadCount = vEntry[API_S_LIB_LOAD_COUNT];
			pInfo->LastStatus = (EEventStatus)vEntry[API_S_LIB_STATUS].To<uint32>();
			pInfo->SignInfo.FromVariant(vEntry[API_S_SIGN_INFO]);
		});
	});
}

void CProgramFile::CleanUpLibraries()
{
	std::unique_lock lock(m_Mutex);

	std::set<CProgramLibraryPtr> UsedLibraries;
	for (auto& IProcess : m_Processes)
		UsedLibraries.merge(IProcess.second->GetLibraries());

	for (auto I = m_Libraries.begin(); I != m_Libraries.end();)
	{
		if (UsedLibraries.find(theCore->ProgramManager()->GetLibrary(I->first)) == UsedLibraries.end())
			m_Libraries.erase(I++);
		else
			++I;
	}
}

CProgramFile::SEnclaveRecordPtr CProgramFile::GetEnclaveRecord(const CFlexGuid& EnclaveGuid)
{
	std::unique_lock lock(m_Mutex);
	SEnclaveRecordPtr& pRecord = m_EnclaveRecord[EnclaveGuid];
	if(!pRecord)
		pRecord = std::make_shared<SEnclaveRecord>();
	return pRecord;
}

void CProgramFile::AddExecParent(const CFlexGuid& TargetEnclave, 
	const std::shared_ptr<CProgramFile>& pActorProgram, const CFlexGuid& ActorEnclave, const SProcessUID& ProcessUID, const std::wstring& ActorServiceTag, 
	const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	CWindowsServicePtr pActorService = pActorProgram->GetService(ActorServiceTag);

	m_AccessLog.AddExecParent(pActorService ? pActorService->GetUID() : pActorProgram->GetUID(), ActorEnclave, ProcessUID, CmdLine, CreateTime, bBlocked);

	if (!TargetEnclave.IsNull()) {
		SEnclaveRecordPtr pRecord = GetEnclaveRecord(TargetEnclave);
		pRecord->AccessLog.AddExecParent(pActorService ? pActorService->GetUID() : pActorProgram->GetUID(), ActorEnclave, ProcessUID, CmdLine, CreateTime, bBlocked);
	}
}

void CProgramFile::AddExecChild(const CFlexGuid& ActorEnclave, const std::wstring& ActorServiceTag, 
	const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
	const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	CWindowsServicePtr pActorService = GetService(ActorServiceTag);
	if (pActorService) {
		pActorService->AddExecChild(pTargetProgram, TargetEnclave, ProcessUID, CmdLine, CreateTime, bBlocked);
		return;
	}

	m_AccessLog.AddExecChild(pTargetProgram->GetUID(), TargetEnclave, ProcessUID, CmdLine, CreateTime, bBlocked);

	if (!ActorEnclave.IsNull()) {
		SEnclaveRecordPtr pRecord = GetEnclaveRecord(ActorEnclave);
		pRecord->AccessLog.AddExecChild(pTargetProgram->GetUID(), TargetEnclave, ProcessUID, CmdLine, CreateTime, bBlocked);
	}
}

CVariant CProgramFile::DumpExecStats() const
{
	SVarWriteOpt Opts;

	CVariant ExecParents;
	ExecParents.BeginList();
	StoreExecParents(ExecParents, Opts);
	ExecParents.Finish();

	CVariant ExecChildren;
	ExecChildren.BeginList();
	StoreExecChildren(ExecChildren, Opts);

	for(auto& pNode : m_Nodes)
	{
		CWindowsServicePtr pSvc = std::dynamic_pointer_cast<CWindowsService>(pNode);
		if(!pSvc) continue;
		pSvc->StoreExecChildren(ExecChildren, Opts, pSvc->GetServiceTag());
	}
	ExecChildren.Finish();

	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROG_EXEC_PARENTS, ExecParents);
	Data.WriteVariant(API_V_PROG_EXEC_CHILDREN, ExecChildren);
	Data.Finish();
	return Data;
}

void CProgramFile::AddIngressActor(const CFlexGuid& TargetEnclave, 
	const std::shared_ptr<CProgramFile>& pActorProgram, const CFlexGuid& ActorEnclave, const SProcessUID& ProcessUID, const std::wstring& ActorServiceTag, 
	bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	CWindowsServicePtr pActorService = pActorProgram->GetService(ActorServiceTag);

	m_AccessLog.AddIngressActor(pActorService ? pActorService->GetUID() : pActorProgram->GetUID(), ActorEnclave, ProcessUID, bThread, AccessMask, AccessTime, bBlocked);

	if (!TargetEnclave.IsNull()) {
		SEnclaveRecordPtr pRecord = GetEnclaveRecord(TargetEnclave);
		pRecord->AccessLog.AddIngressActor(pActorService ? pActorService->GetUID() : pActorProgram->GetUID(), ActorEnclave, ProcessUID, bThread, AccessMask, AccessTime, bBlocked);
	}
}

void CProgramFile::AddIngressTarget(const CFlexGuid& ActorEnclave, const std::wstring& ActorServiceTag, 
	const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
	bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	CWindowsServicePtr pActorService = GetService(ActorServiceTag);
	if (pActorService) {
		pActorService->AddIngressTarget(pTargetProgram, TargetEnclave, ProcessUID, bThread, AccessMask, AccessTime, bBlocked);
		return;
	}

	m_AccessLog.AddIngressTarget(pTargetProgram->GetUID(), TargetEnclave, ProcessUID, bThread, AccessMask, AccessTime, bBlocked);

	if (!ActorEnclave.IsNull()) {
		SEnclaveRecordPtr pRecord = GetEnclaveRecord(ActorEnclave);
		pRecord->AccessLog.AddIngressTarget(pTargetProgram->GetUID(), TargetEnclave, ProcessUID, bThread, AccessMask, AccessTime, bBlocked);
	}
}

CVariant CProgramFile::DumpIngress() const
{
	SVarWriteOpt Opts;

	CVariant IngressActors;
	IngressActors.BeginList();
	StoreIngressActors(IngressActors, Opts);
	IngressActors.Finish();

	CVariant IngressTargets;
	IngressTargets.BeginList();
	StoreIngressTargets(IngressTargets, Opts);

	for(auto pNode : m_Nodes)
	{
		CWindowsServicePtr pSvc = std::dynamic_pointer_cast<CWindowsService>(pNode);
		if(!pSvc) continue;
		pSvc->StoreIngressTargets(IngressTargets, Opts, pSvc->GetServiceTag());
	}
	IngressTargets.Finish();
	
	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROG_INGRESS_ACTORS, IngressActors);
	Data.WriteVariant(API_V_PROG_INGRESS_TARGETS, IngressTargets);
	Data.Finish();
	return Data;
}

void CProgramFile::AddAccess(const std::wstring& ActorServiceTag, const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked)
{
	if (!theCore->Config()->GetBool("Service", "ResTrace", true))
		return;

	if (!ActorServiceTag.empty()) {
		CWindowsServicePtr pActorService = GetService(ActorServiceTag);
		if (pActorService) {
			pActorService->AddAccess(Path, AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked);
			return;
		}
	}

	m_AccessTree.Add(Path, AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked);
}

void CProgramFile::StoreExecParents(CVariant& ExecParents, const SVarWriteOpt& Opts) const
{
	m_AccessLog.StoreAllExecParents(ExecParents, CFlexGuid(), Opts);

	std::unique_lock lock(m_Mutex);
	auto EnclaveRecord = m_EnclaveRecord;
	lock.unlock();

	for (auto& Record : EnclaveRecord)
		Record.second->AccessLog.StoreAllExecParents(ExecParents, Record.first, Opts);
}

bool CProgramFile::LoadExecParents(const CVariant& ExecParents)
{
	ExecParents.ReadRawList([&](const CVariant& vData) {

		CFlexGuid EnclaveGuid;
		EnclaveGuid.FromVariant(vData.Find(API_V_EVENT_TARGET_EID));
		if (EnclaveGuid.IsNull())
			m_AccessLog.LoadExecParent(vData);
		else {
			SEnclaveRecordPtr pRecord = GetEnclaveRecord(EnclaveGuid);
			pRecord->AccessLog.LoadExecParent(vData);
		}
	});
	return true;
}

void CProgramFile::StoreExecChildren(CVariant& ExecChildren, const SVarWriteOpt& Opts) const
{
	m_AccessLog.StoreAllExecChildren(ExecChildren, CFlexGuid(), Opts);

	std::unique_lock lock(m_Mutex);
	auto EnclaveRecord = m_EnclaveRecord;
	lock.unlock();

	for (auto& Record : EnclaveRecord)
		Record.second->AccessLog.StoreAllExecChildren(ExecChildren, Record.first, Opts);
}

bool CProgramFile::LoadExecChildren(const CVariant& ExecChildren)
{
	ExecChildren.ReadRawList([&](const CVariant& vData) {

		CFlexGuid EnclaveGuid;
		EnclaveGuid.FromVariant(vData.Find(API_V_EVENT_ACTOR_EID));
		if (EnclaveGuid.IsNull())
			m_AccessLog.LoadExecChild(vData);
		else {
			SEnclaveRecordPtr pRecord = GetEnclaveRecord(EnclaveGuid);
			pRecord->AccessLog.LoadExecChild(vData);
		}
	});
	return true;
}

void CProgramFile::StoreIngressActors(CVariant& IngressActors, const SVarWriteOpt& Opts) const
{
	m_AccessLog.StoreAllIngressActors(IngressActors, CFlexGuid(), Opts);

	std::unique_lock lock(m_Mutex);
	auto EnclaveRecord = m_EnclaveRecord;
	lock.unlock();

	for (auto& Record : EnclaveRecord)
		Record.second->AccessLog.StoreAllIngressActors(IngressActors, Record.first, Opts);
}

bool CProgramFile::LoadIngressActors(const CVariant& IngressActors)
{
	IngressActors.ReadRawList([&](const CVariant& vData) {

		CFlexGuid EnclaveGuid;
		EnclaveGuid.FromVariant(vData.Find(API_V_EVENT_TARGET_EID));
		if (EnclaveGuid.IsNull())
			m_AccessLog.LoadIngressActor(vData);
		else {
			SEnclaveRecordPtr pRecord = GetEnclaveRecord(EnclaveGuid);
			pRecord->AccessLog.LoadIngressActor(vData);
		}
	});
	return true;
}

void CProgramFile::StoreIngressTargets(CVariant& IngressTargets, const SVarWriteOpt& Opts) const
{
	m_AccessLog.StoreAllIngressTargets(IngressTargets, CFlexGuid(), Opts);

	std::unique_lock lock(m_Mutex);
	auto EnclaveRecord = m_EnclaveRecord;
	lock.unlock();

	for (auto& Record : EnclaveRecord)
		Record.second->AccessLog.StoreAllIngressTargets(IngressTargets, Record.first, Opts);
}

bool CProgramFile::LoadIngressTargets(const CVariant& IngressTargets)
{
	IngressTargets.ReadRawList([&](const CVariant& vData) {

		CFlexGuid EnclaveGuid;
		EnclaveGuid.FromVariant(vData.Find(API_V_EVENT_ACTOR_EID));
		if (EnclaveGuid.IsNull())
			m_AccessLog.LoadIngressTarget(vData);
		else {
			SEnclaveRecordPtr pRecord = GetEnclaveRecord(EnclaveGuid);
			pRecord->AccessLog.LoadIngressTarget(vData);
		}
	});
	return true;
}

CVariant CProgramFile::StoreIngress(const SVarWriteOpt& Opts) const
{
	CVariant ExecParents;
	ExecParents.BeginList();
	StoreExecParents(ExecParents, Opts);
	ExecParents.Finish();

	CVariant ExecChildren;
	ExecChildren.BeginList();
	StoreExecChildren(ExecChildren, Opts);
	ExecChildren.Finish();

	CVariant IngressActors;
	IngressActors.BeginList();
	StoreIngressActors(IngressActors, Opts);
	IngressActors.Finish();

	CVariant IngressTargets;
	IngressTargets.BeginList();
	StoreIngressTargets(IngressTargets, Opts);
	IngressTargets.Finish();

	CVariant Data;
	Data[API_V_PROG_ID] = m_ID.ToVariant(Opts);
	Data[API_V_PROG_EXEC_PARENTS] = ExecParents;
	Data[API_V_PROG_EXEC_CHILDREN] = ExecChildren;
	Data[API_V_PROG_INGRESS_ACTORS] = IngressActors;
	Data[API_V_PROG_INGRESS_TARGETS] = IngressTargets;
	return Data;
}

void CProgramFile::LoadIngress(const CVariant& Data)
{
	LoadExecParents(Data[API_V_PROG_EXEC_PARENTS]);
	LoadExecChildren(Data[API_V_PROG_EXEC_CHILDREN]);
	LoadIngressActors(Data[API_V_PROG_INGRESS_ACTORS]);
	LoadIngressTargets(Data[API_V_PROG_INGRESS_TARGETS]);
}

CVariant CProgramFile::StoreAccess(const SVarWriteOpt& Opts) const
{
	CVariant Data;
	Data[API_V_PROG_ID] = m_ID.ToVariant(Opts);
	Data[API_V_PROG_RESOURCE_ACCESS] = m_AccessTree.StoreTree(Opts);
	return Data;
}

void CProgramFile::LoadAccess(const CVariant& Data)
{
	m_AccessTree.LoadTree(Data[API_V_PROG_RESOURCE_ACCESS]);
}

CVariant CProgramFile::DumpResAccess(uint64 LastActivity) const
{
	return m_AccessTree.DumpTree(LastActivity);
}

CVariant CProgramFile::StoreTraffic(const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex);

	CVariant Data;
	Data[API_V_PROG_ID] = m_ID.ToVariant(Opts);
	Data[API_V_TRAFFIC_LOG] = m_TrafficLog.StoreTraffic(Opts);
	return Data;
}

void CProgramFile::LoadTraffic(const CVariant& Data)
{
	std::unique_lock lock(m_Mutex);

	m_TrafficLog.LoadTraffic(Data[API_V_TRAFFIC_LOG]);
}

uint64 CProgramFile::AddTraceLogEntry(const CTraceLogEntryPtr& pLogEntry, ETraceLogs Log)
{
	std::unique_lock lock(m_Mutex); 

	switch (Log)
	{
	case ETraceLogs::eExecLog:
		if (!theCore->Config()->GetBool("Service", "ExecLog", true))
			return -1;
		break;
	case ETraceLogs::eResLog:
		if (!theCore->Config()->GetBool("Service", "ResLog", true))
			return -1;
		break;
	case ETraceLogs::eNetLog:
		if (!theCore->Config()->GetBool("Service", "NetLog", true))
			return -1;
		break;
	}

	STraceLog& TraceLog = m_TraceLogs[(int)Log];
	TraceLog.Entries.push_back(pLogEntry);
	return TraceLog.IndexOffset + TraceLog.Entries.size() - 1;
}

CProgramFile::STraceLog CProgramFile::GetTraceLog(ETraceLogs Log) const
{ 
	std::unique_lock lock(m_Mutex); 

	return m_TraceLogs[(int)Log];
}

void CProgramFile::TruncateTraceLog()
{
	std::unique_lock lock(m_Mutex); 

	size_t CleanupCount = theCore->Config()->GetUInt64("Service", "TraceLogRetentionCount", 10000); // keep last 10000 entries by default
	uint64 CleanupDateMinutes = theCore->Config()->GetUInt64("Service", "TraceLogRetentionMinutes", 60 * 24 * 14); // default 14 days
	uint64 CleanupDate = GetCurrentTimeAsFileTime() - (CleanupDateMinutes * 60 * 10000000ULL);

	for (int i = 0; i < (int)ETraceLogs::eLogMax; i++)
	{
		STraceLog& TraceLog = m_TraceLogs[i];

		//
		// check if we exceed the maximum number of entries
		//
		size_t pos = 0;
		if(TraceLog.Entries.size() > CleanupCount)
			pos = TraceLog.Entries.size() - CleanupCount;

		//
		// find the first entry older than CleanupDate
		// note: the entries are sorted by timestamp
		//
		auto it = std::lower_bound(TraceLog.Entries.begin(), TraceLog.Entries.end(), CleanupDate, [](const CTraceLogEntryPtr& entry, uint64 CleanupData) {
			return entry->GetTimeStamp() < CleanupData;
		});

		//
		// check if entrys by date require us to clean up more entries than by count alone
		// 
		if (it != TraceLog.Entries.begin()) {
			size_t pos2 = std::distance(TraceLog.Entries.begin(), it);
			if (pos2 > pos)
				pos = pos2;
		}

		//
		// remove the entries
		//
		if(pos > 0){
			TraceLog.IndexOffset += pos;
			TraceLog.Entries.erase(TraceLog.Entries.begin(), TraceLog.Entries.begin() + pos);
		}
	}
}

void CProgramFile::ClearTraceLog(ETraceLogs Log)
{
	std::unique_lock lock(m_Mutex);

	if (Log == ETraceLogs::eLogMax)
	{
		for (int i = 0; i < (int)ETraceLogs::eLogMax; i++)
		{
			STraceLog& TraceLog = m_TraceLogs[i];

			TraceLog.IndexOffset = 0;
			TraceLog.Entries.clear();
		}
	}
	else
	{
		STraceLog& TraceLog = m_TraceLogs[(int)Log];

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
	Stats.LastNetActivity = m_TrafficLog.GetLastActivity();
	Stats.Uploaded = m_TrafficLog.GetUploaded();
	Stats.Downloaded = m_TrafficLog.GetDownloaded();
	for (auto I : m_Processes) 
	{
		Stats.Pids.insert(I.first);

		uint64 LastActivity = FILETIME2ms(I.second->GetLastNetActivity());
		if (LastActivity > Stats.LastNetActivity)
			Stats.LastNetActivity = LastActivity;
		Stats.Upload += I.second->GetUpload();
		Stats.Download += I.second->GetDownload();
		// Note: this is not the same as the sum of the sockets' upload/download, as it includes data from closed sockets
		//Stats.Uploaded += I.second->GetUploaded();
		//Stats.Downloaded += I.second->GetDownloaded();

		std::set<CSocketPtr> Sockets = I.second->GetSocketList();
		for (auto pSocket : Sockets) 
		{
			Stats.SocketRefs.insert((uint64)pSocket.get());

			if (pSocket->GetLastActivity() > Stats.LastNetActivity)
				Stats.LastNetActivity = pSocket->GetLastActivity();
			//Stats.Upload += pSocket->GetUpload();
			//Stats.Download += pSocket->GetDownload();
			Stats.Uploaded += pSocket->GetUploaded();
			Stats.Downloaded += pSocket->GetDownloaded();
		}
	}
}

void CProgramFile::ClearLogs(ETraceLogs Log)
{
	ClearTraceLog(Log);

	if(Log == ETraceLogs::eLogMax || Log == ETraceLogs::eNetLog)
		m_TrafficLog.Clear();

	if (Log == ETraceLogs::eLogMax || Log == ETraceLogs::eExecLog) {
		m_AccessLog.Clear();
		m_LastExec = 0;
	}

	if(Log == ETraceLogs::eLogMax || Log == ETraceLogs::eResLog)
		m_AccessTree.Clear();
}

void CProgramFile::TruncateAccessLog()
{
	m_AccessLog.Truncate();
}

void CProgramFile::CleanUpAccessTree(bool* pbCancel, uint32* puCounter)
{
	m_AccessTree.CleanUp(pbCancel, puCounter);
}

void CProgramFile::TruncateAccessTree()
{
	m_AccessTree.Truncate();
}

void CProgramFile::TestMissing()
{
	std::wstring NtPath = CNtPathMgr::Instance()->TranslateDosToNtPath(m_Path);
	m_IsMissing = (!NtPath.empty() && NtIo_FileExists(SNtObject(NtPath).Get())) ? ePresent : eMissing;
}