#include "pch.h"

#include <objbase.h>

#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/EvtUtil.h"
#include "../Library/Helpers/AppUtil.h"
#include "ServiceCore.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/IPC/PipeServer.h"
#include "../Library/IPC/AlpcPortServer.h"
#include "Network/NetworkManager.h"
#include "Etw/EtwEventMonitor.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Common/Strings.h"
#include "Processes/ProcessList.h"
#include "Programs/ProgramManager.h"
#include "Network/SocketList.h"
#include "Network/Firewall/Firewall.h"
#include "Access/AccessManager.h"
#include "Volumes/VolumeManager.h"
#include "Tweaks/TweakManager.h"
#include "../Library/Helpers/Service.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Helpers/NtObj.h"
#include "../Library/Common/Exception.h"


CServiceCore* theCore = NULL;


CServiceCore::CServiceCore()
{
#ifdef _DEBUG
	TestVariant();
#endif

	m_pLog = new CEventLogger(API_SERVICE_NAME);

	m_pUserPipe = new CPipeServer();
	m_pUserPort = new CAlpcPortServer();

	m_pProgramManager = new CProgramManager();
	m_pProcessList = new CProcessList();

	m_pAccessManager = new CAccessManager();

	m_pNetworkManager = new CNetworkManager();

	m_pVolumeManager = new CVolumeManager();

	m_pTweakManager = new CTweakManager();

	m_pDriver = new CDriverAPI();

	m_pEtwEventMonitor = new CEtwEventMonitor();

	RegisterUserAPI();
}

CServiceCore::~CServiceCore()
{
	delete m_pEtwEventMonitor;

	delete m_pDriver;

	delete m_pTweakManager;

	delete m_pVolumeManager;

	delete m_pNetworkManager;

	delete m_pAccessManager;

	delete m_pProcessList;
	delete m_pProgramManager;

	delete m_pUserPipe;
	delete m_pUserPort;
}

STATUS CServiceCore::Startup(bool bEngineMode)
{
	if (theCore)
		return ERR(SPAPI_E_DEVINST_ALREADY_EXISTS);

	//
	// Check if our event log exists, and if not create it
	//

	if (!EventSourceExists(API_SERVICE_NAME))
		CreateEventSource(API_SERVICE_NAME, APP_NAME);
	
	if (!EventSourceExists(API_DRIVER_NAME))
		CreateEventSource(API_DRIVER_NAME, APP_NAME);

	// todo start driver if not already started

	theCore = new CServiceCore();
	theCore->m_bEngineMode = bEngineMode;

	theCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, 1, L"Starting Service...");

	STATUS Status = theCore->Init();
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

	NTSTATUS status;

	HANDLE tokenHandle;
	if (NT_SUCCESS(status = NtOpenProcessToken(NtCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle)))
	{
		CHAR privilegesBuffer[FIELD_OFFSET(TOKEN_PRIVILEGES, Privileges) + sizeof(LUID_AND_ATTRIBUTES) * 3];
		PTOKEN_PRIVILEGES privileges;
		ULONG i;

		privileges = (PTOKEN_PRIVILEGES)privilegesBuffer;
		privileges->PrivilegeCount = 3;

		for (i = 0; i < privileges->PrivilegeCount; i++)
		{
			privileges->Privileges[i].Attributes = SE_PRIVILEGE_ENABLED;
			privileges->Privileges[i].Luid.HighPart = 0;
		}

		privileges->Privileges[0].Luid.LowPart = SE_DEBUG_PRIVILEGE;
		privileges->Privileges[1].Luid.LowPart = SE_LOAD_DRIVER_PRIVILEGE;
		privileges->Privileges[2].Luid.LowPart = SE_SECURITY_PRIVILEGE; // set audit policy

		status = NtAdjustPrivilegesToken(tokenHandle, FALSE, privileges, 0, NULL, NULL);

		NtClose(tokenHandle);
	}

	if(!NT_SUCCESS(status))
		theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed to set privileges");

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	SVC_STATE DrvState = GetServiceState(API_DRIVER_NAME);
	if ((DrvState & SVC_INSTALLED) == 0)
	{
		This->m_InitStatus = This->m_pDriver->InstallDrv();
		if (This->m_InitStatus) 
		{
			CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
			DWORD disposition;
			if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\" API_DRIVER_NAME L"\\Parameters", 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disposition) == ERROR_SUCCESS)
			{
				CBuffer Data;
				if (ReadFile(theCore->GetDataFolder() + L"\\KernelIsolator.dat", 0, Data))
				{
					CVariant DriverData;
					try {
						DriverData.FromPacket(&Data, true);

						CVariant ConfigData = DriverData[API_S_CONFIG];
						CBuffer ConfigBuff;
						ConfigData.ToPacket(&ConfigBuff);

						RegSet(hKey, L"Config", L"Data", CVariant(ConfigBuff));
						if (DriverData.Has(API_S_USER_KEY))
						{
							CVariant UserKey = DriverData[API_S_USER_KEY];
							RegSet(hKey, L"UserKey", L"PublicKey", UserKey[API_S_PUB_KEY]);
							if (UserKey.Has(API_S_KEY_BLOB)) RegSet(hKey, L"UserKey", L"KeyBlob", UserKey[API_S_KEY_BLOB]);
						}
					}
					catch (const CException&) {
						This->m_InitStatus = ERR(STATUS_UNSUCCESSFUL);
					}
				}
			}
		}
		if (This->m_InitStatus.IsError()) {
			theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed to install driver");
			return -1;
		}
	}

	This->m_InitStatus = This->m_pDriver->ConnectDrv();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed to connect to driver");
		return -1;
	}

	if(This->m_pDriver->GetABIVersion() != MY_ABI_VERSION) {
		theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Driver ABI version mismatch");
		return -1;
	}

	This->m_InitStatus = This->m_pProgramManager->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pProcessList->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pAccessManager->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pNetworkManager->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pVolumeManager->Init();
	if (This->m_InitStatus.IsError()) return -1;

	This->m_InitStatus = This->m_pTweakManager->Init();
	if (This->m_InitStatus.IsError()) return -1;

	if (This->Config()->GetBool("Service", "UseETW", true)) {
		if (!This->m_pEtwEventMonitor->Init())
			theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, 1, L"Failed to initialize ETW monitoring");
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


	CloseHandle(theCore->m_hTimer);

	This->m_pProgramManager->Store();

	This->m_pVolumeManager->DismountAll();

	delete theCore;
	theCore = NULL;

    CoUninitialize();

	return 0;
}

