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
    //DbgPrint("EnumProcesses took %dms\r\n", GetTickCount64() - start);
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

CProcessPtr CProcessList::GetProcessEx(uint64 Pid, EGetMode Mode)
{
	std::unique_lock Lock(m_Mutex);

	auto F = m_List.find(Pid); 
    if (F != m_List.end())
        return F->second;

    if (Mode == eCanNotAdd)
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
        if (Mode != eMustAdd)
            return nullptr;

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
    CResLogEntryPtr pLogEntry = pActorProgram->Allocator()->New<CResLogEntry>(pActorProcess->GetEnclave(), NtPath, ActorServiceTag, AccessMask, EEventStatus::eProtected, AccessTime, ActorPid);
#else
    CResLogEntryPtr pLogEntry = CResLogEntryPtr(new CResLogEntry(pActorProcess->GetEnclave(), NtPath, ActorServiceTag, AccessMask, EEventStatus::eProtected, AccessTime, ActorPid));
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

STATUS CProcessList::Load()
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
    WriteFile(theCore->GetDataFolder() + L"\\" API_INGRESS_RECORD_FILE_NAME, Buffer);

    return OK;
}