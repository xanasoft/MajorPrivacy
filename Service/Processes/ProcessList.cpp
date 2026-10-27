#include "pch.h"

#include <Shlwapi.h>

#include "ProcessList.h"
#include "../Library/Common/DbgHelp.h"
#include "ServiceCore.h"
#include "../Library/API/DriverAPI.h"
#include "../Etw/EtwEventMonitor.h"
#include "../Library/Helpers/EvtUtil.h"
#include "../Library/API/PrivacyAPI.h"
#include "ServiceList.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Programs/ProgramManager.h"
#include "../Library/Common/Strings.h"
#include "../Library/IPC/PipeServer.h"
#include "../Access/ResLogEntry.h"
#include "../Access/AccessManager.h"
#include "../Library/Hooking/HookUtils.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Helpers/CertUtil.h"
#include "../Enclaves/EnclaveManager.h"
#include "../Library/Helpers/WinUtil.h"
#include "../Library/Helpers/MiscHelpers.h"
#include "../Programs/WindowsService.h"
#include "../Network/Socket.h"

extern "C" {
#include "../../Driver/KSI/include/kphmsg.h"
}

#define API_INGRESS_RECORD_FILE_NAME L"IngressRecord.dat"
#define API_INGRESS_RECORD_FILE_VERSION 1

#define API_USAGE_RECORD_FILE_NAME L"UsageRecord.dat"
#define API_USAGE_RECORD_FILE_VERSION 1

CProcessList::CProcessList()
{
    m_Services = new CServiceList(m_Mutex);
}

CProcessList::~CProcessList()
{
    delete m_Services;
}

DWORD CALLBACK CProcessList__LoadProc(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
    MySetThreadDescription(GetCurrentThread(), L"CProcessList__LoadProc");
#endif

    CProcessList* This = (CProcessList*)lpThreadParameter;

    uint64 uStart = GetTickCount64();
    STATUS Status = This->Load();
    DbgPrint(L"CProcessList::Load() took %llu ms\n", GetTickCount64() - uStart);

    NtClose(This->m_hStoreThread);
    This->m_hStoreThread = NULL;
    return 0;
}

STATUS CProcessList::Init()
{
    STATUS Status;

    static bool bInitInject = true;
    if (bInitInject) {
        ULONG err = InitInject();
        if (err)
            theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to initialize LowLevel Inject mechanism err %d.", err);
        bInitInject = false;
    }

    theCore->Driver()->RegisterProcessHandler(&CProcessList::OnProcessDrvEvent, this);
    uint32 uEvents = KphMsgProcessCreate | KphMsgProcessExit | KphMsgUntrustedImage | KphMsgImageLoad | KphMsgProcAccess | KphMsgThreadAccess | KphMsgAccessFile;
    if(theCore->Config()->GetBool("Service", "LogRegistry", false))
        uEvents |= KphMsgAccessReg;
    uEvents |= KphMsgInjectDll;
    Status = theCore->Driver()->RegisterForProcesses(uEvents, true);
    if (Status.IsError()) 
    {
        //
        // When the driver registration fails we fall back to using ETW
        //

        theCore->Log()->LogEvent(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to register with driver for process creation notifications.");

        theCore->EtwEventMonitor()->RegisterProcessHandler(&CProcessList::OnProcessEtwEvent, this);
        Status = OK;
    }

    ULONGLONG uStart = GetTickCount64();
    Update();
    DbgPrint(L"First CProcessList::Update() took %llu ms\n", GetTickCount64() - uStart);

    m_hStoreThread = CreateThread(NULL, 0, CProcessList__LoadProc, (void*)this, 0, NULL);

    return Status;
}

DWORD CALLBACK CProcessList__StoreProc(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
    MySetThreadDescription(GetCurrentThread(), L"CProcessList__StoreProc");
#endif

    CProcessList* This = (CProcessList*)lpThreadParameter;

    uint64 uStart = GetTickCount64();
    STATUS Status = This->Store();
    DbgPrint(L"CProcessList::Store() took %llu ms\n", GetTickCount64() - uStart);

    NtClose(This->m_hStoreThread);
    This->m_hStoreThread = NULL;
    return 0;
}

STATUS CProcessList::StoreAsync()
{
    if (m_hStoreThread) return STATUS_ALREADY_COMMITTED;
    m_hStoreThread = CreateThread(NULL, 0, CProcessList__StoreProc, (void*)this, 0, NULL);
    return STATUS_SUCCESS;
}

void CProcessList::Update()
{
    m_bLogNotFound = theCore->Config()->GetBool("Service", "LogNotFound", false);
    m_bLogInvalid = theCore->Config()->GetBool("Service", "LogInvalid", false);
	m_bLogPipes = theCore->Config()->GetBool("Service", "LogPipes", false);

    //uint64 uStart = GetTickCount64();
    EnumProcesses();
	//DbgPrint(L"CProcessList::EnumProcesses() took %llu ms\n", GetTickCount64() - uStart);

	//uStart = GetTickCount64();
    m_Services->EnumServices();

    UpdateUsage();
	//DbgPrint(L"CProcessList::EnumServices() took %llu ms\n", GetTickCount64() - uStart);
}

void CProcessList::Reconfigure()
{
    bool bRegister = theCore->Config()->GetBool("Service", "LogRegistry", false);
    STATUS Status = theCore->Driver()->RegisterForProcesses(KphMsgAccessReg, bRegister);
    if (Status.IsError()) 
        theCore->Log()->LogEvent(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to register with driver for process notifications.");
}

STATUS CProcessList::EnumProcesses()
{
#ifdef _DEBUG
    uint64 start = GetTickCount64();
#endif

    //
    // The snapshot of what we had is taken first and under its own short lock, so
    // that a process which starts while we are talking to the driver below is not
    // in it - it will simply not be in the driver's list either, and must not be
    // taken for one that has gone.
    //
    std::unique_lock Lock(m_Mutex);

    std::map<SProcessUID, CProcessPtr> OldList(m_ProcessMap);

    Lock.unlock();

    //
    // A process is enlisted and given its program under the lock, so that it is
    // never visible without one - see AddProcessImpl. Reading the libraries it has
    // loaded is left until afterwards: that opens the process and walks its whole
    // address space a region at a time, hundreds of system calls, and holding the
    // lock through it stalled every event thread trying to read the list.
    //
    std::vector<CProcessPtr> NewProcesses;

    if (theCore->Driver()->IsConnected())
    {
        auto Pids = theCore->Driver()->EnumProcesses();
        if(Pids.IsError())
            return Pids.GetStatus();

        //
        // Asked for unlocked, all of it, before anything is touched.
        //
        // This used to be done with the list lock held: a few hundred round trips
        // into the kernel inside one critical section, four times a second. Every
        // driver event arriving in that window queued behind it, and they were
        // found doing exactly that - waiting in GetProcessEx for this very lock,
        // in as many samples as they were found running.
        //
        // SProcessInfo carries no pid of its own, so it is kept alongside.
        std::vector<std::pair<uint64, SProcessInfoPtr> > Infos;
        Infos.reserve(Pids.GetValue()->size());

        for (uint64 Pid : *Pids.GetValue())
        {
            if (Pid == 0)
                continue; // skip Idle Process

            auto Result = theCore->Driver()->GetProcessInfo(Pid);
            if(Result.IsError())
				continue; // process already terminated

            Infos.push_back(std::make_pair(Pid, Result.GetValue()));
        }

        Lock.lock();

        for (const auto& I : Infos)
        {
            const uint64 Pid = I.first;
            const SProcessInfoPtr& pInfo = I.second;

            SProcessUID UID(Pid, FILETIME2ms(pInfo->CreateTime));

            auto F = OldList.find(UID);
            CProcessPtr pProcess;
            if (F != OldList.end()) 
            {
                pProcess = F->second;
                OldList.erase(F);
                if(pProcess->GetParentId() == -1) // reinitialize process info if we had a partial init before
					pProcess->Init(pInfo.get());
            }
            else
            {
                pProcess = CProcessPtr(new CProcess(Pid));
                pProcess->Init(pInfo.get());
                AddProcessImpl(pProcess);
                NewProcesses.push_back(pProcess);
            }

            pProcess->Update(pInfo.get());	
        }
    }
    else
    {
        //
        // Unlocked as well: this walks every process in the kernel and fills a
        // buffer, and touches nothing of ours while it does.
        //
        std::vector<BYTE> Processes;
        //MyQuerySystemInformation(Processes, SystemExtendedProcessInformation);
        bool bFullProcessInfo = true;
        NTSTATUS status = MyQuerySystemInformation(Processes, SystemFullProcessInformation); // requires win 8 and admin
        if (!NT_SUCCESS(status)) {
            bFullProcessInfo = false;
            status = MyQuerySystemInformation(Processes, SystemProcessInformation); // fallback win 7
        }
        if (!NT_SUCCESS(status))
            return ERR(status);

        Lock.lock();

        for (PSYSTEM_PROCESS_INFORMATION process = PH_FIRST_PROCESS(Processes.data()); process != NULL; process = PH_NEXT_PROCESS(process))
        {
            uint64 Pid = (uint64)process->UniqueProcessId;

            if (Pid == 0)
                continue; // skip Idle Process

            SProcessUID UID(Pid, FILETIME2ms(process->CreateTime.QuadPart));

            auto F = OldList.find(UID);
            CProcessPtr pProcess;
            if (F != OldList.end()) {
                pProcess = F->second;
                OldList.erase(F);
            }
            else
            {
                pProcess = CProcessPtr(new CProcess(Pid));
                pProcess->Init(process, bFullProcessInfo);

                AddProcessImpl(pProcess);
                NewProcesses.push_back(pProcess);
            }

            pProcess->Update(process, bFullProcessInfo);
        }
    }

    Lock.unlock();

    for (const CProcessPtr& pNew : NewProcesses)
        LoadProcessLibs(pNew);

    for (auto E : OldList)
    {
        CProcessPtr pProcess = E.second;

        if (pProcess->CanBeRemoved() && pProcess->GetSocketCount() == 0) // keep processes around as long as we have the acompanying sockets
        {
            Lock.lock();
            RemoveProcessUnsafe(pProcess);
			Lock.unlock();
            theCore->ProgramManager()->RemoveProcess(pProcess);
        }
        else if (!pProcess->IsMarkedForRemoval())
            pProcess->MarkForRemoval();
    }

#ifdef _DEBUG
    //DbgPrint("EnumProcesses took %dms\r\n", GetTickCount64() - start);
#endif

	return OK;
}

void CProcessList::AddProcessImpl(const CProcessPtr& pProcess)
{
    AddProcessUnsafe(pProcess);
    theCore->ProgramManager()->AddProcess(pProcess);
}

//
// The slow part of taking on a new process, kept out of the lock: InitLibs opens
// the process and walks its whole address space a region at a time. The program is
// already assigned by the time this runs, see AddProcessImpl.
//
void CProcessList::LoadProcessLibs(const CProcessPtr& pProcess)
{
    pProcess->InitLibs();

    CProgramFilePtr pProgram = pProcess->GetProgram();
    if (pProgram && pProcess->HashInfoUnknown()) {
        if (!pProgram->HashInfoUnknown())
            pProcess->SetSignInfo(pProgram->GetSignInfo());
        //else
        //{
        //    theCore->ThreadPool()->enqueueTask([pProcess, pProgram](const std::wstring& ModulePath) {
        //        SVerifierInfo VerifyInfo;
        //        CProcess::FillVerifyInfo(ModulePath, VerifyInfo);
        //        pProcess->UpdateSignInfo(&VerifyInfo);
        //        pProgram->UpdateSignInfo(&VerifyInfo);
        //    }, pProgram->GetPath());
        //}
    }

    //if (!theCore->AppIsolator()->AllowProcessStart(pProcess)) {
    //    TerminateProcess(Pid, pProcess->GetFileName());
    //    continue;
    //}
}

//
// Looks a process up by pid, taking a pid collision - the number handed on to
// something else entirely - for a miss. The lock must be held.
//
// The map is ordered by pid and then by creation time, so the entry just below the
// pid's upper bound is the newest process that ever had the number. A terminated
// process stays listed while it owns sockets, and a new process may already carry
// its pid; the newest is the one that is, or was last, running under it. With an
// event time the bound moves down to it: the newest process created by then.
//
CProcessPtr CProcessList::FindByPidUnsafe(uint64 Pid, const std::wstring& FileNameNt, bool bLogCollision, uint64 NotAfterMs)
{
    auto I = m_ProcessMap.upper_bound(SProcessUID(Pid, NotAfterMs ? NotAfterMs : 0x000003FFFFFFFFFFull));
    if (I == m_ProcessMap.begin())
        return NULL;
    --I;
    const CProcessPtr& pProcess = I->second;
    if (pProcess->m_Pid != Pid)
        return NULL;

    if (Pid != NT_OS_KERNEL_PID && !FileNameNt.empty() && !pProcess->IsSameImage(FileNameNt)) {
        if (bLogCollision)
            theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_PID_COLISION, L"Found Process PID collision detected: %s (%u) vs %s (%u).", pProcess->GetNtFilePath().c_str(), Pid, FileNameNt.c_str(), Pid);
        return NULL;
    }

    return pProcess;
}

//
// Builds a process from what the driver knows about it, and tells the caller
// whether it is really there. Takes no lock of its own - see the two callers.
//
CProcessPtr CProcessList::MakeProcess(uint64 Pid, const std::wstring& FileNameNt, SProcessInfoPtr& Info, bool& bOk, uint64 NotAfterMs)
{
    CProcessPtr pProcess = CProcessPtr(new CProcess(Pid));
    auto Result = theCore->Driver()->GetProcessInfo(Pid);
    Info = Result.GetValue();
    bOk = pProcess->Init(Info.get());

    //
    // What the driver has under the pid is not the one meant when it runs another image, or
    // when it was created after the event being attributed (the pid was handed on since).
    //
    bool bOther = false;
    if (Pid != NT_OS_KERNEL_PID && !FileNameNt.empty() && !pProcess->IsSameImage(FileNameNt)) {
        theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_PID_COLISION, L"New Process PID collision detected: %s (%u) vs %s (%u).", pProcess->m_NtFilePath.c_str(), Pid, FileNameNt.c_str(), Pid);
        bOther = true;
    }
    else if (bOk && NotAfterMs && pProcess->GetCreateTimeStamp() > NotAfterMs)
        bOther = true;

    if (bOther) {
        // Create dummy
        pProcess = CProcessPtr(new CProcess(Pid));
        pProcess->m_NtFilePath = FileNameNt;
        pProcess->m_Name = GetFileNameFromPath(pProcess->m_NtFilePath);

        bOk = false;
    }

    //
    // A placeholder for something that is not running (or that the driver could not tell
    // about) must not shadow a real process of the same pid: the lookup takes the newest
    // creation time, so it gets none.
    //
    if (!bOk)
        pProcess->m_CreateTimeStamp = 0;

    return pProcess;
}