STATUS CServiceCore::Init()
{
	m_AppDir = GetApplicationDirectory();

	// Initialize this variable here befor any threads are started and later access it without synchronisation for reading only
	m_DataFolder = CConfigIni::GetAppDataFolder() + L"\\" + GROUP_NAME + L"\\" + APP_NAME;
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
	if (!theCore || !theCore->m_hThread)
		return;
	HANDLE hThread = theCore->m_hThread;

	theCore->m_pDriver->Disconnect();
	if (theCore->m_bEngineMode)
	{
		KillService(API_DRIVER_NAME);

		CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\" API_DRIVER_NAME L"\\Parameters", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			CVariant Config = RegQuery(hKey, L"Config", L"Data");
			CBuffer ConfigBuff = Config;
			CVariant ConfigData;			
			ConfigData.FromPacket(&ConfigBuff, true);

			CVariant KeyBlob = RegQuery(hKey, L"UserKey", L"KeyBlob");
			CVariant PublicKey = RegQuery(hKey, L"UserKey", L"PublicKey");

			CVariant DriverData;
			DriverData[API_S_CONFIG] = ConfigData;
			if (PublicKey.GetSize() > 0) {
				CVariant UserKey;
				UserKey[API_S_PUB_KEY] = PublicKey;
				if (KeyBlob.GetSize() > 0) UserKey[API_S_KEY_BLOB] = KeyBlob;
				DriverData[API_S_USER_KEY] = UserKey;
			}

			CBuffer Data;
			DriverData.ToPacket(&Data);
			WriteFile(theCore->GetDataFolder() + L"\\KernelIsolator.dat", 0, Data);

#ifndef _DEBUG
			RemoveService(API_DRIVER_NAME);
#endif
		}
	}
	theCore->m_Terminate = true;

	if (!theCore->m_bEngineMode) {
		if (WaitForSingleObject(hThread, 10 * 1000) != WAIT_OBJECT_0)
			TerminateThread(hThread, -1);
		CloseHandle(hThread);
	}
}

void CServiceCore::OnTimer()
{
	m_pProcessList->Update();

	m_pNetworkManager->Update();

	m_pProgramManager->Update();

	m_pAccessManager->Update();
}