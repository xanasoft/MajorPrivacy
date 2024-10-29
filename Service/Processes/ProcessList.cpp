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
#include "../Library/Helpers/AppUtil.h"
#include "../Programs/ProgramManager.h"
#include "../Library/Common/Strings.h"
#include "../Library/IPC/PipeServer.h"
#include "../Access/ResLogEntry.h"


CProcessList::CProcessList()
{
    m_Services = new CServiceList(m_Mutex);
}

CProcessList::~CProcessList()
{
    delete m_Services;
}

STATUS CProcessList::Init()
{
    STATUS Status;

    theCore->Driver()->RegisterProcessHandler(&CProcessList::OnProcessDrvEvent, this);
    Status = theCore->Driver()->RegisterForProcesses(true);
    if (Status.IsError()) 
    {
        //
        // When the driver registration fails we fall back to using ETW
        //

        theCore->Log()->LogEvent(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Failed to register with driver for process creation notifications.");

        theCore->EtwEventMonitor()->RegisterProcessHandler(&CProcessList::OnProcessEtwEvent, this);
        Status = OK;
    }

    Update();

    return Status;
}

void CProcessList::Update()
{
    m_bLogNotFound = theCore->Config()->GetBool("Service", "LogNotFound", false);

    EnumProcesses();

    m_Services->EnumServices();
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
            {
                AddProcessUnsafe(pProcess);
                theCore->ProgramManager()->AddProcess(pProcess);
                pProcess->InitLibs();

                //if (!theCore->AppIsolator()->AllowProcessStart(pProcess)) {
                //    TerminateProcess(Pid, pProcess->GetFileName());
                //    continue;
                //}
            }
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

                AddProcessUnsafe(pProcess);
                theCore->ProgramManager()->AddProcess(pProcess);
                pProcess->InitLibs();

                //if (!theCore->AppIsolator()->AllowProcessStart(pProcess)) {
                //    TerminateProcess(Pid, pProcess->GetFileName());
                //    continue;
                //}
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
    std::wstring FilePath = pProcess->GetFileName();
    if(!FilePath.empty())
        m_ByPath.insert(std::make_pair(NormalizeFilePath(FilePath), pProcess));
}

void CProcessList::RemoveProcessUnsafe(const CProcessPtr& pProcess)
{
    m_List.erase(pProcess->GetProcessId());
    std::wstring AppContainerSID = pProcess->GetAppContainerSid();
    if (!AppContainerSID.empty())
        mmap_erase(m_Apps, MkLower(AppContainerSID), pProcess);
    std::wstring FilePath = pProcess->GetFileName();
    if(!FilePath.empty())
        mmap_erase(m_ByPath, NormalizeFilePath(FilePath), pProcess);
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

bool CProcessList::OnProcessStarted(uint64 Pid, uint64 ParentPid, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& FileName, const std::wstring& Command, uint64 CreateTime, EEventStatus Status, bool bETW)
{
    CProcessPtr pProcess = GetProcess(Pid, true);
    if (!pProcess->IsInitDone()) {

        std::unique_lock Lock(pProcess->m_Mutex);

        bool bInitDone = pProcess->m_ParentPid != -1;
        pProcess->m_ParentPid = ParentPid;

        if (pProcess->m_CreationTime == 0)
            pProcess->SetRawCreationTime(CreateTime);

        if (bETW)
        {
            //
            // ETW returns only the actual file name not file path
            // we try to recover the full path from the command line
            //

            pProcess->m_Name = FileName;

            std::wstring FilePath = GetPathFromCmd(Command, FileName, ParentPid);
            if (!FilePath.empty())
                pProcess->m_FileName = DosPathToNtPath(FilePath);
        }
        else
            pProcess->m_FileName = FileName;

        if (pProcess->m_CommandLine.empty())
            pProcess->m_CommandLine = Command;

        Lock.unlock();

        if (!bInitDone) {
            std::unique_lock Lock(m_Mutex);

            AddProcessUnsafe(pProcess); // Note: at this point its already listed in m_List but as its not a multi map its fine
            theCore->ProgramManager()->AddProcess(pProcess);
        }
    }

    CProgramFilePtr pProgram = pProcess->GetProgram();

    CProcessPtr pActorProcess = GetProcess(ActorPid, true);
    CProgramFilePtr pActorProgram = pActorProcess ? pActorProcess->GetProgram() : NULL;
    if (pProgram && pActorProgram)
    {
        pProgram->AddExecActor(pActorProgram, ActorServiceTag, Command, CreateTime, (Status != EEventStatus::eAllowed));
        pActorProgram->AddExecTarget(ActorServiceTag, pProgram, Command, CreateTime, (Status != EEventStatus::eAllowed));

        CExecLogEntryPtr pLogActorEntry = CExecLogEntryPtr(new CExecLogEntry(EExecLogRole::eActor, EExecLogType::eProcessStarted, Status, pProgram->GetUID(), ActorServiceTag, CreateTime, Pid));
        AddExecLogEntry(pActorProgram, pLogActorEntry);

        CExecLogEntryPtr pLogEntry = CExecLogEntryPtr(new CExecLogEntry(EExecLogRole::eTarget, EExecLogType::eProcessStarted, Status, pActorProgram->GetUID(), ActorServiceTag, CreateTime, Pid));
        AddExecLogEntry(pProgram, pLogEntry);
    }

    //return theCore->AppIsolator()->AllowProcessStart(pProcess);
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
    CProcessPtr pProcess = GetProcess(Pid, true);
    CProgramFilePtr pProgram = pProcess ? pProcess->GetProgram() : NULL;

    CProcessPtr pActorProcess = GetProcess(ActorPid, true);
    CProgramFilePtr pActorProgram = pActorProcess ? pActorProcess->GetProgram() : NULL;

    if (!pProgram || !pActorProgram || pProgram == pActorProgram) // discard self accesses
        return;

    pProgram->AddIngressActor(pActorProgram, ActorServiceTag, bThread, AccessMask, AccessTime, (Status != EEventStatus::eAllowed));
    pActorProgram->AddIngressTarget(ActorServiceTag, pProgram, bThread, AccessMask, AccessTime, (Status != EEventStatus::eAllowed));

    CExecLogEntryPtr pLogActorEntry = CExecLogEntryPtr(new CExecLogEntry(EExecLogRole::eActor, bThread ? EExecLogType::eThreadAccess : EExecLogType::eProcessAccess, Status, pProgram->GetUID(), ActorServiceTag, AccessTime, Pid, AccessMask));
    AddExecLogEntry(pActorProgram, pLogActorEntry);

    CExecLogEntryPtr pLogEntry = CExecLogEntryPtr(new CExecLogEntry(EExecLogRole::eTarget, bThread ? EExecLogType::eThreadAccess : EExecLogType::eProcessAccess, Status, pActorProgram->GetUID(), ActorServiceTag, AccessTime, Pid, AccessMask));
    AddExecLogEntry(pProgram, pLogEntry);
}

void CProcessList::OnImageEvent(const SProcessImageEvent* pImageEvent)
{
	CProcessPtr pProcess = GetProcess(pImageEvent->ProcessId, true);
    if(!pProcess)
        return;

    std::wstring ProgramPath = NormalizeFilePath(pProcess->GetFileName());
    std::wstring ModulePath = NormalizeFilePath(pImageEvent->FileName);

    bool bIsProcess = ProgramPath == ModulePath;
    if (bIsProcess)
        pProcess->UpdateSignInfo(pImageEvent->SignAuthority, pImageEvent->SignLevel, pImageEvent->SignPolicy);

    CProgramFilePtr pProgram = pProcess->GetProgram();
    if (!pProgram)
        return;

    if (bIsProcess) {
        pProgram->UpdateSignInfo(pImageEvent->SignAuthority, pImageEvent->SignLevel, pImageEvent->SignPolicy);
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
    //

    EEventStatus Status;
    if (pImageEvent->Type == SProcessEvent::EType::UntrustedLoad) { // image was deamed not trusted
        if (pImageEvent->bLoadPrevented)
            Status = EEventStatus::eBlocked;
        else
            Status = EEventStatus::eUntrusted;
    } else // image was loaded, we do not know if it was trusted or not
        Status = Status = EEventStatus::eAllowed;
	
    pProgram->AddLibrary(pLibrary, pImageEvent->TimeStamp, pImageEvent->SignAuthority, pImageEvent->SignLevel, pImageEvent->SignPolicy, Status);

    CExecLogEntryPtr pLogEntry = CExecLogEntryPtr(new CExecLogEntry(EExecLogRole::eBooth, EExecLogType::eImageLoad, Status, pLibrary->GetUID(), pImageEvent->ActorServiceTag, pImageEvent->TimeStamp, pImageEvent->ProcessId));
    AddExecLogEntry(pProgram, pLogEntry);
}

void CProcessList::OnResourceAccessed(const std::wstring& Path, uint64 ActorPid, const std::wstring& ActorServiceTag, uint32 AccessMask, uint64 AccessTime, EEventStatus Status, NTSTATUS NtStatus)
{
	if (!m_bLogNotFound && (NtStatus == STATUS_OBJECT_NAME_NOT_FOUND || NtStatus == STATUS_OBJECT_PATH_NOT_FOUND))
		return; // ignore not found errors

    CProcessPtr pActorProcess = GetProcess(ActorPid, true);
    CProgramFilePtr pActorProgram = pActorProcess ? pActorProcess->GetProgram() : NULL;

    if (!pActorProgram)
        return;

    pActorProgram->AddAccess(ActorServiceTag, Path, AccessMask, AccessTime, (Status != EEventStatus::eAllowed));

    CResLogEntryPtr pLogEntry = CResLogEntryPtr(new CResLogEntry(Path, ActorServiceTag, AccessMask, Status, AccessTime, ActorPid));
    
    uint64 LogIndex = pActorProgram->AddTraceLogEntry(pLogEntry, ETraceLogs::eResLog);

    CVariant Event;
    //Event[SVC_API_EVENT_TYPE]	= SVC_API_EVENT_FW_EVENT;
    Event[API_V_PROG_ID]		= pActorProgram->GetID().ToVariant(SVarWriteOpt());
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

    CVariant Event;
    //Event[SVC_API_EVENT_TYPE]	= SVC_API_EVENT_FW_EVENT;
    Event[API_V_PROG_ID]		= pProgram->GetID().ToVariant(SVarWriteOpt());
    Event[API_V_EVENT_INDEX]	= LogIndex;
    Event[API_V_EVENT_DATA]	    = pLogEntry->ToVariant();

    if(pLogEntry->GetStatus() != EEventStatus::eAllowed)
		theCore->BroadcastMessage(SVC_API_EVENT_EXEC_ACTIVITY, Event); // always breadcast suspiciosue activity
	else
        theCore->BroadcastMessage(SVC_API_EVENT_EXEC_ACTIVITY, Event, pProgram);
}

NTSTATUS CProcessList::OnProcessDrvEvent(const SProcessEvent* pEvent)
{
    switch (pEvent->Type)
    {
        case SProcessEvent::EType::ProcessStarted: 
        {
            const SProcessStartEvent* pStartEvent = (SProcessStartEvent*)(pEvent);
            OnProcessStarted(pStartEvent->ProcessId, pStartEvent->ParentId, pStartEvent->ActorProcessId, pStartEvent->ActorServiceTag, pStartEvent->FileName, pStartEvent->CommandLine, pStartEvent->TimeStamp, pStartEvent->Status);

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
            OnProcessAccessed(pAccessEvent->ProcessId, pAccessEvent->ActorProcessId, pAccessEvent->ActorServiceTag, pEvent->Type == SProcessEvent::EType::ThreadAccess, pAccessEvent->AccessMask, pAccessEvent->TimeStamp, pAccessEvent->Status);
			break;
		}
        case SProcessEvent::EType::ResourceAccess:
        {
            const SResourceAccessEvent* pAccessEvent = (SResourceAccessEvent*)(pEvent);
			OnResourceAccessed(pAccessEvent->Path, pAccessEvent->ActorProcessId, pAccessEvent->ActorServiceTag, pAccessEvent->AccessMask, pAccessEvent->TimeStamp, pAccessEvent->Status, pAccessEvent->NtStatus);
			break;
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

    OnProcessStarted(pEvent->ProcessId, pEvent->ParentId, pEvent->ParentId, L"", pEvent->FileName, pEvent->CommandLine, pEvent->TimeStamp, EEventStatus::eAllowed, true);
    //if (!OnProcessStarted(pEvent->ProcessId, pEvent->ParentId, pEvent->FileName, pEvent->CommandLine, pEvent->TimeStamp, true))
    //    TerminateProcess(pEvent->ProcessId, pEvent->FileName);
}