//
// Enlists what MakeProcess built. The lock must be held.
//
CProcessPtr CProcessList::InsertProcessUnsafe(const CProcessPtr& pProcess, bool bOk, const SProcessInfoPtr& Info, EGetMode Mode)
{
    if (bOk)
    {
        AddProcessUnsafe(pProcess);
        theCore->ProgramManager()->AddProcess(pProcess);

        //if (!theCore->AppIsolator()->AllowProcessStart(pProcess))
        //    TerminateProcess(Pid, pProcess->GetFileName());

        pProcess->Update(Info.get());
    }
    else 
    {
        // Process already terminated
        if (Mode != eMustAdd)
            return nullptr;

		m_ProcessMap.insert(std::make_pair(pProcess->GetProcessUID(), pProcess));
    }
    
	return pProcess; 
}

//
// For callers that already hold the lock, which is why the driver is asked with it
// held - there is no way around that from in here. The paths that come this way are
// rare: a service being linked to its process, and a process being told about its
// own parent. See CServiceList::SetProcessUnsafe and CProcessList::OnProcessStarted.
//
CProcessPtr CProcessList::GetProcessExUnsafe(uint64 Pid, const std::wstring& FileNameNt, EGetMode Mode, uint64 NotAfterMs)
{
    CProcessPtr pFound = FindByPidUnsafe(Pid, FileNameNt, true, NotAfterMs);
    if (pFound && !pFound->IsMarkedForRemoval())
        return pFound;

    if (Mode == eCanNotAdd)
        return pFound;

    SProcessInfoPtr Info;
    bool bOk = false;
    CProcessPtr pProcess = MakeProcess(Pid, FileNameNt, Info, bOk, NotAfterMs);

    return ResolveProcessUnsafe(pFound, pProcess, bOk, Info, Mode);
}

//
// What a lookup found may be a process that has terminated, kept for its sockets,
// while the pid has since been handed on. The driver was asked what runs under the
// pid now; this decides between the two. The lock must be held.
//
CProcessPtr CProcessList::ResolveProcessUnsafe(const CProcessPtr& pFound, const CProcessPtr& pProcess, bool bOk, const SProcessInfoPtr& Info, EGetMode Mode)
{
    if (pFound)
    {
        if (!bOk) // nothing runs under the pid, the terminated process is the best answer
            return pFound;

        if (pFound->GetProcessUID() == pProcess->GetProcessUID()) // the same process, still there
            return pFound;
    }

    return InsertProcessUnsafe(pProcess, bOk, Info, Mode);
}

