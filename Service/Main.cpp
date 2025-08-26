#include "pch.h"
#include "ServiceCore.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/Service.h"
#include "../Library/API/ServiceAPI.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "../Library/Helpers/ScopedHandle.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/IPC/AbstractClient.h"
#include <shellapi.h>
#include "Helpers/SecDeskHelper.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

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
       SERVICE_STATUS        g_ServiceStatus;
       SERVICE_STATUS_HANDLE g_ServiceStatusHandle = NULL;
       BOOLEAN               g_UnloadProtection = FALSE;


DWORD WINAPI ServiceHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
    if (dwControl == SERVICE_CONTROL_STOP || dwControl == SERVICE_CONTROL_SHUTDOWN)
    {
        if(dwControl == SERVICE_CONTROL_STOP && g_UnloadProtection)
			return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;

#ifdef _DEBUG
		DbgPrint("PrivacyAgent ServiceHandlerEx: Stopping service %d ...\n", dwControl);
#endif
        CServiceCore::Shutdown(dwControl == SERVICE_CONTROL_SHUTDOWN ? CServiceCore::eShutdown_System : CServiceCore::eShutdown_Wait);

        g_ServiceStatus.dwCurrentState        = SERVICE_STOPPED;
        g_ServiceStatus.dwCheckPoint          = 0;
        g_ServiceStatus.dwWaitHint            = 0;

    } 
    /*else if (dwControl == SERVICE_CONTROL_PRESHUTDOWN) 
    {

    } */
    else if (dwControl != SERVICE_CONTROL_INTERROGATE)
        return ERROR_CALL_NOT_IMPLEMENTED;

    if (! SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus))
        return GetLastError();

    return 0;
}

