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
#include "../Library/Hooking/HookUtils.h"
#include "../Library/Common/FileIO.h"

extern "C" {
#include "../../Driver/KSI/include/kphmsg.h"
}

#define API_INGRESS_RECORD_FILE_NAME L"IngressRecord.dat"
#define API_INGRESS_RECORD_FILE_VERSION 1

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
    SetThreadDescription(GetCurrentThread(), L"CProcessList__LoadProc");
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
        InitInject(); // todo handle error
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
    SetThreadDescription(GetCurrentThread(), L"CProcessList__StoreProc");
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
    uint64 start = GetUSTickCount();
#endif

    std::unique_lock Lock(m_Mutex);

    std::map<uint64, CProcessPtr> OldList(m_List);

    if (theCore->Driver()->IsConnected())
    {
        auto Pids = theCore->Driver()->EnumProcesses();
        if(Pids.IsError())
            return Pids.GetStatus();

        for (uint64 Pid : *Pids.GetValue())
        {
            if (Pid == 0)
                continue; // skip Idle Process

            bool bAdd = false;
            auto F = OldList.find(Pid);
            CProcessPtr pProcess;
            if (F != OldList.end()) {
                pProcess = F->second;
                OldList.erase(F);
            }
            else
            {
                pProcess = CProcessPtr(new CProcess(Pid));
                pProcess->Init();
                bAdd = true;
            }

            pProcess->Update();

            if (bAdd) 
				AddProcessImpl(pProcess);
        }
    }
    else
    {
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

        for (PSYSTEM_PROCESS_INFORMATION process = PH_FIRST_PROCESS(Processes.data()); process != NULL; process = PH_NEXT_PROCESS(process))
        {
            uint64 Pid = (uint64)process->UniqueProcessId;

            if (Pid == 0)
                continue; // skip Idle Process

            auto F = OldList.find(Pid);
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
            }

            pProcess->Update(process, bFullProcessInfo);
        }
    }

    Lock.unlock();

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
    //DbgPrint("EnumProcesses took %.2f ms cycles\r\n", (GetUSTickCount() - start) / 1000.0);
#endif

	return OK;
}

