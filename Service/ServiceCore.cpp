#include "pch.h"

#include <objbase.h>

#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/EvtUtil.h"
#include "../Library/Helpers/AppUtil.h"
#include "ServiceCore.h"
#include "ServiceAPI.h"
#include "../Library/API/PipeServer.h"
#include "../Library/API/AlpcPortServer.h"
#include "Network/NetIsolator.h"
#include "Processes/AppIsolator.h"
#include "Filesystem/FSIsolator.h"
#include "Registry/RegIsolator.h"
#include "Etw/EtwEventMonitor.h"
#include "../Library/API/DriverAPI.h"
#include "../Driver/DriverApi.h"
#include "../Library/Common/Strings.h"
#include "Processes/ProcessList.h"
#include "Programs/ProgramManager.h"
#include "Network/SocketList.h"
#include "Network/Firewall/Firewall.h"
#include "Tweaks/TweakManager.h"


CServiceCore* svcCore = NULL;


CServiceCore::CServiceCore()
{
#ifdef _DEBUG
	//TestVariant();
#endif

	m_pLog = new CEventLogger(API_SERVICE_NAME);

	m_pUserPipe = new CPipeServer();
	m_pUserPort = new CAlpcPortServer();

	m_pProgramManager = new CProgramManager();
	m_pProcessList = new CProcessList();

	m_pAppIsolator = new CAppIsolator();
	m_pNetIsolator = new CNetIsolator();
	m_pFSIsolator = new CFSIsolator();
	m_pRegIsolator = new CRegIsolator();

	m_pTweakManager = new CTweakManager();

	m_Driver = new CDriverAPI();

	m_pEtwEventMonitor = new CEtwEventMonitor();

	RegisterUserAPI();
}

CServiceCore::~CServiceCore()
{
	delete m_pEtwEventMonitor;

	delete m_Driver;

	delete m_pTweakManager;

	delete m_pAppIsolator;
	delete m_pNetIsolator;
	delete m_pFSIsolator;
	delete m_pRegIsolator;

	delete m_pProcessList;
	delete m_pProgramManager;

	delete m_pUserPipe;
	delete m_pUserPort;
}

STATUS CServiceCore::Startup(bool bEngineMode)
{
	if (svcCore)
		return ERR(SPAPI_E_DEVINST_ALREADY_EXISTS);

	//
	// Check if our event log exists, and if not create it
	//

	if (!EventSourceExists(API_SERVICE_NAME))
		CreateEventSource(API_SERVICE_NAME, APP_NAME);
	
	if (!EventSourceExists(API_DRIVER_NAME))
		CreateEventSource(API_DRIVER_NAME, APP_NAME);

	// todo start driver if not already started

	svcCore = new CServiceCore();
	svcCore->m_bEngineMode = bEngineMode;

	svcCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, 1, L"Starting Service...");

	STATUS Status = svcCore->Init();
	if (Status.IsError())
		Shutdown();
	
	return Status;
}

VOID CALLBACK CServiceCore__TimerProc(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	CServiceCore* This = (CServiceCore*)lpArgToCompletionRoutine;

	This->OnTimer();
}

DWORD CALLBACK CServiceCore__ThreadProc(LPVOID lpThreadParameter)
{
	CServiceCore* This = (CServiceCore*)lpThreadParameter;

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

#ifdef _DEBUG // todo
	/*if (This->Config()->GetBool("Service", "UseDriver", true)) {
		if (!This->m_Driver->ConnectDrv())
			svcCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed to connect to driver");
	}*/
#endif

	This->m_InitStatus = This->m_pProgramManager->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pProcessList->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pAppIsolator->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pNetIsolator->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pFSIsolator->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pRegIsolator->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pTweakManager->Init();
	if (This->m_InitStatus.IsError()) return -1;

	if (This->Config()->GetBool("Service", "UseETW", true)) {
		if (!This->m_pEtwEventMonitor->Init())
			svcCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed to initialize ETW monitoring");
	}

	//
	// timer being created indicates to CServiceCore::Init() that it can continue
	//

	This->m_hTimer = CreateWaitableTimer(NULL, FALSE, FALSE);

    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -10000000LL; // let the timer start after 1 seconds and the repeat every 250 ms
    if (!SetWaitableTimer(This->m_hTimer, &liDueTime, 250, CServiceCore__TimerProc, This, FALSE))
        return ERR(GetLastWin32ErrorAsNtStatus());

	//
	// Loop waiting for the timer to do something, or for other events
	//

	while (!This->m_Terminate)
		WaitForSingleObjectEx(This->m_hTimer, INFINITE, TRUE);


	CloseHandle(svcCore->m_hTimer);

	delete svcCore;
	svcCore = NULL;

    CoUninitialize();

	return 0;
}

STATUS CServiceCore::Init()
{
	// Initialize this variable here befor any threads are started and later access it without synchronisation for reading only
	m_DataFolder = CConfigIni::GetAppDataFolder() + L"\\" + APP_NAME;
	if (!std::filesystem::exists(m_DataFolder)) 
	{
		if (std::filesystem::create_directories(m_DataFolder)) 
		{
			SetAdminFullControl(m_DataFolder);

			std::wofstream file(m_DataFolder + L"\\Readme.txt");
			if (file.is_open()) {
				file << L"This folder contains data used by " << APP_NAME << std::endl;
				file.close();
			}
		}
	}

	m_pConfig = new CConfigIni(m_DataFolder + L"\\" + APP_NAME + L".ini");

	m_hThread = CreateThread(NULL, 0, CServiceCore__ThreadProc, (void *)this, 0, NULL);
    if (!m_hThread)
        return ERR(GetLastWin32ErrorAsNtStatus());

	while (!m_hTimer) {
		DWORD exitCode;
		if (GetExitCodeThread(m_hThread, &exitCode) && exitCode != STILL_ACTIVE)
			break; // thread terminated prematurely - init failed
		Sleep(100);
	}
	
	if (!m_InitStatus.IsError())
		m_InitStatus = m_pUserPipe->Open(API_SERVICE_PIPE);

	if (!m_InitStatus.IsError())
		m_InitStatus = m_pUserPort->Open(API_SERVICE_PORT);

	return m_InitStatus;
}

void CServiceCore::Shutdown()
{
	if (!svcCore || !svcCore->m_hThread)
		return;

	HANDLE hThread = svcCore->m_hThread;
	svcCore->m_Terminate = true;
	if (WaitForSingleObject(hThread, 10 * 1000) != WAIT_OBJECT_0)
		TerminateThread(hThread, -1);
	CloseHandle(hThread);
}

void CServiceCore::OnTimer()
{
	m_pProcessList->Update();

	m_pNetIsolator->Update();
}