CProcessPtr CProcessList::GetProcessEx(uint64 Pid, const std::wstring& FileNameNt, EGetMode Mode, uint64 NotAfterMs)
{
    //
    // The common case by far, and all that most callers ever need: the process is
    // already known and this is one lookup.
    //
    {
        //
        // Shared: this only reads the map, and while it was exclusive every event
        // thread doing the same lookup excluded all the others.
        //
        std::shared_lock Lock(m_Mutex);

        CProcessPtr pFound = FindByPidUnsafe(Pid, FileNameNt, true, NotAfterMs);
        if (pFound && !pFound->IsMarkedForRemoval())
            return pFound;

        //
        // Known only as terminated, or not at all. Without the driver that is the
        // answer; with it, the pid may have been handed on since, so ask below.
        //
        if (Mode == eCanNotAdd)
            return pFound;
    }

    //
    // Asking the driver is a round trip into the kernel, and it used to be made with
    // the list lock held - on the driver event path, so every other event coming in
    // waited behind it. Done here with nothing held; if two threads ask about the
    // same process at once they both do the work and the second one throws its
    // answer away below, which is cheaper than everyone queueing for the first.
    //
    SProcessInfoPtr Info;
    bool bOk = false;
    CProcessPtr pProcess = MakeProcess(Pid, FileNameNt, Info, bOk, NotAfterMs);

    std::unique_lock Lock(m_Mutex);

    //
    // Looked up again: another event thread may have gone through all of the above
    // while we were in the kernel, and there must only ever be one of each process.
    // Not logged this time, the first look already did that.
    //
    CProcessPtr pFound = FindByPidUnsafe(Pid, FileNameNt, false, NotAfterMs);
    if (pFound && !pFound->IsMarkedForRemoval())
        return pFound;

    return ResolveProcessUnsafe(pFound, pProcess, bOk, Info, Mode);
}

void CProcessList::AddProcessUnsafe(const CProcessPtr& pProcess)
{
    m_ProcessMap.insert(std::make_pair(pProcess->GetProcessUID(), pProcess));

    std::wstring AppContainerSID = pProcess->GetAppContainerSid();
    if (!AppContainerSID.empty())
        m_Apps.insert(std::make_pair(MkLower(AppContainerSID), pProcess));
    std::wstring FilePath = pProcess->GetNtFilePath();
    ASSERT(!FilePath.empty());
    m_ByPath.insert(std::make_pair(theCore->NormalizePath(FilePath), pProcess));
}

void CProcessList::RemoveProcessUnsafe(const CProcessPtr& pProcess)
{
    m_ProcessMap.erase(pProcess->GetProcessUID());


    std::wstring AppContainerSID = pProcess->GetAppContainerSid();
    if (!AppContainerSID.empty())
        mmap_erase(m_Apps, MkLower(AppContainerSID), pProcess);
    std::wstring FilePath = pProcess->GetNtFilePath();
    ASSERT(!FilePath.empty())
    mmap_erase(m_ByPath, theCore->NormalizePath(FilePath), pProcess);
}

std::map<SProcessUID, CProcessPtr> CProcessList::FindProcesses(const CProgramID& ID)
{
	std::shared_lock Lock(m_Mutex);	// reads only

	std::map<SProcessUID, CProcessPtr> Found;

	switch (ID.GetType())
	{
	    case EProgramType::eProgramFile:
        {
            for (auto I = m_ByPath.find(ID.GetFilePath()); I != m_ByPath.end() && I->first == ID.GetFilePath(); ++I) {
                Found.insert(std::make_pair(I->second->GetProcessUID(), I->second));
            }
            break;
        }
	    case EProgramType::eWindowsService:
        {
            //
            // Unsafe: the lock is held here, and it is the one the service list
            // shares with us.
            //
            auto pService = m_Services->GetServiceUnsafe(ID.GetServiceTag());
            if (!pService || !pService->ProcessId)
                break;
            //
            // Unsafe: FindProcesses holds the lock.
            //
            CProcessPtr pProcess = GetProcessUnsafe(pService->ProcessId);
            if (!pProcess)
                break;
            
            Found.insert(std::make_pair(pProcess->GetProcessUID(), pProcess));
            break;
        }
	    case EProgramType::eAppPackage:
        {
            for (auto I = m_Apps.find(ID.GetAppContainerSid()); I != m_Apps.end() && I->first == ID.GetAppContainerSid(); ++I) {
                Found.insert(std::make_pair(I->second->GetProcessUID(), I->second));
            }
            break;
        }
	    case EProgramType::eAllPrograms:
		    return m_ProcessMap;
	}
	return Found;
}

//void CProcessList::TerminateProcess(uint64 Pid, const std::wstring& FileName)
//{
//    theCore->Log()->LogEvent(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_BLOCK_PROC, StrLine(L"Terminating blocked process: %s (%u).", FileName.c_str(), Pid));
//    CScopedHandle ProcessHandle = CScopedHandle(OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)Pid), CloseHandle);
//    if (!ProcessHandle || !NT_SUCCESS(NtTerminateProcess(ProcessHandle, -1)))
//        theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_KILL_FAILED, StrLine(L"Failed to terminate blocked process: %s (%u)!", FileName.c_str(), Pid));
//}

std::wstring FindInPath(const std::wstring& filePath)
{
    if (filePath.length() > 2 * MAX_PATH - 1)
        return std::wstring();

    wchar_t pathValue[2 * MAX_PATH] = { 0 };
    wcscpy_s(pathValue, filePath.c_str());
    if (PathFindOnPathW(pathValue, NULL))
        return std::wstring(pathValue);
    return std::wstring();
}

std::wstring FindExe(const std::wstring& curPath)
{
    if (std::filesystem::exists(curPath))
        return curPath;
    if (std::filesystem::exists(curPath + L".exe"))
        return curPath + L".exe";
    return L"";
}

std::wstring CProcessList::GetPathFromCmd(const std::wstring &CommandLine, const std::wstring &FileName, uint64 ParentID)
{
    if (CommandLine.empty())
        return std::wstring();

    std::wstring filePath = GetFileFromCommand(CommandLine);

    std::wstring curPath = FindExe(filePath);
    if (!curPath.empty())
        return curPath;

    // in some cases the command line does not contain the file name at all
    if (_wcsicmp(GetFileNameFromPath(filePath).c_str(), FileName.c_str()) != 0 
     && _wcsicmp((GetFileNameFromPath(filePath) + L".exe").c_str(), FileName.c_str()) != 0)
        filePath = FileName;

    if (filePath.find(L"\\??\\") == 0)
        filePath = filePath.substr(4);

    filePath = ExpandEnvStrings(filePath);

    if (!PathIsRelativeW(filePath.c_str()))
        return filePath;

    std::shared_lock Lock(m_Mutex);	// reads only

    // check relative paths based on the parrent process working directory
    std::wstring workingDir;
    if (CProcessPtr pParent = FindByPidUnsafe(ParentID, L"", false))
        workingDir = pParent->GetWorkDir();
    if (!workingDir.empty())
    {
        std::wstring curPath = FindExe(std::filesystem::absolute(workingDir + L"\\" + filePath));
        if (!curPath.empty())
            return curPath;
    }

    // find exe in path
    std::wstring fullPath = FindInPath(filePath);
    if (!fullPath.empty())
        return fullPath;
    return FindInPath(filePath + L".exe");
}

EProgramOnSpawn CProcessList::GetAllowStart(uint64 Pid, uint64 ParentPid, uint64 ActorPid, const std::wstring& ActorServiceTag, /*const std::wstring& EnclaveId, */const std::wstring& NtPath, const std::wstring& Command, const struct SVerifierInfo* pVerifyInfo, uint64 CreateTime, const CFlexGuid& RuleGuid, const CFlexGuid& EnclaveGuid, EEventStatus Status, uint32 TimeOut)
{
    EProgramOnSpawn Action = EProgramOnSpawn::eUnknown;

	std::wstring EnclaveId = EnclaveGuid.ToWString();
	bool bTrusted = (Status != EEventStatus::eUntrusted);

    if (!RuleGuid.IsNull())
    {
        CProgramRulePtr pRule = theCore->ProgramManager()->GetRule(RuleGuid);
        if (!pRule)
            theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_RULE_NOT_FOUND, L"Program rule not found: %s", RuleGuid.ToString().c_str());
        else if (pRule->HasScript())
        {
			auto pScript = pRule->GetScriptEngine();
            if(pScript)
                Action = pScript->RunStartScript(NtPath, Command, ActorPid, ActorServiceTag, EnclaveId, bTrusted, pVerifyInfo);
        }
    }

    if (Action == EProgramOnSpawn::eUnknown && !EnclaveGuid.IsNull())
    {
        CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(EnclaveGuid);
        if (!pEnclave)
            theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_RULE_NOT_FOUND, L"Enclave not found: %s", EnclaveGuid.ToString().c_str());
        else if (pEnclave->HasScript())
        {
            auto pScript = pEnclave->GetScriptEngine();
            if (pScript) {
                EProgramOnSpawn EnclaveAction = pScript->RunStartScript(NtPath, Command, ActorPid, ActorServiceTag, EnclaveId, bTrusted, pVerifyInfo);
                if (EnclaveAction != EProgramOnSpawn::eUnknown)
                    Action = EnclaveAction;
            }
        }
    }

    return Action;
}

