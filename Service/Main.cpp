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
#include "../Library/Helpers/WinUtil.h"
#include "../Library/Helpers/TokenUtil.h"
#include "../Library/IPC/AbstractClient.h"
#include <shellapi.h>
//#include <shlobj.h>
#include "Helpers/SecDeskHelper.h"
#include "../Library/IPC/ServerReadyEvent.h"

#include "../library/Helpers/MiniDumpFilter.h"
#include "../MajorPrivacy/version.h"

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

static int RestartSelf(const wchar_t* arg_tag, bool bAsSystem = false)
{
    std::wstring Command = GetCommandLineW();
    if (arg_tag) { Command += L" -"; Command += arg_tag; }

    bool bSetEvent = false;
    CServerReadyEvent ReadyEvent;
    ReadyEvent.ClientCreate(API_WORKER_READY_EVENT);

    CScopedHandle hProcess = CScopedHandle((HANDLE)0, CloseHandle);
    if (bAsSystem) {
        if (!RunAsSystem(Command, &hProcess)) { // try run as system tocken based method
            //
            // The scheduler method restarts us in session 0 those this helper needs to wait for the global event and set the local event itself.
            //
            bSetEvent = true;
            ReadyEvent.ClientRelease();
            ReadyEvent.ClientCreate(API_SERVICE_READY_EVENT);
            if(!RunAsSystemTask(Command, NULL)) // fallback to task scheduler based method
                return -1;
        }
    } else {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        if (!CreateProcessW(NULL, (WCHAR*)Command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            return -1;
		hProcess.Set(pi.hProcess);
        CloseHandle(pi.hThread);    
    } 

    // Wait for engine to signal ready, but also watch for process exit
    HANDLE Handles[] = { ReadyEvent.GetHandle(), hProcess };
    DWORD Ret = WaitForMultipleObjects(hProcess ? 2 : 1, Handles, FALSE, 5 * 60 * 1000);

    if (Ret == WAIT_OBJECT_0) // Event signaled - engine is ready
    {
        if (bSetEvent)
            CServerReadyEvent::WorkerSignal(API_WORKER_READY_EVENT);
        return 0;
    }
    else if (Ret == WAIT_OBJECT_0 + 1) { // Process exited
        DWORD exitCode;
        GetExitCodeProcess(hProcess, &exitCode);
        if (exitCode == 0 && bSetEvent)
            CServerReadyEvent::WorkerSignal(API_WORKER_READY_EVENT);
        return exitCode;
    }
	return -1; // Timeout or error
}

EXTERN_C DECLSPEC_IMPORT HRESULT STDAPICALLTYPE SHGetFolderPathW(_Reserved_ HWND hwnd, _In_ int csidl, _In_opt_ HANDLE hToken, _In_ DWORD dwFlags, _Out_writes_(MAX_PATH) LPWSTR pszPath);

//
// Close any running MajorPrivacy GUI processes.
// Used when unloading/removing the service so the GUI does not linger without its backend.
// A WM_CLOSE would only minimize the GUI to the tray, hence we terminate the processes.
//
static void CloseGui()
{
    std::vector<BYTE> Processes;
    if (!NT_SUCCESS(MyQuerySystemInformation(Processes, SystemProcessInformation)))
        return;

    for (PSYSTEM_PROCESS_INFORMATION process = PH_FIRST_PROCESS(Processes.data()); process != NULL; process = PH_NEXT_PROCESS(process))
    {
        if (process->UniqueProcessId == 0 || process->ImageName.Buffer == NULL)
            continue; // skip Idle Process and entries without a name

        std::wstring Name(process->ImageName.Buffer, process->ImageName.Length / sizeof(wchar_t));
        if (_wcsicmp(Name.c_str(), APP_NAME L".exe") != 0)
            continue;

        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)(ULONG_PTR)process->UniqueProcessId);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }
}