void CProcessList::AddProcessImpl(const CProcessPtr& pProcess)
{
    AddProcessUnsafe(pProcess);
    CProgramFilePtr pProgram = theCore->ProgramManager()->AddProcess(pProcess);
    pProcess->InitLibs();

    if (pProcess->HashInfoUnknown()) {
        if (!pProgram->HashInfoUnknown())
            pProcess->SetSignInfo(pProgram->GetSignInfo());
        //else
        //{
        //    theCore->ThreadPool()->enqueueTask([pProcess, pProgram](const std::wstring& ModulePath) {
        //        SVerifierInfo VerifyInfo;
        //        CProcess::MakeVerifyInfo(ModulePath, VerifyInfo);
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

CProcessPtr CProcessList::GetProcess(uint64 Pid, bool bCanAdd) 
{
	std::unique_lock Lock(m_Mutex);

	auto F = m_List.find(Pid); 
    if (F != m_List.end())
        return F->second;

    if (!bCanAdd)
        return NULL;

    CProcessPtr pProcess = CProcessPtr(new CProcess(Pid));
    if (pProcess->Init()) 
    {
        AddProcessUnsafe(pProcess);
        theCore->ProgramManager()->AddProcess(pProcess);

        //if (!theCore->AppIsolator()->AllowProcessStart(pProcess))
        //    TerminateProcess(Pid, pProcess->GetFileName());

        pProcess->Update();
    }
    else 
    {
        // Process already terminated

        m_List.insert(std::make_pair(pProcess->GetProcessId(), pProcess));
    }
    
	return pProcess; 
}

void CProcessList::AddProcessUnsafe(const CProcessPtr& pProcess)
{
    m_List.insert(std::make_pair(pProcess->GetProcessId(), pProcess));
    std::wstring AppContainerSID = pProcess->GetAppContainerSid();
    if (!AppContainerSID.empty())
        m_Apps.insert(std::make_pair(MkLower(AppContainerSID), pProcess));
    std::wstring FilePath = pProcess->GetNtFilePath();
    ASSERT(!FilePath.empty());
    m_ByPath.insert(std::make_pair(theCore->NormalizePath(FilePath), pProcess));
}

void CProcessList::RemoveProcessUnsafe(const CProcessPtr& pProcess)
{
    m_List.erase(pProcess->GetProcessId());
    std::wstring AppContainerSID = pProcess->GetAppContainerSid();
    if (!AppContainerSID.empty())
        mmap_erase(m_Apps, MkLower(AppContainerSID), pProcess);
    std::wstring FilePath = pProcess->GetNtFilePath();
    ASSERT(!FilePath.empty())
    mmap_erase(m_ByPath, theCore->NormalizePath(FilePath), pProcess);
}

std::map<uint64, CProcessPtr> CProcessList::FindProcesses(const CProgramID& ID)
{
	std::unique_lock Lock(m_Mutex);

	std::map<uint64, CProcessPtr> Found;

	switch (ID.GetType())
	{
	case EProgramType::eProgramFile:
        {
            for (auto I = m_ByPath.find(ID.GetFilePath()); I != m_ByPath.end() && I->first == ID.GetFilePath(); ++I) {
                Found.insert(std::make_pair(I->second->GetProcessId(), I->second));
            }
        }
		break;
	case EProgramType::eWindowsService:
        {
            auto pService = m_Services->GetService(ID.GetServiceTag());
            if (!pService || !pService->ProcessId)
                break;
            CProcessPtr pProcess = GetProcess(pService->ProcessId);
            if (!pProcess)
                break;
            
            Found.insert(std::make_pair(pProcess->GetProcessId(), pProcess));
        }
		break;
	case EProgramType::eAppPackage:
        {
            for (auto I = m_Apps.find(ID.GetAppContainerSid()); I != m_Apps.end() && I->first == ID.GetAppContainerSid(); ++I) {
                Found.insert(std::make_pair(I->second->GetProcessId(), I->second));
            }
        }
		break;
	case EProgramType::eAllPrograms:
		return m_List;
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

    std::unique_lock Lock(m_Mutex);

    // check relative paths based on the parrent process working directory
    std::wstring workingDir;
	auto F = m_List.find(ParentID); 
    if (F != m_List.end())
        workingDir = F->second->GetWorkDir();
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

bool CProcessList::OnProcessStarted(uint64 Pid, uint64 ParentPid, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& FileName, const std::wstring& Command, const struct SVerifierInfo* pVerifyInfo, uint64 CreateTime, EEventStatus Status, bool bETW)
{
    CProcessPtr pChildProcess = GetProcess(Pid, true);

    //
    // Note: It's possible to receive an event for a process before the OnProcessStarted is called.
    // Therefore, we need to update the process information here to ensure it stays in sync with the drivers protection status.
    //
    // This situation can occur, for instance, if a task manager detects the process and opens it before the process creation is fully completed.
    //
    pChildProcess->UpdateMisc();

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

            pChildProcess->m_Name = FileName;

            std::wstring FilePath = GetPathFromCmd(Command, FileName, ParentPid);
            if (!FilePath.empty()) {
                std::wstring NtPath = CNtPathMgr::Instance()->TranslateDosToNtPath(FilePath);
                if(NtPath.empty()) // fallback
                    pChildProcess->m_NtFilePath = L"\\??\\" + FilePath;
                else
                    pChildProcess->m_NtFilePath = NtPath;
            }
        }
        else
            pChildProcess->m_NtFilePath = FileName;

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
        pChildProcess->UpdateSignInfo(pVerifyInfo);
        if(pProgram)
            pProgram->UpdateSignInfo(pVerifyInfo);
    }

    CProcessPtr pParentProcess = GetProcess(ActorPid, true);
    CProgramFilePtr pActorProgram = pParentProcess ? pParentProcess->GetProgram() : NULL;
    if (pProgram && pActorProgram)
    {
		CFlexGuid TargetEnclave = pChildProcess->GetEnclave();
		CFlexGuid ActorEnclave = pParentProcess->GetEnclave();

        pProgram->AddExecParent(TargetEnclave, 
            pActorProgram, ActorEnclave, pParentProcess->GetProcessUID(), ActorServiceTag, 
            Command, CreateTime, (Status != EEventStatus::eAllowed));
        pActorProgram->AddExecChild(ActorEnclave, ActorServiceTag, 
            pProgram, TargetEnclave, pChildProcess->GetProcessUID(), 
            Command, CreateTime, (Status != EEventStatus::eAllowed));

#ifdef DEF_USE_POOL
		CExecLogEntryPtr pLogActorEntry = pActorProgram->Allocator()->New<CExecLogEntry>(ActorEnclave, EExecLogRole::eActor, EExecLogType::eProcessStarted, Status, pProgram->GetUID(), TargetEnclave, ActorServiceTag, CreateTime, Pid);
#else
        CExecLogEntryPtr pLogActorEntry = CExecLogEntryPtr(new CExecLogEntry(ActorEnclave, EExecLogRole::eActor, EExecLogType::eProcessStarted, Status, pProgram->GetUID(), TargetEnclave, ActorServiceTag, CreateTime, Pid));
#endif
        AddExecLogEntry(pActorProgram, pLogActorEntry);

#ifdef DEF_USE_POOL
		CExecLogEntryPtr pLogEntry = pProgram->Allocator()->New<CExecLogEntry>(TargetEnclave, EExecLogRole::eTarget, EExecLogType::eProcessStarted, Status, pActorProgram->GetUID(), ActorEnclave, ActorServiceTag, CreateTime, Pid);
#else
        CExecLogEntryPtr pLogEntry = CExecLogEntryPtr(new CExecLogEntry(TargetEnclave, EExecLogRole::eTarget, EExecLogType::eProcessStarted, Status, pActorProgram->GetUID(), ActorEnclave, ActorServiceTag, CreateTime, Pid));
#endif
        AddExecLogEntry(pProgram, pLogEntry);
    }

    //return theCore->AppIsolator()->AllowProcessStart(pChildProcess);
    return true;
}

void CProcessList::OnProcessStopped(uint64 Pid, uint32 ExitCode)
{
	std::unique_lock Lock(m_Mutex);

    auto F = m_List.find(Pid);
    if (F == m_List.end())
        return;
    CProcessPtr pProcess = F->second;

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
	CExecLogEntryPtr pLogActorEntry = pActorProgram->Allocator()->New<CExecLogEntry>(TargetEnclave, EExecLogRole::eActor, bThread ? EExecLogType::eThreadAccess : EExecLogType::eProcessAccess, Status, pActorProgram->GetUID(), ActorEnclave, ActorServiceTag, AccessTime, Pid, AccessMask);
#else
    CExecLogEntryPtr pLogActorEntry = CExecLogEntryPtr(new CExecLogEntry(ActorEnclave, EExecLogRole::eActor, bThread ? EExecLogType::eThreadAccess : EExecLogType::eProcessAccess, Status, pTargetProgram->GetUID(), TargetEnclave, ActorServiceTag, AccessTime, Pid, AccessMask));
#endif
    AddExecLogEntry(pActorProgram, pLogActorEntry);

#ifdef DEF_USE_POOL
	CExecLogEntryPtr pLogEntry = pTargetProgram->Allocator()->New<CExecLogEntry>(TargetEnclave, EExecLogRole::eTarget, bThread ? EExecLogType::eThreadAccess : EExecLogType::eProcessAccess, Status, pActorProgram->GetUID(), ActorEnclave, ActorServiceTag, AccessTime, Pid, AccessMask);
#else
    CExecLogEntryPtr pLogEntry = CExecLogEntryPtr(new CExecLogEntry(TargetEnclave, EExecLogRole::eTarget, bThread ? EExecLogType::eThreadAccess : EExecLogType::eProcessAccess, Status, pActorProgram->GetUID(), ActorEnclave, ActorServiceTag, AccessTime, Pid, AccessMask));
#endif
    AddExecLogEntry(pTargetProgram, pLogEntry);
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
    // Note: an image may receive a loaded event folowed by an unloaded event
    // this means the library was unloaded bevore controll was returned to user space
    // in which case we handle it as if it would have been unloaded instantly.
    // Note: this shoudl no longer be the case, we now issue the event once all is checked and final
    //

    EEventStatus Status;
    if (pImageEvent->Type == SProcessEvent::EType::UntrustedLoad) { // image was deamed not trusted
        if (pImageEvent->bLoadPrevented)
            Status = EEventStatus::eBlocked;
        else
            Status = EEventStatus::eUntrusted;
    } else // image was loaded, we do not know if it was trusted or not
        Status = Status = EEventStatus::eAllowed;
	
    CFlexGuid EnclaveGuid = pProcess->GetEnclave();

    pProcess->AddLibrary(pLibrary);
    pProgram->AddLibrary(EnclaveGuid, pLibrary, pImageEvent->TimeStamp, &pImageEvent->VerifierInfo, Status);

#ifdef DEF_USE_POOL
	CExecLogEntryPtr pLogEntry = pProgram->Allocator()->New<CExecLogEntry>(EnclaveGuid, EExecLogRole::eBooth, EExecLogType::eImageLoad, Status, pLibrary->GetUID(), CFlexGuid(), pImageEvent->ActorServiceTag, pImageEvent->TimeStamp, pImageEvent->ProcessId);
#else
    CExecLogEntryPtr pLogEntry = CExecLogEntryPtr(new CExecLogEntry(EnclaveGuid, EExecLogRole::eBooth, EExecLogType::eImageLoad, Status, pLibrary->GetUID(), CFlexGuid(), pImageEvent->ActorServiceTag, pImageEvent->TimeStamp, pImageEvent->ProcessId));
#endif
    AddExecLogEntry(pProgram, pLogEntry);
}

void CProcessList::OnResourceAccessed(const std::wstring& NtPath, uint64 ActorPid, const std::wstring& ActorServiceTag, uint32 AccessMask, uint64 AccessTime, EEventStatus Status, NTSTATUS NtStatus, bool IsDirectory)
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
	CResLogEntryPtr pLogEntry = pActorProgram->Allocator()->New<CResLogEntry>(pActorProcess->GetEnclave(), NtPath, ActorServiceTag, AccessMask, Status, AccessTime, ActorPid);
#else
    CResLogEntryPtr pLogEntry = CResLogEntryPtr(new CResLogEntry(pActorProcess->GetEnclave(), NtPath, ActorServiceTag, AccessMask, Status, AccessTime, ActorPid));
#endif
    
    uint64 LogIndex = pActorProgram->AddTraceLogEntry(pLogEntry, ETraceLogs::eResLog);

    StVariant Event;
    //Event[SVC_API_EVENT_TYPE]	= SVC_API_EVENT_FW_EVENT;
    Event[API_V_ID]		= pActorProgram->GetID().ToVariant(SVarWriteOpt());
    Event[API_V_EVENT_INDEX]	= LogIndex;
    Event[API_V_EVENT_DATA]	    = pLogEntry->ToVariant();

    if(pLogEntry->GetStatus() != EEventStatus::eAllowed)
        theCore->BroadcastMessage(SVC_API_EVENT_RES_ACTIVITY, Event); // always breadcast suspiciosue activity
    else
        theCore->BroadcastMessage(SVC_API_EVENT_RES_ACTIVITY, Event, pActorProgram);
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
		theCore->BroadcastMessage(SVC_API_EVENT_EXEC_ACTIVITY, Event); // always breadcast suspiciosue activity
	else
        theCore->BroadcastMessage(SVC_API_EVENT_EXEC_ACTIVITY, Event, pProgram);
}

NTSTATUS CProcessList::OnInjectionRequest(uint64 Pid)
{
    const ULONG DesiredAccess =
        PROCESS_DUP_HANDLE | PROCESS_TERMINATE | PROCESS_SUSPEND_RESUME
        | PROCESS_SET_INFORMATION | PROCESS_QUERY_INFORMATION
        | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE;

    CScopedHandle hProcess = CScopedHandle(OpenProcess(DesiredAccess, FALSE, (DWORD)Pid), CloseHandle);

    /*CScopedHandle hHandle = CScopedHandle((HANDLE)0, CloseHandle);
    if (NT_SUCCESS(NtOpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_ADJUST_DEFAULT, &hHandle)))
    { 
    }*/

    ULONG errlvl = InjectLdr(hProcess, 0);

	return STATUS_SUCCESS;
}

NTSTATUS CProcessList::OnProcessDrvEvent(const SProcessEvent* pEvent)
{
    switch (pEvent->Type)
    {
        case SProcessEvent::EType::ProcessStarted: 
        {
            const SProcessStartEvent* pStartEvent = (SProcessStartEvent*)(pEvent);
            OnProcessStarted(pStartEvent->ProcessId, pStartEvent->ParentId, pStartEvent->ActorProcessId, pStartEvent->ActorServiceTag, pStartEvent->FileName, pStartEvent->CommandLine, &pStartEvent->VerifierInfo, pStartEvent->TimeStamp, pStartEvent->Status);

            //
            // Note: this triggers before teh process is started, during early process initialization
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
        case SProcessEvent::EType::UntrustedLoad:
        {
            OnImageEvent((SProcessImageEvent*)pEvent);
            break;
		}
        case SProcessEvent::EType::ProcessAccess:
        case SProcessEvent::EType::ThreadAccess:
		{
			const SProcessAccessEvent* pAccessEvent = (SProcessAccessEvent*)(pEvent);
            if(pAccessEvent->bCreating)
                break; // don't log events which are part of regular process creation process
            OnProcessAccessed(pAccessEvent->ProcessId, pAccessEvent->ActorProcessId, pAccessEvent->ActorServiceTag, pEvent->Type == SProcessEvent::EType::ThreadAccess, pAccessEvent->AccessMask, pAccessEvent->TimeStamp, pAccessEvent->Status);
			break;
		}
        case SProcessEvent::EType::ResourceAccess:
        {
            const SResourceAccessEvent* pAccessEvent = (SResourceAccessEvent*)(pEvent);
			OnResourceAccessed(pAccessEvent->Path, pAccessEvent->ActorProcessId, pAccessEvent->ActorServiceTag, pAccessEvent->AccessMask, pAccessEvent->TimeStamp, pAccessEvent->Status, pAccessEvent->NtStatus, pAccessEvent->IsDirectory);
			break;
        }
		case SProcessEvent::EType::InjectionRequest:
		{
			const SInjectionRequest* pInjectEvent = (SInjectionRequest*)(pEvent);
			return OnInjectionRequest(pInjectEvent->ProcessId);
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

    OnProcessStarted(pEvent->ProcessId, pEvent->ParentId, pEvent->ParentId, L"", pEvent->FileName, pEvent->CommandLine, NULL, pEvent->TimeStamp, EEventStatus::eAllowed, true);
    //if (!OnProcessStarted(pEvent->ProcessId, pEvent->ParentId, pEvent->FileName, pEvent->CommandLine, pEvent->TimeStamp, true))
    //    TerminateProcess(pEvent->ProcessId, pEvent->FileName);
}

//////////////////////////////////////////////////////////////////////////
// Load/Store

STATUS CProcessList::Load()
{
    CBuffer Buffer;
    if (!ReadFile(theCore->GetDataFolder() + L"\\" API_INGRESS_RECORD_FILE_NAME, Buffer)) {
        theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_INGRESS_RECORD_FILE_NAME L" not found");
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
        CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
        if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
            pProgram->LoadIngress(Item);
        else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
            pService->LoadIngress(Item);
    }

    return OK;
}

STATUS CProcessList::Store()
{
    SVarWriteOpt Opts;
    Opts.Format = SVarWriteOpt::eIndex;
    Opts.Flags = SVarWriteOpt::eSaveToFile;

    StVariantWriter List;
    List.BeginList();

    bool bSave = theCore->Config()->GetBool("Service", "SaveIngressRecord", true);

    for (auto pItem : theCore->ProgramManager()->GetItems())
    {
        //ESavePreset ePreset = pItem.second->GetSaveTrace();
        //if (ePreset == ESavePreset::eDontSave || (ePreset == ESavePreset::eDefault && !bSave))
        if (!bSave)
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
    WriteFile(theCore->GetDataFolder() + L"\\" API_INGRESS_RECORD_FILE_NAME, Buffer);

    return OK;
}