bool CProcessList::OnProcessStarted(uint64 Pid, uint64 ParentPid, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, const std::wstring& NtPath, const std::wstring& Command, const struct SVerifierInfo* pVerifyInfo, uint64 CreateTime, EEventStatus Status, bool bETW)
{
    CProcessPtr pChildProcess = GetProcess(Pid, true);

    //
    // Note: It's possible to receive an event for a process before the OnProcessStarted is called.
    // Therefore, we need to update the process information here to ensure it stays in sync with the drivers protection status.
    //
    // This situation can occur, for instance, if a task manager detects the process and opens it before the process creation is fully completed.
    //
    pChildProcess->Update();

    if (!pChildProcess->IsInitDone()) {

        std::unique_lock Lock(pChildProcess->m_Mutex);

        bool bInitDone = pChildProcess->m_ParentPid != -1;
        pChildProcess->m_ParentPid = ParentPid;

        if (pChildProcess->m_CreationTime == 0)
            pChildProcess->SetRawCreationTime(CreateTime);

        if (bETW)
        {
            //
            // ETW returns only the actual file name not file path
            // we try to recover the full path from the command line
            //

            pChildProcess->m_Name = NtPath;

            std::wstring FilePath = GetPathFromCmd(Command, NtPath, ParentPid);
            if (!FilePath.empty()) {
                std::wstring NtPath = CNtPathMgr::Instance()->TranslateDosToNtPath(FilePath);
                if(NtPath.empty()) // fallback
                    pChildProcess->m_NtFilePath = L"\\??\\" + FilePath;
                else
                    pChildProcess->m_NtFilePath = NtPath;
            }
        }
        else
            pChildProcess->m_NtFilePath = NtPath;

        if (pChildProcess->m_CommandLine.empty())
            pChildProcess->m_CommandLine = Command;

        Lock.unlock();

        if (!bInitDone) {
            std::unique_lock Lock(m_Mutex);

            AddProcessUnsafe(pChildProcess); // Note: at this point its already listed in m_List but as its not a multi map its fine
            theCore->ProgramManager()->AddProcess(pChildProcess);
        }
    }

    CProgramFilePtr pProgram = pChildProcess->GetProgram();

    if (pVerifyInfo) 
    {
        //if (pVerifyInfo && (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_CI) && pVerifyInfo->SignPolicyBits == 0)
        //{
			// Eider the file is not signed or the signature is not valid
            //bool Valid;
            //bool Present = GetEmbeddedSignatureStatus(pProgram->GetPath(), &test1);
        //}

        pChildProcess->UpdateSignInfo(pVerifyInfo);
        if (pProgram) 
        {
            pProgram->UpdateSignInfo(pVerifyInfo);

            if (Status == EEventStatus::eProtected)
            {
                StVariant Data;
                Data[API_V_ID] = pProgram->GetID().ToVariant(SVarWriteOpt());
                Data[API_V_NAME] = pProgram->GetNameEx();
				Data[API_V_SIGN_FLAGS] = pVerifyInfo->StatusFlags;
                theCore->EmitEvent(ELogLevels::eWarning, eLogProgramBlocked, Data);
            }
        }
    }

    //
    // No lock is held at this point - the one above is scoped to the block it is
    // declared in - so this is the ordinary locking form.
    //
    CProcessPtr pParentProcess = GetProcess(ActorPid, true);
    CProgramFilePtr pActorProgram = pParentProcess ? pParentProcess->GetProgram() : NULL;
    if (pProgram && pActorProgram)
    {
		CFlexGuid TargetEnclave = pChildProcess->GetEnclave(); // same as EnclaveId from event
		CFlexGuid ActorEnclave = pParentProcess->GetEnclave();

        pProgram->AddExecParent(TargetEnclave, 
            pActorProgram, ActorEnclave, pParentProcess->GetProcessUID(), ActorServiceTag, 
            Command, CreateTime, (Status != EEventStatus::eAllowed));
        pActorProgram->AddExecChild(ActorEnclave, ActorServiceTag, 
            pProgram, TargetEnclave, pChildProcess->GetProcessUID(), 
            Command, CreateTime, (Status != EEventStatus::eAllowed));

#ifdef DEF_USE_POOL
		CExecLogEntryPtr pLogActorEntry = pActorProgram->Allocator()->New<CExecLogEntry>(ActorEnclave, EExecLogRole::eActor, EExecLogType::eProcessStarted, Status, pProgram->GetUID(), TargetEnclave, ActorServiceTag, CreateTime, Pid, pVerifyInfo);
#else
        CExecLogEntryPtr pLogActorEntry = CExecLogEntryPtr(new CExecLogEntry(ActorEnclave, EExecLogRole::eActor, EExecLogType::eProcessStarted, Status, pProgram->GetUID(), TargetEnclave, ActorServiceTag, CreateTime, Pid, pVerifyInfo));
#endif
        AddExecLogEntry(pActorProgram, pLogActorEntry);

#ifdef DEF_USE_POOL
		CExecLogEntryPtr pLogEntry = pProgram->Allocator()->New<CExecLogEntry>(TargetEnclave, EExecLogRole::eTarget, EExecLogType::eProcessStarted, Status, pActorProgram->GetUID(), ActorEnclave, ActorServiceTag, CreateTime, Pid, pVerifyInfo);
#else
        CExecLogEntryPtr pLogEntry = CExecLogEntryPtr(new CExecLogEntry(TargetEnclave, EExecLogRole::eTarget, EExecLogType::eProcessStarted, Status, pActorProgram->GetUID(), ActorEnclave, ActorServiceTag, CreateTime, Pid, pVerifyInfo));
#endif
        AddExecLogEntry(pProgram, pLogEntry);
    }

    //return theCore->AppIsolator()->AllowProcessStart(pChildProcess);
    return true;
}

void CProcessList::OnProcessStopped(uint64 Pid, uint32 ExitCode)
{
	std::unique_lock Lock(m_Mutex);

    CProcessPtr pProcess = FindByPidUnsafe(Pid, L"", false);
    if (!pProcess)
        return;

    if (!pProcess->IsMarkedForRemoval())
	    pProcess->MarkForRemoval();
}

void CProcessList::OnProcessAccessed(uint64 Pid, uint64 ActorPid, const std::wstring& ActorServiceTag, bool bThread, uint32 AccessMask, uint64 AccessTime, EEventStatus Status)
{
    CProcessPtr pTargetProcess = GetProcess(Pid, true);
    CProgramFilePtr pTargetProgram = pTargetProcess ? pTargetProcess->GetProgram() : NULL;


    CProcessPtr pActorProcess = GetProcess(ActorPid, true);
    CProgramFilePtr pActorProgram = pActorProcess ? pActorProcess->GetProgram() : NULL;

    if (!pTargetProgram || !pActorProgram || pTargetProgram == pActorProgram) // discard self accesses
        return;

    //
	// Record Interprocess Access Events 
    // the Actor is the process executing the access
	// the Target is the process being accessed
    // 
    // A Program item records IngressActors i.e. a list of processes which accessed it
	// As well as IngressTargets i.e. a list of programs it accessed
    //

    CFlexGuid TargetEnclave = pTargetProcess->GetEnclave();
    CFlexGuid ActorEnclave = pActorProcess->GetEnclave();

    pTargetProgram->AddIngressActor(TargetEnclave, 
        pActorProgram, ActorEnclave, pActorProcess->GetProcessUID(), ActorServiceTag, 
        bThread, AccessMask, AccessTime, (Status != EEventStatus::eAllowed));
    pActorProgram->AddIngressTarget(ActorEnclave, ActorServiceTag, 
        pTargetProgram, TargetEnclave, pTargetProcess->GetProcessUID(), 
        bThread, AccessMask, AccessTime, (Status != EEventStatus::eAllowed));

#ifdef DEF_USE_POOL
	CExecLogEntryPtr pLogActorEntry = pActorProgram->Allocator()->New<CExecLogEntry>(TargetEnclave, EExecLogRole::eActor, bThread ? EExecLogType::eThreadAccess : EExecLogType::eProcessAccess, Status, pActorProgram->GetUID(), ActorEnclave, ActorServiceTag, AccessTime, Pid, nullptr, AccessMask);
#else
    CExecLogEntryPtr pLogActorEntry = CExecLogEntryPtr(new CExecLogEntry(ActorEnclave, EExecLogRole::eActor, bThread ? EExecLogType::eThreadAccess : EExecLogType::eProcessAccess, Status, pTargetProgram->GetUID(), TargetEnclave, ActorServiceTag, AccessTime, Pid, nullptr, AccessMask));
#endif
    AddExecLogEntry(pActorProgram, pLogActorEntry);

#ifdef DEF_USE_POOL
	CExecLogEntryPtr pLogEntry = pTargetProgram->Allocator()->New<CExecLogEntry>(TargetEnclave, EExecLogRole::eTarget, bThread ? EExecLogType::eThreadAccess : EExecLogType::eProcessAccess, Status, pActorProgram->GetUID(), ActorEnclave, ActorServiceTag, AccessTime, Pid, nullptr, AccessMask);
#else
    CExecLogEntryPtr pLogEntry = CExecLogEntryPtr(new CExecLogEntry(TargetEnclave, EExecLogRole::eTarget, bThread ? EExecLogType::eThreadAccess : EExecLogType::eProcessAccess, Status, pActorProgram->GetUID(), ActorEnclave, ActorServiceTag, AccessTime, Pid, nullptr, AccessMask));
#endif
    AddExecLogEntry(pTargetProgram, pLogEntry);
}

