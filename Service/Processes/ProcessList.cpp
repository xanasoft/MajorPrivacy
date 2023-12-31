#include "pch.h"

#include <Shlwapi.h>

#include "ProcessList.h"
#include "../Library/Common/DbgHelp.h"
#include "ServiceCore.h"
#include "../Library/API/DriverAPI.h"
#include "../Etw/EtwEventMonitor.h"
#include "../Library/Helpers/EvtUtil.h"
#include "ServiceAPI.h"
#include "ServiceList.h"
#include "AppIsolator.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Programs/ProgramManager.h"


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

    svcCore->Driver()->RegisterProcessHandler(&CProcessList::OnProcessDrvEvent, this);
    Status = svcCore->Driver()->RegisterForProcesses(true);
    if (Status.IsError()) 
    {
        //
        // When the driver registration fails we fall back to using ETW
        //

        svcCore->Log()->LogEvent(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_DRIVER_FAILED, L"Failed to register with driver for process creation notifications.");

        svcCore->EtwEventMonitor()->RegisterProcessHandler(&CProcessList::OnProcessEtwEvent, this);
        Status = OK;
    }

    EnumProcesses();

    m_Services->EnumServices();

    return Status;
}

void CProcessList::Update()
{
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

    if (svcCore->Driver()->IsDrvConnected())
    {
        auto Pids = svcCore->Driver()->EnumProcesses();

        for (uint64 Pid : *Pids.GetValue())
        {
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
                pProcess->Init();

                AddProcessUnsafe(pProcess);
                svcCore->ProgramManager()->AddProcess(pProcess);

                if (!svcCore->AppIsolator()->AllowProcessStart(pProcess)) {
                    TerminateProcess(Pid, pProcess->GetFileName());
                    continue;
                }
            }

            pProcess->Update();
        }
    }
    else
    {
        std::vector<BYTE> Processes;
        //NtEnumProcesses(Processes, SystemExtendedProcessInformation);
        bool bFullProcessInfo = true;
        NTSTATUS status = NtEnumProcesses(Processes, SystemFullProcessInformation); // requires win 8 and admin
        if (!NT_SUCCESS(status)) {
            bFullProcessInfo = false;
            status = NtEnumProcesses(Processes, SystemProcessInformation); // fallback win 7
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
                svcCore->ProgramManager()->AddProcess(pProcess);

                if (!svcCore->AppIsolator()->AllowProcessStart(pProcess)) {
                    TerminateProcess(Pid, pProcess->GetFileName());
                    continue;
                }
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
            RemoveProcessUnsafe(pProcess);
            svcCore->ProgramManager()->RemoveProcess(pProcess);
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
    pProcess->Init();

    AddProcessUnsafe(pProcess);
    svcCore->ProgramManager()->AddProcess(pProcess);

    if (!svcCore->AppIsolator()->AllowProcessStart(pProcess))
        TerminateProcess(Pid, pProcess->GetFileName());

    pProcess->Update();
    
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
	case CProgramID::eFile:
        {
            for (auto I = m_ByPath.find(ID.GetFilePath()); I != m_ByPath.end() && I->first == ID.GetFilePath(); ++I) {
                Found.insert(std::make_pair(I->second->GetProcessId(), I->second));
            }
        }
		break;
	case CProgramID::eService:
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
	case CProgramID::eApp:
        {
            for (auto I = m_Apps.find(ID.GetAppContainerSid()); I != m_Apps.end() && I->first == ID.GetAppContainerSid(); ++I) {
                Found.insert(std::make_pair(I->second->GetProcessId(), I->second));
            }
        }
		break;
	case CProgramID::eAll:
		return m_List;
	}
	return Found;
}

void CProcessList::TerminateProcess(uint64 Pid, const std::wstring& FileName)
{
    svcCore->Log()->LogEvent(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_BLOCK_PROC, StrLine(L"Terminating blocked process: %s (%u).", FileName.c_str(), Pid));
    CScopedHandle ProcessHandle = CScopedHandle(OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)Pid), CloseHandle);
    if (!ProcessHandle || !NT_SUCCESS(NtTerminateProcess(ProcessHandle, -1)))
        svcCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_KILL_FAILED, StrLine(L"Failed to terminate blocked process: %s (%u)!", FileName.c_str(), Pid));
}

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

bool CProcessList::OnProcessStarted(uint64 Pid, uint64 ParentPid, const std::wstring& FileName, const std::wstring& Command, uint64 CreateTime, bool bETW)
{
    CProcessPtr pProcess;

	std::unique_lock Lock(m_Mutex);

    auto F = m_List.find(Pid);
    if (F == m_List.end())
    {
        pProcess = CProcessPtr(new CProcess(Pid));
        pProcess->Init();
        
        //
        // pProcess->Init(); may fail eider entierly or partially,
        // hence here we set whatever data may be missing and we have
        //

        if(pProcess->m_CreationTime == 0)
            pProcess->SetRawCreationTime(CreateTime);
        if(pProcess->m_ParentPid == -1)
            pProcess->m_ParentPid = ParentPid;

        if(bETW && pProcess->m_Name.empty())
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
        else if(pProcess->m_FileName.empty())
            pProcess->m_FileName = FileName;
        if(pProcess->m_CommandLine.empty())
            pProcess->m_CommandLine = Command;

        AddProcessUnsafe(pProcess);
        Lock.unlock();
        svcCore->ProgramManager()->AddProcess(pProcess);
    }
    else
    {
        pProcess = F->second;

        Lock.unlock();

        std::unique_lock Lock(pProcess->m_Mutex);

        if(pProcess->m_CommandLine.empty())
            pProcess->m_CommandLine = Command;
    }

    return svcCore->AppIsolator()->AllowProcessStart(pProcess);
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

NTSTATUS CProcessList::OnProcessDrvEvent(const SProcessEvent* pEvent)
{
    if (pEvent->Type != SProcessEvent::EType::Started)
    {
        OnProcessStopped(pEvent->ProcessId, pEvent->ExitCode);
        return STATUS_SUCCESS;
    }

    //
    // Note: this triggers before teh process is started, during early process initialization
    // returnign an error code canceles the process creation
    //

#ifdef _DEBUG
    //DbgPrint("CProcessList::OnProcessDrvEvent (%d) %S\r\n", pEvent->ProcessId, pEvent->CommandLine.c_str());
#endif

    if (!OnProcessStarted(pEvent->ProcessId, pEvent->ParentId, pEvent->FileName, pEvent->CommandLine, pEvent->CreateTime)) 
    {
        svcCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_BLOCK_PROC, StrLine(L"Preventing start of blocked process: %s (%u).", pEvent->FileName, pEvent->ProcessId));

        return STATUS_ACCESS_DENIED;
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

    if (!OnProcessStarted(pEvent->ProcessId, pEvent->ParentId, pEvent->FileName, pEvent->CommandLine, pEvent->TimeStamp, true))
        TerminateProcess(pEvent->ProcessId, pEvent->FileName);
}