int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
	srand((unsigned int)time(NULL));

    if (!IsDebuggerPresent()) {
        // Dumps go under %ProgramData%\Xanasoft\MajorPrivacy\MiniDump.
        WCHAR szDumpDir[MAX_PATH] = {0};
        if (SUCCEEDED(SHGetFolderPathW(NULL, 0x0023/*CSIDL_COMMON_APPDATA*/, NULL, 0, szDumpDir))) {
            wcscat_s(szDumpDir, MAX_PATH, L"\\Xanasoft\\MajorPrivacy\\MiniDump");
        }
        MiniDumpFilter_Init(NULL, L"PrivacyAgent-v" VERSION_WSTR, MDF_TYPE_TRIAGE, NULL,
                            szDumpDir[0] ? szDumpDir : NULL);
    }

#ifdef _DEBUG
    MySetThreadDescription(GetCurrentThread(), L"WinMain");
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
    //
    // Password Prompt - uses section-based IPC for secure password transfer
    //
    std::wstring PwPrompt = GetArgument(arguments, L"PwPrompt");
    if (!PwPrompt.empty())
    {
        // Enable per-monitor DPI awareness for proper scaling on high-DPI displays
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        // Parse section handle from command line (format: 0x...)
        HANDLE hSection = (HANDLE)wcstoull(PwPrompt.c_str() + 2, NULL, 16);
        if (!hSection)
            return -1;

        // Map the section
        SPasswordPromptSection* pSection = (SPasswordPromptSection*)
            MapViewOfFile(hSection, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(SPasswordPromptSection));
        if (!pSection)
        {
            CloseHandle(hSection);
            return -1;
        }

        // Get background image path
        wchar_t szPath[MAX_PATH];
        GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
        *wcsrchr(szPath, L'\\') = L'\0';
        wcscat_s(szPath, MAX_PATH, L"\\MajorWallpaper.png");

        // Show the password dialog on secure desktop
        ShowSecurePasswordDialog(NULL, pSection, szPath);

        // Cleanup
        UnmapViewOfFile(pSection);
        CloseHandle(hSection);
        return 0;
    }
    else if (HasFlag(arguments, L"engine"))
    {
        //while (! IsDebuggerPresent()) {
        //    Sleep(1000);
        //}
        //MessageBoxW(NULL, GetCommandLineW(), L"PrivacyAgent.exe -engine", MB_OK);

        HANDLE hToken = NULL;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken) && !HasFlag(arguments, L"sync_tok"))
        {
            //
            // When we are started by ShellExecuteEx while the driver is already loaded, windows fails to set the correct permissions on out process token
			// We need to restart to fix that
            // 
			// Without the right permissions we can't enable require privileges to create a system process and access SCM to start our service and
            //

            return RestartSelf(L"sync_tok");
        }
        NtClose(hToken);
        hToken = NULL;

        if (!IsRunningAsSystem() && !HasFlag(arguments, L"sync_sys"))
        {
            //
            // We must run as SYSTEM to have full privileges for the engine mode
            //

            return RestartSelf(L"sync_sys", true);
        }

        Status = CServiceCore::Startup(true);
        if (Status) {
            if (HANDLE hThread = theCore->GetThreadHandle())
                WaitForSingleObject(hThread, INFINITE);
        }
        else if (Status.GetStatus() == STATUS_SYNCHRONIZATION_REQUIRED && !HasFlag(arguments, L"sync_drv"))
        {
            //
            // We want to elevate our trust level from High to Maximum,
            // and need to restart while the driver is loaded.
            //

            return RestartSelf(L"sync_drv");
        }
    }
    else if (HasFlag(arguments, L"startup"))
    {
        std::wstring AppDir = CServiceCore::NormalizePath(GetApplicationDirectory());

        SVC_STATE DrvState = GetServiceState(API_DRIVER_NAME);
        if ((DrvState & SVC_INSTALLED) == SVC_INSTALLED && (DrvState & SVC_RUNNING) != SVC_RUNNING)
        {
            std::wstring BinaryPath = GetServiceBinaryPath(API_DRIVER_NAME);
            std::wstring ServicePath = CServiceCore::NormalizePath(BinaryPath);
            if (ServicePath.length() < AppDir.length() || _wcsnicmp(ServicePath.c_str(), AppDir.c_str(), AppDir.length()) != 0)
            {
                theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Updated driver, old path: %s; new path: %s", ServicePath.c_str(), AppDir.c_str());
                RemoveService(API_DRIVER_NAME);
                DrvState = SVC_NOT_FOUND;
            }
        }

        SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
        if ((SvcState & SVC_INSTALLED) == SVC_INSTALLED && (SvcState & SVC_RUNNING) != SVC_RUNNING) 
        {
            std::wstring BinaryPath = GetServiceBinaryPath(API_SERVICE_NAME);
            std::wstring ServicePath = CServiceCore::NormalizePath(GetFileFromCommand(BinaryPath));
            if (ServicePath.length() < AppDir.length() || ServicePath.compare(0, AppDir.length(), AppDir) != 0)
                RemoveService(API_SERVICE_NAME);
        }

        DrvState = GetServiceState(API_DRIVER_NAME);
        if ((DrvState & SVC_INSTALLED) == 0)
            Status = CDriverAPI::InstallDrv(false);
        if ((DrvState & SVC_RUNNING) == 0)
            Status = RunService(API_DRIVER_NAME);

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
        bool bRemove = HasFlag(arguments, L"remove");

        CDriverAPI* pDrvApi = new CDriverAPI();
        Status = pDrvApi->ConnectDrv();
        if (Status) {
            g_UnloadProtection = pDrvApi->GetConfigBool("UnloadProtection", false);
            pDrvApi->Disconnect();
        }
        delete pDrvApi;
        pDrvApi = NULL;

        SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
        if (SvcState == 0 || (SvcState & SVC_RUNNING) == SVC_RUNNING) // not installed or running
        {
            CServiceAPI* pSvcAPI = new CServiceAPI();
			if (SvcState == 0) // when service is not installed try connect in case it runs in engine mode
                Status = pSvcAPI->ConnectEngine(false);
            else
                Status = pSvcAPI->ConnectSvc();
            if (Status) 
            {
                if (g_UnloadProtection)
                {
                    StVariant Args;
                    if (bRemove)
                        Args[API_V_MB_TEXT] = L"Do you want to remove MajorPrivacy Service and Driver?";
                    else
                        Args[API_V_MB_TEXT] = L"Do you want to unload MajorPrivacy Service and Driver?";
                    //Args[API_V_MB_TITLE]
                    Args[API_V_MB_TYPE] = (uint32)MB_YESNO;
                    auto Ret = pSvcAPI->Call(SVC_API_SHOW_SECURE_PROMPT, Args, NULL);
                    if (!Ret || Ret.GetValue().Get(API_V_MB_CODE).To<uint32>() != IDYES) {
                        return -1;
                    }
                }

                auto Ret = pSvcAPI->Call(SVC_API_SHUTDOWN, StVariant(), NULL);
                if(Ret.IsError())
					Status = Ret;
                else if (SvcState == 0)
                {
					DWORD PID = Ret.GetValue().Get(API_V_PID).To<uint32>();
					HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, PID);
                    if (hProcess) {
                        WaitForSingleObject(hProcess, 30000);
                        CloseHandle(hProcess);
                    }
                }
                pSvcAPI->Disconnect();
            }
            delete pSvcAPI;
            pSvcAPI = NULL;

            if ((SvcState & SVC_RUNNING) == SVC_RUNNING)
                Status = KillService(API_SERVICE_NAME);
        }

        // The service/driver is going away, so close the GUI as well.
        CloseGui();

        Status = CServiceCore::StopDriver();

        if (Status && bRemove)
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