EImageOnLoad CProcessList::GetAllowImage(const struct SProcessImageEvent* pImageEvent)
{
    EImageOnLoad Action = EImageOnLoad::eUnknown;

    //CProcessPtr pProcess = GetProcess(pImageEvent->ProcessId, true);
    //std::wstring ProgramPath = pProcess ? theCore->NormalizePath(pProcess->GetNtFilePath(), false) : L"";
    //std::wstring ModulePath = theCore->NormalizePath(pImageEvent->FileName, false);
    bool bTrusted = (pImageEvent->Status == EEventStatus::eAllowed);

    if (!pImageEvent->RuleGuid.empty())
    {
        CProgramRulePtr pRule = theCore->ProgramManager()->GetRule(pImageEvent->RuleGuid);
        if (!pRule)
            theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_RULE_NOT_FOUND, L"Program rule not found: %s", pImageEvent->RuleGuid.c_str());
        else if (pRule->HasScript())
        {
            auto pScript = pRule->GetScriptEngine();
            if(pScript)
                Action = pScript->RunLoadScript(pImageEvent->ProcessId, pImageEvent->FileName, pImageEvent->EnclaveId, bTrusted, &pImageEvent->VerifierInfo);
        }
    }

    if (Action == EImageOnLoad::eUnknown && !pImageEvent->EnclaveGuid.empty())
    {
        CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(pImageEvent->EnclaveGuid);
        if (!pEnclave)
            theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_RULE_NOT_FOUND, L"Enclave not found: %s", pImageEvent->EnclaveGuid.c_str());
        else if (pEnclave->HasScript())
        {
            auto pScript = pEnclave->GetScriptEngine();
            if (pScript) {
                EImageOnLoad EnclaveAction = pScript->RunLoadScript(pImageEvent->ProcessId, pImageEvent->FileName, pImageEvent->EnclaveId, bTrusted, &pImageEvent->VerifierInfo);
                if (EnclaveAction != EImageOnLoad::eUnknown)
                    Action = EnclaveAction;
            }
        }
    }

    return Action;
}

void CProcessList::OnImageEvent(const SProcessImageEvent* pImageEvent)
{
	CProcessPtr pProcess = GetProcess(pImageEvent->ProcessId, true);
    if(!pProcess)
        return;

    std::wstring ProgramPath = theCore->NormalizePath(pProcess->GetNtFilePath(), false);
    std::wstring ModulePath = theCore->NormalizePath(pImageEvent->FileName, false);

    bool bIsProcess = MkLower(ProgramPath) == MkLower(ModulePath);
    if (bIsProcess)
        pProcess->UpdateSignInfo(&pImageEvent->VerifierInfo);

    CProgramFilePtr pProgram = pProcess->GetProgram();
    if (!pProgram)
        return;

    if (bIsProcess) {
        pProgram->UpdateSignInfo(&pImageEvent->VerifierInfo);
        return;
    }

    CProgramLibraryPtr pLibrary = theCore->ProgramManager()->GetLibrary(ModulePath, true);
	pLibrary->UpdateSignInfo(&pImageEvent->VerifierInfo);

    //CProcessPtr pActorProcess = GetProcess(ActorPid, true);
    //CProgramFilePtr pActorProgram = pActorProcess ? pActorProcess->GetProgram() : NULL;
    
#ifdef _DEBUG
    if(pImageEvent->ProcessId != pImageEvent->ActorProcessId)
	{
		DbgPrint("CProcessList::OnImageEvent: Pid != ActorPid\r\n");
	}
#endif

    // todo:

    //
    // Note: an image may receive a loaded event followed by an unloaded event
    // this means the library was unloaded before control was returned to user space
    // in which case we handle it as if it would have been unloaded instantly.
    // Note: this should no longer be the case, we now issue the event once all is checked and final
    //

    CFlexGuid EnclaveGuid = pProcess->GetEnclave();

    pProcess->AddLibrary(pLibrary);
    pProgram->AddLibrary(EnclaveGuid, pLibrary, pImageEvent->TimeStamp, &pImageEvent->VerifierInfo, pImageEvent->Status);

#ifdef DEF_USE_POOL
	CExecLogEntryPtr pLogEntry = pProgram->Allocator()->New<CExecLogEntry>(EnclaveGuid, EExecLogRole::eBoth, EExecLogType::eImageLoad, pImageEvent->Status, pLibrary->GetUID(), CFlexGuid(), pImageEvent->ActorServiceTag, pImageEvent->TimeStamp, pImageEvent->ProcessId, &pImageEvent->VerifierInfo);
#else
    CExecLogEntryPtr pLogEntry = CExecLogEntryPtr(new CExecLogEntry(EnclaveGuid, EExecLogRole::eBoth, EExecLogType::eImageLoad, pImageEvent->Status, pLibrary->GetUID(), CFlexGuid(), pImageEvent->ActorServiceTag, pImageEvent->TimeStamp, pImageEvent->ProcessId, &pImageEvent->VerifierInfo));
#endif
    AddExecLogEntry(pProgram, pLogEntry);
}

EAccessRuleType CProcessList::GetResourceAccess(const std::wstring& NtPath, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, uint32 AccessMask, uint64 AccessTime, const CFlexGuid& RuleGuid, EEventStatus Status, uint32 TimeOut)
{
    if (RuleGuid.IsNull()) // this should not happen
		return EAccessRuleType::eNone;
    
    CAccessRulePtr pRule = theCore->AccessManager()->GetRule(RuleGuid);
    if (!pRule) {
        theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_RULE_NOT_FOUND, L"Access rule not found: %s", RuleGuid.ToString().c_str());
        return EAccessRuleType::eNone;
    }
    
    if(pRule->HasScript())
    {
        auto pScript = pRule->GetScriptEngine();
        if (pScript) {
            EAccessRuleType Action = pScript->RunAccessScript(NtPath, ActorPid, ActorServiceTag, EnclaveId, AccessMask);
            if (Action != EAccessRuleType::eNone)
                return Action;
        }
    }

    if(!pRule->IsInteractive())
		return EAccessRuleType::eNone;

    //
	// Rule is interactive, emit event and wait for response from UI
    //

    CProcessPtr pActorProcess = GetProcess(ActorPid, true);
    CProgramFilePtr pActorProgram = pActorProcess ? pActorProcess->GetProgram() : NULL;

    if (!pActorProgram)
        return EAccessRuleType::eNone;

#ifdef DEF_USE_POOL
    CResLogEntryPtr pLogEntry = pActorProgram->Allocator()->New<CResLogEntry>(pActorProcess->GetEnclave(), NtPath, pActorProcess->GetUserSid(), ActorServiceTag, AccessMask, EEventStatus::eProtected, AccessTime, ActorPid);
#else
    CResLogEntryPtr pLogEntry = CResLogEntryPtr(new CResLogEntry(pActorProcess->GetEnclave(), NtPath, pActorProcess->GetUserSid(), ActorServiceTag, AccessMask, EEventStatus::eProtected, AccessTime, ActorPid));
#endif

    uint64 LogIndex = pActorProgram->AddTraceLogEntry(pLogEntry, ETraceLogs::eResLog);

    StVariant Event;
    //Event[SVC_API_EVENT_TYPE]	= SVC_API_EVENT_FW_EVENT;
    Event[API_V_ID]		        = pActorProgram->GetID().ToVariant(SVarWriteOpt());
	Event[API_V_EVENT_TIMEOUT]  = TimeOut;
    Event[API_V_EVENT_INDEX]	= LogIndex;
    Event[API_V_EVENT_DATA]	    = pLogEntry->ToVariant();

    if (theCore->BroadcastMessage(SVC_API_EVENT_RES_ACTIVITY, Event) == 0)
        return EAccessRuleType::eNone; // no cleints to make a decision

    std::unique_lock WaitLock(m_WaitingEventsMutex);

    SWaitingEvent WaitingEvent;
    uint64 EventId = (uint64)pLogEntry.Get();
    m_WaitingEvents.insert(std::make_pair(EventId, &WaitingEvent));
    
    WaitingEvent.cv.wait_for(WaitLock, std::chrono::milliseconds(TimeOut), [&WaitingEvent] { return WaitingEvent.Action != EAccessRuleType::eNone; });

    m_WaitingEvents.erase(EventId);

    return WaitingEvent.Action;
}

