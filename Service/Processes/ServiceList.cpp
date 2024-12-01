#include "pch.h"
#include "ServiceCore.h"
#include "ServiceList.h"
#include "ProcessList.h"
#include "../Programs/ProgramManager.h"

#include "../../Library/Common/Buffer.h"
#include "../../Library/Common/Strings.h"
#include "../../Library/Helpers/WinUtil.h"
#include "../../Library/Helpers/Scoped.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../../Library/Helpers/AppUtil.h"

CServiceList::CServiceList(std::recursive_mutex& Mutex)
    : m_Mutex(Mutex)
{
}

void CServiceList::EnumServices()
{
	CScopedHandle scmHandle(OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE), CloseServiceHandle);
    if (!scmHandle)
        return;

    NTSTATUS status;

    ULONG type;
    if (g_WindowsVersion >= WINDOWS_10_RS1)
        type = SERVICE_WIN32 | SERVICE_ADAPTER | /*SERVICE_DRIVER |*/ SERVICE_INTERACTIVE_PROCESS | SERVICE_USER_SERVICE | SERVICE_USERSERVICE_INSTANCE | SERVICE_PKG_SERVICE; // SERVICE_TYPE_ALL;
    else if (g_WindowsVersion >= WINDOWS_10)
        type = SERVICE_WIN32 | SERVICE_ADAPTER | /*SERVICE_DRIVER |*/ SERVICE_INTERACTIVE_PROCESS | SERVICE_USER_SERVICE | SERVICE_USERSERVICE_INSTANCE;
    else
        type = /*SERVICE_DRIVER |*/ SERVICE_WIN32;

    CBuffer Buffer(0x8000);

    ULONG returnLength;
    ULONG servicesReturned;
    for (;;) 
    {
        if (EnumServicesStatusEx(scmHandle, SC_ENUM_PROCESS_INFO, type, SERVICE_STATE_ALL, (PBYTE)Buffer.GetBuffer(), (DWORD)Buffer.GetCapacity(), &returnLength, &servicesReturned, NULL, NULL))
            status = STATUS_SUCCESS;
        else
            status = GetLastWin32ErrorAsNtStatus();

        if (status == STATUS_MORE_ENTRIES)
        {
            Buffer.SetSize(0, true, Buffer.GetCapacity() * 2);
            continue;
        }
        break;
    }

    if (!NT_SUCCESS(status))
        return;
    Buffer.SetSize(returnLength);

    LPENUM_SERVICE_STATUS_PROCESS services = (LPENUM_SERVICE_STATUS_PROCESS)Buffer.GetBuffer();

    std::unique_lock Lock(m_Mutex);

    std::map<std::wstring, SServicePtr> OldList(m_List);

    for (ULONG i = 0; i < servicesReturned; i++)
    {
        LPENUM_SERVICE_STATUS_PROCESS service = &services[i];

        if (g_WindowsVersion >= WINDOWS_10_RS2)
            PhServiceWorkaroundWindowsServiceTypeBug(service);

        //if (service->ServiceStatusProcess.dwServiceType & SERVICE_DRIVER)
        //    continue; // for now we dont care about drivers

        std::wstring Id = MkLower(service->lpServiceName); // its not case sensitive

        std::wstring BinaryPath;

        // todo: read directly from registyr to spead up
        CScopedHandle serviceHandle(OpenService(scmHandle, service->lpServiceName, SERVICE_QUERY_CONFIG), CloseServiceHandle);
        if (serviceHandle) 
        {
            bool ok;
            CBuffer Buffer(0x200);
            ULONG bufferSize = 0x200;
            if (!(ok = QueryServiceConfigW(serviceHandle, (LPQUERY_SERVICE_CONFIGW)Buffer.GetBuffer(), (DWORD)Buffer.GetCapacity(), &bufferSize))) {
                Buffer.SetSize(bufferSize, true);
                ok = QueryServiceConfigW(serviceHandle, (LPQUERY_SERVICE_CONFIGW)Buffer.GetBuffer(), (DWORD)Buffer.GetCapacity(), &bufferSize);
                Buffer.SetSize(bufferSize);
            }

            if (ok) {
                LPQUERY_SERVICE_CONFIGW config = (LPQUERY_SERVICE_CONFIGW)Buffer.GetBuffer();
                BinaryPath = DosPathToNtPath(GetFileFromCommand(config->lpBinaryPathName));
            }
        }

        auto F = OldList.find(Id);
        SServicePtr pService;
        if (F != OldList.end()) {
            pService = F->second;
            OldList.erase(F);

			if (pService->BinaryPath != BinaryPath) {
				pService->BinaryPath = BinaryPath;
				theCore->ProgramManager()->AddService(pService); // update the binary path and parent program file
			}
        }
        else
        {
            pService = SServicePtr(new SService());
            pService->Id = service->lpServiceName;
            pService->Name = service->lpDisplayName;
			pService->BinaryPath = BinaryPath;

            m_List.insert(std::make_pair(Id, pService));
            theCore->ProgramManager()->AddService(pService);
        }
        
        pService->State = service->ServiceStatusProcess.dwCurrentState;
        if (pService->ProcessId != service->ServiceStatusProcess.dwProcessId)
        {
            if (pService->ProcessId) 
				ClearProcessUnsafe(pService);

            pService->ProcessId = service->ServiceStatusProcess.dwProcessId;

            if (pService->ProcessId)
				SetProcessUnsafe(pService);
        }
    }

    for(auto E: OldList) {
        m_List.erase(E.first);
        SServicePtr pService = E.second;
        if (pService->ProcessId)
            ClearProcessUnsafe(pService);
        theCore->ProgramManager()->RemoveService(pService);
    }
}

void CServiceList::SetProcessUnsafe(const SServicePtr& pService)
{
    std::wstring Id = MkLower(pService->Id);

    m_Pids.insert(std::make_pair(pService->ProcessId, Id));
    CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pService->ProcessId, true);
    if (pProcess) {
        pProcess->AddService(Id);
        CWindowsServicePtr pService = theCore->ProgramManager()->GetService(Id);
        pService->SetProcess(pProcess);
    }
}

void CServiceList::ClearProcessUnsafe(const SServicePtr& pService)
{
    std::wstring Id = MkLower(pService->Id);

    mmap_erase(m_Pids, pService->ProcessId, Id);

    CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pService->ProcessId);
    if (pProcess) {
        pProcess->RemoveService(Id);
        CWindowsServicePtr pService = theCore->ProgramManager()->GetService(Id, false);
        if (pService)
            pService->SetProcess(NULL);
    }
}