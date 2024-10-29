#include "pch.h"
#include "ServiceCore.h"
#include "../Library/API/PrivacyAPI.h"
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

    if (! SetServiceStatus(ServiceStatusHandle, &ServiceStatus))
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

    if (HasFlag(arguments, L"engine"))
    {
        STATUS Status = CServiceCore::Startup(true);

        while(theCore)
            Sleep(1000);

        return Status.GetStatus();
    }


    SERVICE_TABLE_ENTRY myServiceTable[] = {
        { ServiceName, ServiceMain },
        { NULL, NULL }
    };

    if (! StartServiceCtrlDispatcher(myServiceTable))
        return GetLastError();

    return NO_ERROR;
}