STATUS CProcessList::SetAccessEventAction(uint64 EventId, EAccessRuleType Action)
{
    std::unique_lock WaitLock(m_WaitingEventsMutex);
    auto F = m_WaitingEvents.find(EventId);
    if (F != m_WaitingEvents.end()) {
        F->second->Action = Action;
        F->second->cv.notify_one();
        return STATUS_SUCCESS;
    }
	return STATUS_NOT_FOUND;
}

void CProcessList::OnResourceAccessed(const std::wstring& NtPath, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, uint32 AccessMask, uint64 AccessTime, const CFlexGuid& RuleGuid, EEventStatus Status, NTSTATUS NtStatus, bool IsDirectory)
{
	if (!m_bLogNotFound && (NtStatus == STATUS_UNRECOGNIZED_VOLUME 
        || NtStatus == STATUS_OBJECT_NAME_NOT_FOUND || NtStatus == STATUS_OBJECT_PATH_NOT_FOUND
        || NtStatus == STATUS_BAD_NETWORK_PATH || NtStatus == STATUS_BAD_NETWORK_NAME
      ))
		return; // ignore not found errors
    if (!m_bLogInvalid && (NtStatus == STATUS_INVALID_PARAMETER || NtStatus == STATUS_OBJECT_TYPE_MISMATCH
        || NtStatus == STATUS_OBJECT_NAME_INVALID || NtStatus == STATUS_OBJECT_PATH_INVALID || NtStatus == STATUS_OBJECT_PATH_SYNTAX_BAD
      ))
        return; // ignore not found errors

	if (MatchPathPrefix(NtPath, L"\\device\\mup\\;lanmanredirector") || MatchPathPrefix(NtPath, L"\\device\\mup\\;webdavredirector"))
		return; // ignore network redirectors

	if (!m_bLogPipes && MatchPathPrefix(NtPath, L"\\Device\\NamedPipe"))
		return; // ignore named pipes

    CProcessPtr pActorProcess = GetProcess(ActorPid, true);
    CProgramFilePtr pActorProgram = pActorProcess ? pActorProcess->GetProgram() : NULL;

    if (!pActorProgram)
        return;

    std::wstring DosPath = theCore->NormalizePath(NtPath);

    pActorProgram->AddAccess(ActorServiceTag, DosPath, AccessMask, AccessTime, NtStatus, IsDirectory, (Status != EEventStatus::eAllowed));

#ifdef DEF_USE_POOL
	CResLogEntryPtr pLogEntry = pActorProgram->Allocator()->New<CResLogEntry>(pActorProcess->GetEnclave(), NtPath, pActorProcess->GetUserSid(), ActorServiceTag, AccessMask, Status, AccessTime, ActorPid);
#else
    CResLogEntryPtr pLogEntry = CResLogEntryPtr(new CResLogEntry(pActorProcess->GetEnclave(), NtPath, pActorProcess->GetUserSid(), ActorServiceTag, AccessMask, Status, AccessTime, ActorPid));
#endif
    
    uint64 LogIndex = pActorProgram->AddTraceLogEntry(pLogEntry, ETraceLogs::eResLog);

    StVariant Event;
    //Event[SVC_API_EVENT_TYPE]	= SVC_API_EVENT_FW_EVENT;
    Event[API_V_ID]		= pActorProgram->GetID().ToVariant(SVarWriteOpt());
    Event[API_V_EVENT_INDEX]	= LogIndex;
    Event[API_V_EVENT_DATA]	    = pLogEntry->ToVariant();

    if(pLogEntry->GetStatus() != EEventStatus::eAllowed)
        theCore->QueueBroadcast(SVC_API_EVENT_RES_ACTIVITY, Event); // always breadcast suspiciosue activity
    else
        theCore->QueueBroadcast(SVC_API_EVENT_RES_ACTIVITY, Event, pActorProgram);
}

void CProcessList::AddExecLogEntry(const std::shared_ptr<CProgramFile>& pProgram, const CExecLogEntryPtr& pLogEntry)
{
    uint64 LogIndex = pProgram->AddTraceLogEntry(pLogEntry, ETraceLogs::eExecLog);

    StVariant Event;
    //Event[SVC_API_EVENT_TYPE]	= SVC_API_EVENT_FW_EVENT;
    Event[API_V_ID]		= pProgram->GetID().ToVariant(SVarWriteOpt());
    Event[API_V_EVENT_INDEX]	= LogIndex;
    Event[API_V_EVENT_DATA]	    = pLogEntry->ToVariant();

    if(pLogEntry->GetStatus() != EEventStatus::eAllowed)
		theCore->QueueBroadcast(SVC_API_EVENT_EXEC_ACTIVITY, Event); // always breadcast suspiciosue activity
	else
        theCore->QueueBroadcast(SVC_API_EVENT_EXEC_ACTIVITY, Event, pProgram);
}

ULONG InjectDllLow(uint64 Pid, ULONG flags)
{
    const ULONG ProcessAccess =
        PROCESS_DUP_HANDLE | PROCESS_TERMINATE | PROCESS_SUSPEND_RESUME
        | PROCESS_SET_INFORMATION | PROCESS_QUERY_INFORMATION
        | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE;

    CScopedHandle hProcess = CScopedHandle(OpenProcess(ProcessAccess, FALSE, (DWORD)Pid), CloseHandle);

    ULONG err = InjectLdr(hProcess, 0);
    if (err)
        theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"InjectDllLow failed err %d, for PID %d.", err, Pid);

    return err;
}

struct SInjectDllHigParams
{
    uint64 Pid;
	CScopedHandle<HANDLE, BOOL (__stdcall*)(HANDLE)> hThread;
    ULONG flags;
};

DWORD CALLBACK InjectDllHighFunc(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
MySetThreadDescription(GetCurrentThread(), L"InjectDllHighFunc");
#endif
 
    CScoped<SInjectDllHigParams> params = (SInjectDllHigParams*)lpThreadParameter;
	
    const ULONG ProcessAccess =
        PROCESS_DUP_HANDLE | PROCESS_TERMINATE | PROCESS_SUSPEND_RESUME |
        PROCESS_SET_INFORMATION | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE;

    CScopedHandle hProcess = CScopedHandle(OpenProcess(ProcessAccess, FALSE, (DWORD)params->Pid), CloseHandle);

    DWORD ret =  InjectDll(hProcess, params->hThread, params->flags);

    if (ret)
        theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"InjectDllHigh failed err %d, for PID %d.", ret, params->Pid);

    ResumeThread(params->hThread);

    return ret;
}

ULONG InjectDllHigh(uint64 Pid, ULONG flags)
{
    //
	// Note: NtGetContextThread called here locks untill the driver callback finishes, so we need to suspend the thread and return
	// we will resume the thread from a helper thread after we finished the injection process.
	// As each thread has a suspend count, we are not at risk of the thread being resumed by the creating process before we are done.
    // 
    // Also note that we can not use our drivers thread tracking as it also only fires after the process creation callback finishes.
    //

    uint64 Tid = 0;

    std::vector<BYTE> Processes;
    NTSTATUS status = MyQuerySystemInformation(Processes, SystemProcessInformation);
    if (NT_SUCCESS(status))
    {
        for (PSYSTEM_PROCESS_INFORMATION process = PH_FIRST_PROCESS(Processes.data()); process != NULL; process = PH_NEXT_PROCESS(process))
        {
            if (process->UniqueProcessId != (HANDLE)Pid)
                continue;

            ASSERT(process->NumberOfThreads == 1);
            _SYSTEM_THREAD_INFORMATION* Thread = &process->Threads[0];
            Tid = (uint64)Thread->ClientId.UniqueThread;
            break;
        }
    }

    const ULONG ThreadAccess =
        THREAD_SUSPEND_RESUME |
        THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | 
        THREAD_QUERY_INFORMATION | THREAD_SET_INFORMATION;

    HANDLE hThread = OpenThread(ThreadAccess, FALSE, (DWORD)Tid);
    if (hThread == INVALID_HANDLE_VALUE) {
        //DWORD err = GetLastError();
        return -1;
    }

	SuspendThread(hThread);

    SInjectDllHigParams* params = new SInjectDllHigParams();
	params->Pid = Pid;
	params->hThread.Set(hThread, CloseHandle);
	params->flags = flags;

    HANDLE hHelper = CreateThread(NULL, 0, InjectDllHighFunc, (void*)params, 0, NULL);
    CloseHandle(hHelper);

    return 0;
}

