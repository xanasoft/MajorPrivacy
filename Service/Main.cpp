#include "pch.h"
#include "ServiceCore.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/Service.h"
#include "../Library/API/ServiceAPI.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "../Library/Helpers/ScopedHandle.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include <shellapi.h>

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

//#include "../NtCRT/NtCRT.h"

extern "C" _ACRTIMP
void __cdecl _wassert(wchar_t const* _Message, wchar_t const* _File, unsigned _Line);

//SYSTEM_INFO g_SystemInfo;



static WCHAR                *ServiceName = (WCHAR*)API_SERVICE_NAME;
static SERVICE_STATUS        ServiceStatus;
static SERVICE_STATUS_HANDLE ServiceStatusHandle = NULL;


DWORD WINAPI ServiceHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
    if (dwControl == SERVICE_CONTROL_STOP || dwControl == SERVICE_CONTROL_SHUTDOWN)
    {
        CServiceCore::Shutdown();

        ServiceStatus.dwCurrentState        = SERVICE_STOPPED;
        ServiceStatus.dwCheckPoint          = 0;
        ServiceStatus.dwWaitHint            = 0;

    } else if (dwControl != SERVICE_CONTROL_INTERROGATE)
        return ERROR_CALL_NOT_IMPLEMENTED;

    if (! SetServiceStatus(ServiceStatusHandle, &ServiceStatus))
        return GetLastError();

    return 0;
}

void WINAPI ServiceMain(DWORD argc, WCHAR *argv[])
{
    ServiceStatusHandle = RegisterServiceCtrlHandlerEx(ServiceName, ServiceHandlerEx, NULL);
    if (! ServiceStatusHandle)
        return;

    ServiceStatus.dwServiceType                 = SERVICE_WIN32;
    ServiceStatus.dwCurrentState                = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted            = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    ServiceStatus.dwWin32ExitCode               = 0;
    ServiceStatus.dwServiceSpecificExitCode     = 0;
    ServiceStatus.dwCheckPoint                  = 1;
    ServiceStatus.dwWaitHint                    = 6000;

    ULONG status = 0;

    if (!SetServiceStatus(ServiceStatusHandle, &ServiceStatus))
        status = GetLastError();

    /*while (! IsDebuggerPresent()) {
        Sleep(1000);
    } __debugbreak();*/


    STATUS Status = CServiceCore::Startup();
    if (!Status)
        status = Status.GetStatus();

    if (status == 0) {

        ServiceStatus.dwCurrentState        = SERVICE_RUNNING;
        ServiceStatus.dwCheckPoint          = 0;
        ServiceStatus.dwWaitHint            = 0;

    } else {

        ServiceStatus.dwCurrentState        = SERVICE_STOPPED;
        ServiceStatus.dwWin32ExitCode       = ERROR_SERVICE_SPECIFIC_ERROR;
        ServiceStatus.dwServiceSpecificExitCode = status;
    }

    SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
}

bool HasFlag(const std::vector<std::wstring>& arguments, std::wstring name)
{
	return std::find(arguments.begin(), arguments.end(), L"-" + name) != arguments.end();
}

std::wstring GetArgument(const std::vector<std::wstring>& arguments, std::wstring name, std::wstring mod = L"-") 
{
	std::wstring prefix = mod + name + L":";
	for (size_t i = 0; i < arguments.size(); i++) {
		if (_wcsicmp(arguments[i].substr(0, prefix.length()).c_str(), prefix.c_str()) == 0) {
			return arguments[i].substr(prefix.length());
		}
	}
	return L"";
}

int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
    SetThreadDescription(GetCurrentThread(), L"WinMain");
#endif

	//NTCRT_DEFINE(MyCRT);
	//InitGeneralCRT(&MyCRT);

    //GetSystemInfo(&g_SystemInfo);

#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc(1162);
#endif

	int nArgs = 0;
	LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	std::vector<std::wstring> arguments;
	for (int i = 0; i < nArgs; i++)
		arguments.push_back(szArglist[i]);
	LocalFree(szArglist);

    STATUS Status = OK;

    if (HasFlag(arguments, L"engine"))
    {
        Status = CServiceCore::Startup(true);
        if(HANDLE hThread = theCore->GetThreadHandle()) 
            WaitForSingleObject(hThread, INFINITE);
    }
    else if (HasFlag(arguments, L"startup"))
    {
        SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);

        //
        // Check if the service is ours
        //

        if ((SvcState & SVC_INSTALLED) == SVC_INSTALLED && (SvcState & SVC_RUNNING) != SVC_RUNNING)
        {
            std::wstring BinaryPath = GetServiceBinaryPath(API_SERVICE_NAME);

            std::wstring ServicePath = NormalizeFilePath(GetFileFromCommand(BinaryPath));
            std::wstring AppDir = NormalizeFilePath(GetApplicationDirectory());
            if (ServicePath.length() < AppDir.length() || ServicePath.compare(0, AppDir.length(), AppDir) != 0)
            {
                RemoveService(API_SERVICE_NAME);
                SvcState = SVC_NOT_FOUND;
            }
        }

        if (SvcState == SVC_NOT_FOUND)
            Status = CServiceAPI::InstallSvc();
        if (Status && (SvcState & SVC_RUNNING) == 0)
            Status = RunService(API_SERVICE_NAME);
    }
    else if (HasFlag(arguments, L"install"))
    {
        SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
        if ((SvcState & SVC_INSTALLED) == 0)
            Status = CServiceAPI::InstallSvc();
    }
    else if (HasFlag(arguments, L"remove"))
    {
        SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
        if ((SvcState & SVC_RUNNING) == SVC_RUNNING)
            Status = KillService(API_SERVICE_NAME);
		if (Status && (SvcState & SVC_INSTALLED) == SVC_INSTALLED)
            Status = RemoveService(API_SERVICE_NAME);

        SVC_STATE DrvState = GetServiceState(API_DRIVER_NAME);
        if (Status && (DrvState & SVC_RUNNING) == SVC_RUNNING)
            Status = KillService(API_DRIVER_NAME);
        if (Status && (DrvState & SVC_INSTALLED) == SVC_INSTALLED)
            Status = RemoveService(API_DRIVER_NAME);
    }
    else
    {
        SERVICE_TABLE_ENTRY myServiceTable[] = {
            { ServiceName, ServiceMain },
            { NULL, NULL }
        };

        if (!StartServiceCtrlDispatcher(myServiceTable)) {
            DWORD err = GetLastError();
            if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
            {
                MessageBoxW(NULL, L"Please run this application as a service, or use the -engine flag to run it as a standalone application.", L"Error", MB_OK | MB_ICONERROR);
            }
            return err;
        }
    }

    CNtPathMgr::Dispose();

    return Status.GetStatus();
}