void WINAPI ServiceMain(DWORD argc, WCHAR *argv[])
{
    g_ServiceStatusHandle = RegisterServiceCtrlHandlerEx(ServiceName, ServiceHandlerEx, NULL);
    if (! g_ServiceStatusHandle)
        return;

    g_ServiceStatus.dwServiceType                 = SERVICE_WIN32;
    g_ServiceStatus.dwCurrentState                = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted            = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN; // | SERVICE_ACCEPT_PRESHUTDOWN
    g_ServiceStatus.dwWin32ExitCode               = 0;
    g_ServiceStatus.dwServiceSpecificExitCode     = 0;
    g_ServiceStatus.dwCheckPoint                  = 1;
    g_ServiceStatus.dwWaitHint                    = 6000;

    ULONG status = 0;

    if (!SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus))
        status = GetLastError();

    /*while (! IsDebuggerPresent()) {
        Sleep(1000);
    } __debugbreak();*/


    STATUS Status = CServiceCore::Startup();
    if (!Status)
        status = Status.GetStatus();

    if (status == 0) {

        g_ServiceStatus.dwCurrentState        = SERVICE_RUNNING;
        g_ServiceStatus.dwCheckPoint          = 0;
        g_ServiceStatus.dwWaitHint            = 0;

    } else {

        g_ServiceStatus.dwCurrentState        = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode       = ERROR_SERVICE_SPECIFIC_ERROR;
        g_ServiceStatus.dwServiceSpecificExitCode = status;
    }

    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
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

    WSADATA wsaData;
    WORD  wVersionRequested = MAKEWORD(2, 2);
    WSAStartup(wVersionRequested, &wsaData);

    STATUS Status = OK;

    std::wstring MsgBox = GetArgument(arguments, L"MsgBox");
    if (!MsgBox.empty())
    {
        auto TypePrompt = Split2(MsgBox, L":");

        auto TypeIcon = Split2(TypePrompt.first, L"-");

        UINT Type = MB_OK;
        if(_wcsicmp(TypeIcon.first.c_str(), L"OK") == 0) Type = MB_OK;
        else if(_wcsicmp(TypeIcon.first.c_str(), L"OKCANCEL") == 0) Type = MB_OKCANCEL;
        else if(_wcsicmp(TypeIcon.first.c_str(), L"ABORTRETRYIGNORE") == 0) Type = MB_ABORTRETRYIGNORE;
        else if(_wcsicmp(TypeIcon.first.c_str(), L"YESNOCANCEL") == 0) Type = MB_YESNOCANCEL;
        else if(_wcsicmp(TypeIcon.first.c_str(), L"YESNO") == 0) Type = MB_YESNO;
        else if(_wcsicmp(TypeIcon.first.c_str(), L"RETRYCANCEL") == 0) Type = MB_RETRYCANCEL;
        else if(_wcsicmp(TypeIcon.first.c_str(), L"CANCELTRYCONTINUE") == 0) Type = MB_CANCELTRYCONTINUE;

        if(_wcsicmp(TypeIcon.second.c_str(), L"STOP") == 0) Type |= MB_ICONHAND;
        else if(_wcsicmp(TypeIcon.second.c_str(), L"QUESTION") == 0) Type |= MB_ICONQUESTION;
        else if(_wcsicmp(TypeIcon.second.c_str(), L"EXCLAMATION") == 0) Type |= MB_ICONEXCLAMATION;

        wchar_t szPath[MAX_PATH];
        GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
        *wcsrchr(szPath, L'\\') = L'\0';
        wcscat_s(szPath, MAX_PATH, L"\\MajorWallpaper.png");

        return ShowSecureMessageBox(TypePrompt.second, L"MajorPrivacy", Type, szPath);
    }
    else if (HasFlag(arguments, L"engine"))
    {
        Status = CServiceCore::Startup(true);
        if (Status) {
            if (HANDLE hThread = theCore->GetThreadHandle())
                WaitForSingleObject(hThread, INFINITE);
        }
    }
    else if (HasFlag(arguments, L"startup"))
    {
        SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
        if ((SvcState & SVC_INSTALLED) == SVC_INSTALLED && (SvcState & SVC_RUNNING) != SVC_RUNNING) {
            std::wstring BinaryPath = GetServiceBinaryPath(API_SERVICE_NAME);
            std::wstring ServicePath = CServiceCore::NormalizePath(GetFileFromCommand(BinaryPath));
            std::wstring AppDir = CServiceCore::NormalizePath(GetApplicationDirectory());
            if (ServicePath.length() < AppDir.length() || ServicePath.compare(0, AppDir.length(), AppDir) != 0)
                RemoveService(API_SERVICE_NAME);
        }

        SvcState = GetServiceState(API_SERVICE_NAME);
        if ((SvcState & SVC_INSTALLED) == 0)
            Status = CServiceAPI::InstallSvc(false);
        if ((SvcState & SVC_RUNNING) == 0)
            Status = RunService(API_SERVICE_NAME);
    }
    else if (HasFlag(arguments, L"install"))
    {
        SVC_STATE DrvState = GetServiceState(API_DRIVER_NAME);
        if ((DrvState & SVC_INSTALLED) == 0)
#ifdef _DEBUG_
            Status = CServiceCore::InstallDriver(false);
#else
            Status = CServiceCore::InstallDriver(true);
#endif
        
        SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
        if ((SvcState & SVC_INSTALLED) == 0)
#ifdef _DEBUG_
            Status = CServiceAPI::InstallSvc(false);
#else
            Status = CServiceAPI::InstallSvc(true);
#endif
    }
    else if (HasFlag(arguments, L"unload") || HasFlag(arguments, L"remove"))
    {
        SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
        if ((SvcState & SVC_RUNNING) == SVC_RUNNING) 
        {
            CDriverAPI* pDrvApi = new CDriverAPI();
            Status = pDrvApi->ConnectDrv();
            if (Status) {
                g_UnloadProtection = pDrvApi->GetConfigBool("UnloadProtection", false);
                pDrvApi->Disconnect();
            }
            delete pDrvApi;
            pDrvApi = NULL;

            CServiceAPI* pSvcAPI = new CServiceAPI();
            Status = pSvcAPI->ConnectSvc();
            if (Status) {

                if (g_UnloadProtection) 
                {
                    StVariant Args;
                    Args[API_V_MB_TEXT] = L"Do you want to remove MajorPrivacy Service and Driver?";
                    //Args[API_V_MB_TITLE]
                    Args[API_V_MB_TYPE] = (uint32)MB_YESNO;
                    auto Ret = pSvcAPI->Call(SVC_API_SHOW_SECURE_PROMPT, Args, NULL);
                    if (!Ret || Ret.GetValue().Get(API_V_MB_CODE).To<uint32>() != IDYES)
                        return -1;
                }

                Status = pSvcAPI->Call(SVC_API_SHUTDOWN, StVariant(), NULL);
                pSvcAPI->Disconnect();
            }
            delete pSvcAPI;
            pSvcAPI = NULL;

            Status = KillService(API_SERVICE_NAME);
        }

        Status = CServiceCore::StopDriver();

        if (Status && HasFlag(arguments, L"remove"))
        {
            if (Status && (SvcState & SVC_INSTALLED) == SVC_INSTALLED)
                Status = RemoveService(API_SERVICE_NAME);

            SVC_STATE DrvState = GetServiceState(API_DRIVER_NAME);
            if ((DrvState & SVC_INSTALLED) == SVC_INSTALLED)
                Status = CServiceCore::RemoveDriver();
        }
    }
    else if (HasFlag(arguments, L"cleanup"))
    {
        // todo: remove all user config
    }
    else
    {
        SERVICE_TABLE_ENTRY myServiceTable[] = {
            { ServiceName, ServiceMain },
            { NULL, NULL }
        };

#if 0
        while(!IsDebuggerPresent())
            Sleep(100);
        DebugBreak();
#endif

        if (!StartServiceCtrlDispatcher(myServiceTable)) {
            DWORD err = GetLastError();
            if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
            {
                MessageBoxW(NULL, L"Please run this application as a service, or use the -engine flag to run it as a standalone application.", L"Error", MB_OK | MB_ICONERROR);
            }
            return err;
        }
    }

    WSACleanup();

    CNtPathMgr::Dispose();

    return Status.GetStatus();
}