NTSTATUS CProcessList::OnInjectionRequest(uint64 Pid, EExecDllMode DllMode, const CFlexGuid& EnclaveGuid, const CFlexGuid& RuleGuid)
{
    //CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(EnclaveGuid);
    //if (!pEnclave) {
    //    theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_RULE_NOT_FOUND, L"Enclave not found: %s", EnclaveGuid.ToString().c_str());
    //    return STATUS_NOT_FOUND;
    //}

    //std::shared_ptr<std::vector<uint64>> ThreadIDs = theCore->Driver()->GetThreadIDs(Pid).GetValue();
    //if(ThreadIDs->empty())
    //return STATUS_PROCESS_IS_TERMINATING;
    //uint64 Tid = ThreadIDs->front();

    switch (DllMode) {
    case EExecDllMode::eInjectLow:  InjectDllLow(Pid, 0);   break;
    case EExecDllMode::eInjectHigh: InjectDllHigh(Pid, 0);  break;
    }

	return STATUS_SUCCESS;
}

NTSTATUS CProcessList::OnProcessDrvEvent(const SProcessEvent* pEvent)
{
    switch (pEvent->Type)
    {
        case SProcessEvent::EType::ProcessStarted: 
        {
            SProcessStartEvent* pStartEvent = (SProcessStartEvent*)(pEvent);
            if (pStartEvent->TimeOut != 0)
				pStartEvent->Action = GetAllowStart(pStartEvent->ProcessId, pStartEvent->ParentId, pStartEvent->ActorProcessId, pStartEvent->ActorServiceTag, /*pStartEvent->EnclaveId, */pStartEvent->NtPath, pStartEvent->CommandLine, &pStartEvent->VerifierInfo, pStartEvent->TimeStamp, pStartEvent->RuleGuid, pStartEvent->EnclaveGuid, pStartEvent->Status, pStartEvent->TimeOut);
            else
                OnProcessStarted(pStartEvent->ProcessId, pStartEvent->ParentId, pStartEvent->ActorProcessId, pStartEvent->ActorServiceTag, pStartEvent->EnclaveId, pStartEvent->NtPath, pStartEvent->CommandLine, &pStartEvent->VerifierInfo, pStartEvent->TimeStamp, pStartEvent->Status);

            //
            // Note: this triggers before the process is started, during early process initialization
            // returnign an error could cancel the process creation, if we uncoment the code in the driver and driver interface
            //
#ifdef _DEBUG
            //DbgPrint("CProcessList::OnProcessDrvEvent (%d) %S\r\n", pEvent->ProcessId, pEvent->CommandLine.c_str());
#endif
            //if (!OnProcessStarted(pEvent->ProcessId, pStartEvent->ParentId, pStartEvent->FileName, pStartEvent->CommandLine, pStartEvent->CreateTime)) 
            //{
            //    theCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_BLOCK_PROC, StrLine(L"Preventing start of blocked process: %s (%u).", pStartEvent->FileName, pEvent->ProcessId));
            //    return STATUS_ACCESS_DENIED;
            //}
            break;
        }
        case SProcessEvent::EType::ProcessStopped:
        {
			const SProcessStopEvent* pStopEvent = (SProcessStopEvent*)(pEvent);
            OnProcessStopped(pStopEvent->ProcessId, pStopEvent->ExitCode);
            break;
		}
        case SProcessEvent::EType::ImageLoad:
        {
            SProcessImageEvent* pImageEvent = (SProcessImageEvent*)(pEvent);
            if (pImageEvent->TimeOut != 0)
                pImageEvent->Action = GetAllowImage(pImageEvent);
            else
                OnImageEvent(pImageEvent);
            break;
		}
        case SProcessEvent::EType::ProcessAccess:
        case SProcessEvent::EType::ThreadAccess:
		{
			SProcessAccessEvent* pAccessEvent = (SProcessAccessEvent*)(pEvent);
            if(pAccessEvent->bCreating)
                break; // don't log events which are part of regular process creation process
            if (pAccessEvent->TimeOut != 0)
                ;//pAccessEvent->Action = EAccessRuleType::eNone; // todo <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
            else
                OnProcessAccessed(pAccessEvent->ProcessId, pAccessEvent->ActorProcessId, pAccessEvent->ActorServiceTag, pEvent->Type == SProcessEvent::EType::ThreadAccess, pAccessEvent->AccessMask, pAccessEvent->TimeStamp, pAccessEvent->Status);
			break;
		}
        case SProcessEvent::EType::ResourceAccess:
        {
            SResourceAccessEvent* pAccessEvent = (SResourceAccessEvent*)(pEvent);
            if (pAccessEvent->TimeOut != 0)
                pAccessEvent->Action = GetResourceAccess(pAccessEvent->Path, pAccessEvent->ActorProcessId, pAccessEvent->ActorServiceTag, pAccessEvent->EnclaveId, pAccessEvent->AccessMask, pAccessEvent->TimeStamp, pAccessEvent->RuleGuid, pAccessEvent->Status, pAccessEvent->TimeOut);
            else
			    OnResourceAccessed(pAccessEvent->Path, pAccessEvent->ActorProcessId, pAccessEvent->ActorServiceTag, pAccessEvent->EnclaveId, pAccessEvent->AccessMask, pAccessEvent->TimeStamp, pAccessEvent->RuleGuid, pAccessEvent->Status, pAccessEvent->NtStatus, pAccessEvent->IsDirectory);
			break;
        }
		case SProcessEvent::EType::InjectionRequest:
		{
			const SInjectionRequest* pInjectEvent = (SInjectionRequest*)(pEvent);
			return OnInjectionRequest(pInjectEvent->ProcessId, pInjectEvent->DllInject, pInjectEvent->EnclaveId, pInjectEvent->RuleGuid);
		}
    }

    return STATUS_SUCCESS;
}

void CProcessList::OnProcessEtwEvent(const struct SEtwProcessEvent* pEvent)
{
    if (pEvent->Type != SEtwProcessEvent::EType::Started)
    {
        OnProcessStopped(pEvent->ProcessId, pEvent->ExitCode);
        return;
    }

    //
    // Note: this triggers with a significant delay of a few seconds !!!
    //

#ifdef _DEBUG
    //DbgPrint("CProcessList::OnProcessEtwEvent (%d) %S\r\n", pEvent->ProcessId, pEvent->CommandLine.c_str());
#endif

    OnProcessStarted(pEvent->ProcessId, pEvent->ParentId, pEvent->ParentId, L"", L"", pEvent->FileName, pEvent->CommandLine, NULL, pEvent->TimeStamp, EEventStatus::eAllowed, true);
    //if (!OnProcessStarted(pEvent->ProcessId, pEvent->ParentId, pEvent->FileName, pEvent->CommandLine, pEvent->TimeStamp, true))
    //    TerminateProcess(pEvent->ProcessId, pEvent->FileName);
}

//////////////////////////////////////////////////////////////////////////
// Load/Store

STATUS CProcessList::LoadIngress()
{
    CBuffer Buffer;
    if (!ReadFile(theCore->GetDataFolder() + L"\\" API_INGRESS_RECORD_FILE_NAME, Buffer)) {
        theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_INGRESS_RECORD_FILE_NAME L" not found");
        return ERR(STATUS_NOT_FOUND);
    }

    StVariant Data;
    //try {
    auto ret = Data.FromPacket(&Buffer, true);
    //} catch (const CException&) {
    //	return ERR(STATUS_UNSUCCESSFUL);
    //}
    if (ret != StVariant::eErrNone) {
        theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_INGRESS_RECORD_FILE_NAME);
        return ERR(STATUS_UNSUCCESSFUL);
    }

    if (Data[API_S_VERSION].To<uint32>() != API_INGRESS_RECORD_FILE_VERSION) {
        theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_INGRESS_RECORD_FILE_NAME);
        return ERR(STATUS_UNSUCCESSFUL);
    }

    StVariant List = Data[API_S_INGRESS_LOG];

    for (uint32 i = 0; i < List.Count(); i++)
    {
        StVariant Item = List[i];

        CProgramID ID;
        if(!ID.FromVariant(StVariantReader(Item).Find(API_V_ID)))
			continue;
        CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
        if (!pItem) {
            DbgPrint(L"CProcessList::Load() - No program found for access record item with ID %s\n", ID.ToString().c_str());
            continue;
        }
        if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
            pProgram->LoadIngress(Item);
        else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
            pService->LoadIngress(Item);
    }

    return OK;
}

STATUS CProcessList::StoreIngress()
{
    SVarWriteOpt Opts;
    Opts.Format = SVarWriteOpt::eIndex;
    Opts.Flags = SVarWriteOpt::eSaveToFile;

    StVariantWriter List;
    List.BeginList();

    bool bSave = theCore->Config()->GetBool("Service", "SaveIngressRecord", false);

    for (auto pItem : theCore->ProgramManager()->GetItems())
    {
        ESavePreset ePreset = pItem.second->GetSaveTrace();
        if (ePreset == ESavePreset::eDontSave || (ePreset == ESavePreset::eDefault && !bSave))
			continue;

        // StoreAccess saves API_V_ID
        if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
            List.WriteVariant(pProgram->StoreIngress(Opts));
        else if (CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem.second))
            List.WriteVariant(pService->StoreIngress(Opts));
    }

    StVariant Data;
    Data[API_S_VERSION] = API_INGRESS_RECORD_FILE_VERSION;
    Data[API_S_INGRESS_LOG] = List.Finish();

    CBuffer Buffer;
    Data.ToPacket(&Buffer);
    WriteFileSafe(theCore->GetDataFolder() + L"\\" API_INGRESS_RECORD_FILE_NAME, Buffer);

    return OK;
}

STATUS CProcessList::Load()
{
    LoadIngress();
    LoadUsage();
    return OK;
}

STATUS CProcessList::Store()
{
    StoreIngress();
    StoreUsage();
    return OK;
}

STATUS CProcessList::LoadUsage()
{
    STATUS Status = LoadUsageImpl();
    m_bUsageLoaded = true; // whatever the outcome, there is nothing more to wait for
    return Status;
}

STATUS CProcessList::LoadUsageImpl()
{
    CBuffer Buffer;
    if (!ReadFile(theCore->GetDataFolder() + L"\\" API_USAGE_RECORD_FILE_NAME, Buffer)) {
        theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_USAGE_RECORD_FILE_NAME L" not found");
        return ERR(STATUS_NOT_FOUND);
    }

    StVariant Data;
    auto ret = Data.FromPacket(&Buffer, true);
    if (ret != StVariant::eErrNone) {
        theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_USAGE_RECORD_FILE_NAME);
        return ERR(STATUS_UNSUCCESSFUL);
    }

    if (Data[API_S_VERSION].To<uint32>() != API_USAGE_RECORD_FILE_VERSION) {
        theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_USAGE_RECORD_FILE_NAME);
        return ERR(STATUS_UNSUCCESSFUL);
    }

    StVariant List = Data[API_S_USAGE_LOG];

    for (uint32 i = 0; i < List.Count(); i++)
    {
        StVariant Item = List[i];

        CProgramID ID;
        if(!ID.FromVariant(StVariantReader(Item).Find(API_V_ID)))
            continue;
        CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
        if (!pItem) {
            DbgPrint(L"CProcessList::LoadUsage() - No program found for usage record item with ID %s\n", ID.ToString().c_str());
            continue;
        }
        if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
            pProgram->LoadUsage(Item);
        else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
            pService->LoadUsage(Item);
    }

    return OK;
}

STATUS CProcessList::StoreUsage()
{
    SVarWriteOpt Opts;
    Opts.Format = SVarWriteOpt::eIndex;
    Opts.Flags = SVarWriteOpt::eSaveToFile;

    StVariantWriter List;
    List.BeginList();

    bool bSave = theCore->Config()->GetBool("Service", "SaveUsageRecord", true);

    for (auto pItem : theCore->ProgramManager()->GetItems())
    {
        ESavePreset ePreset = pItem.second->GetSaveTrace();
        if (ePreset == ESavePreset::eDontSave || (ePreset == ESavePreset::eDefault && !bSave))
            continue;

        // StoreUsage saves API_V_ID, and returns an invalid variant when there is nothing to save
        StVariant Entry;
        if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
            Entry = pProgram->StoreUsage(Opts);
        else if (CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem.second))
            Entry = pService->StoreUsage(Opts);
        if (Entry.IsValid())
            List.WriteVariant(Entry);
    }

    StVariant Data;
    Data[API_S_VERSION] = API_USAGE_RECORD_FILE_VERSION;
    Data[API_S_USAGE_LOG] = List.Finish();

    CBuffer Buffer;
    Data.ToPacket(&Buffer);
    WriteFileSafe(theCore->GetDataFolder() + L"\\" API_USAGE_RECORD_FILE_NAME, Buffer);

    return OK;
}

//////////////////////////////////////////////////////////////////////////
// Usage sampling

void CProcessList::UpdateUsage()
{
    if (!theCore->Config()->GetBool("Service", "UsageStats", true))
        return;

    uint64 uNow = GetTickCount64();
    uint64 uInterval = theCore->Config()->GetUInt64("Service", "UsageSampleInterval", 1000);
    if (m_LastUsageSample && uNow - m_LastUsageSample < uInterval)
        return;
    uint64 uElapsed = m_LastUsageSample ? uNow - m_LastUsageSample : uInterval;
    m_LastUsageSample = uNow;

    //
    // One snapshot gives the cumulative CPU times and I/O byte counts of every process,
    // the driver does not report those. Taken without the lock, like EnumProcesses does.
    //
    std::vector<BYTE> Processes;
    NTSTATUS status = MyQuerySystemInformation(Processes, SystemProcessInformation);
    if (!NT_SUCCESS(status))
        return;

    uint64 NowMs = GetCurTime() * 1000ULL;

    std::vector<CProcessPtr> Known;
    {
        std::shared_lock Lock(m_Mutex);

        for (PSYSTEM_PROCESS_INFORMATION process = PH_FIRST_PROCESS(Processes.data()); process != NULL; process = PH_NEXT_PROCESS(process))
        {
            uint64 Pid = (uint64)process->UniqueProcessId;
            if (Pid == 0)
                continue; // skip Idle Process

            auto F = m_ProcessMap.find(SProcessUID(Pid, FILETIME2ms(process->CreateTime.QuadPart)));
            if (F == m_ProcessMap.end())
                continue; // not (yet) known, it will be picked up by the next sample
            F->second->UpdateUsage(process);
        }

        Known.reserve(m_ProcessMap.size());
        for (auto& I : m_ProcessMap)
            Known.push_back(I.second);
    }

    //
    // GPU and NPU time comes from dxgkrnl, per process and adapter node, see CGpuMonitor.
    // Adapters are enumerated on first use and again every few minutes when none was found.
    //
    if (theCore->Config()->GetBool("Service", "UsageGpuStats", true))
    {
        if (!m_GpuMonitor.IsReady() && (m_GpuMonitorRetry == 0 || uNow - m_GpuMonitorRetry > 5 * 60 * 1000)) {
            m_GpuMonitorRetry = uNow;
            m_GpuMonitor.SetLogger([](const std::wstring& Msg) {
                theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"%s", Msg.c_str());
            });
            m_GpuMonitor.Init();
        }

        if (m_GpuMonitor.IsReady())
        {
            std::map<uint32, CGpuMonitor::SProcessUsage> GpuUsage;
            m_GpuMonitor.Sample(GpuUsage);

            if (!GpuUsage.empty())
            {
                // a terminated process may still be listed under the same id as a new one, the newest wins
                std::map<uint32, CProcessPtr> ByPid;
                for (const CProcessPtr& pProcess : Known) {
                    CProcessPtr& pSlot = ByPid[(uint32)pProcess->GetProcessId()];
                    if (!pSlot || pSlot->GetCreationTime() < pProcess->GetCreationTime())
                        pSlot = pProcess;
                }
                for (auto& I : GpuUsage) {
                    auto F = ByPid.find(I.first);
                    if (F != ByPid.end())
                        F->second->AddGpuUsage(I.second.Time[CGpuMonitor::eGpu], I.second.Time[CGpuMonitor::eNpu]);
                }
            }
        }
    }
    else if (m_GpuMonitor.IsReady())
        m_GpuMonitor.Reset();

    //
    // Open connections are a gauge: each process' sockets are counted now and booked as
    // connection seconds (count times the seconds since the last sample) to the usage log of
    // the socket's owner, like its bytes. See CSocket::BookUsageConnections and SUsageSample.
    //
    uint32 ConnSeconds = (uint32)std::min<uint64>(std::max<uint64>((uElapsed + 500) / 1000, 1), 60);
    for (const CProcessPtr& pProcess : Known)
        CSocket::BookUsageConnections(pProcess->GetSocketList(), ConnSeconds);

    //
    // Book the deltas. Each sample goes to exactly one item, the same way the other records
    // are routed: a process that hosts exactly one service books to that service, every other
    // process to its program file. Pending network bytes are committed on every item of the
    // process, so a shared service host gets its sockets' traffic on the right service.
    //
    for (const CProcessPtr& pProcess : Known)
    {
        CProgramFilePtr pProgram = pProcess->GetProgram();
        if (!pProgram)
            continue;

        SUsageSample Sample;
        bool bHasSample = pProcess->TakeUsage(Sample);

        std::set<std::wstring> SvcTags = pProcess->GetServices();
        CWindowsServicePtr pTarget;
        if (SvcTags.size() == 1)
            pTarget = pProgram->GetService(*SvcTags.begin());

        if (pTarget) {
            pTarget->UsageLog()->Commit(NowMs, bHasSample ? &Sample : nullptr);
            pProgram->UsageLog()->Commit(NowMs, nullptr);
        }
        else {
            pProgram->UsageLog()->Commit(NowMs, bHasSample ? &Sample : nullptr);
            for (const std::wstring& SvcTag : SvcTags) {
                CWindowsServicePtr pService = pProgram->GetService(SvcTag);
                if (pService)
                    pService->UsageLog()->Commit(NowMs, nullptr);
            }
        }
    }
}
