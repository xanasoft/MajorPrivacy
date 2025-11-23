#include "pch.h"

#include <objbase.h>
#include <userenv.h>

#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/TokenUtil.h"
#include "ServiceCore.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/IPC/PipeServer.h"
#include "../Library/IPC/AlpcPortServer.h"
#include "Network/NetworkManager.h"
#include "Network/Firewall/Firewall.h"
#include "Network/Dns/DnsFilter.h"
#include "Etw/EtwEventMonitor.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Common/Strings.h"
#include "Processes/ProcessList.h"
#include "Enclaves/EnclaveManager.h"
#include "HashDB/HashDB.h"
#include "Programs/ProgramManager.h"
#include "Network/SocketList.h"
#include "Access/AccessManager.h"
#include "Volumes/VolumeManager.h"
#include "Tweaks/TweakManager.h"
#include "../Library/Helpers/Service.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Helpers/NtObj.h"
#include "../Library/Common/Exception.h"
#include "../MajorPrivacy/version.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "JSEngine/JSEngine.h"
#include "Common/EventLog.h"
#include "../Library/Hooking/HookUtils.h"
#include "../Library/Helpers/WinUtil.h"
#include "Presets/PresetManager.h"

CServiceCore* theCore = NULL;

FW::DefaultMemPool g_DefaultMemPool;

extern BOOLEAN				 g_UnloadProtection;
extern SERVICE_STATUS        g_ServiceStatus;
extern SERVICE_STATUS_HANDLE g_ServiceStatusHandle;

CServiceCore::CServiceCore()
	: m_Pool(4)
{
	m_pMemPool = FW::MemoryPool::Create();

	m_pSysLog = new CEventLogger(API_SERVICE_NAME);
	m_pEventLog = new CEventLog();

	m_pUserPipe = new CPipeServer();
	m_pUserPort = new CAlpcPortServer();

	m_pEnclaveManager = new CEnclaveManager();

	m_pHashDB = new CHashDB();

	m_pProgramManager = new CProgramManager();

	m_pProcessList = new CProcessList();

	m_pAccessManager = new CAccessManager();

	m_pNetworkManager = new CNetworkManager();

	m_pVolumeManager = new CVolumeManager();

	m_pTweakManager = new CTweakManager();

	m_pPresetManager = new CPresetManager();

	m_pDriver = new CDriverAPI();

	m_pEtwEventMonitor = new CEtwEventMonitor();

	RegisterUserAPI();

	CNtPathMgr::Instance()->RegisterDeviceChangeCallback(DeviceChangedCallback, this);
}

CServiceCore::~CServiceCore()
{
	m_pUserPipe->Close();
	m_pUserPort->Close();

	delete m_pEtwEventMonitor;

	delete m_pDriver;

	delete m_pPresetManager;

	delete m_pTweakManager;

	delete m_pVolumeManager;

	delete m_pNetworkManager;

	delete m_pAccessManager;

	delete m_pProcessList;

	delete m_pProgramManager;

	delete m_pEnclaveManager;

	delete m_pHashDB;

	delete m_pUserPipe;
	delete m_pUserPort;

	delete m_pSysLog;
	delete m_pEventLog;

	delete m_pConfig; // could be NULL

	CNtPathMgr::Instance()->UnRegisterDeviceChangeCallback(DeviceChangedCallback, this);

	FW::MemoryPool::Destroy(m_pMemPool);
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

	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Starting PrivacyAgent v%S", VERSION_STR);
	ULONGLONG Start = GetTickCount64();

	STATUS Status = theCore->Init();
	if (Status.IsError()) {
		Shutdown(eShutdown_Wait);
		return Status;
	}
	
	ULONGLONG End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Startup took %llu ms", End - Start);

	return OK;
}

VOID CALLBACK CServiceCore__TimerProc(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	CServiceCore* This = (CServiceCore*)lpArgToCompletionRoutine;

	This->OnTimer();
}

STATUS CServiceCore::InitDriver()
{
	SVC_STATE DrvState = GetServiceState(API_DRIVER_NAME);

	//
	// Check if the driver is ours
	//

	if ((DrvState & SVC_INSTALLED) == SVC_INSTALLED && (DrvState & SVC_RUNNING) != SVC_RUNNING)
	{
		std::wstring BinaryPath = GetServiceBinaryPath(API_DRIVER_NAME);

		std::wstring ServicePath = CServiceCore::NormalizePath(BinaryPath);
		std::wstring AppDir = CServiceCore::NormalizePath(m_AppDir);
		if (ServicePath.length() < AppDir.length() || _wcsnicmp(ServicePath.c_str(), AppDir.c_str(), AppDir.length()) != 0)
		{
			theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Updated driver, old path: %s; new path: %s", ServicePath.c_str(), AppDir.c_str());
			RemoveService(API_DRIVER_NAME);
			DrvState = SVC_NOT_FOUND;
		}
	}

	//
	// Install the driver
	//

	if ((DrvState & SVC_INSTALLED) == 0)
	{
		InstallDriver(false);
	}

	//
	// Connect to the driver, its started when not running
	//

	STATUS Status = m_pDriver->ConnectDrv();
	if(Status.IsError())
		return Status;

	g_UnloadProtection = m_pDriver->GetConfigBool("UnloadProtection", false);
	if (g_UnloadProtection)
	{
		m_pDriver->AcquireUnloadProtection();

		if (g_ServiceStatusHandle) {
			g_ServiceStatus.dwControlsAccepted &= ~SERVICE_ACCEPT_STOP;
			SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
		}
	}

	return OK;
}

STATUS CServiceCore::PrepareShutdown()
{
	LONG UnloadProtectionCount = 0;
	if (g_UnloadProtection) 
	{
		UnloadProtectionCount = m_pDriver->ReleaseUnloadProtection();

		if (g_ServiceStatusHandle) {
			g_ServiceStatus.dwControlsAccepted |= SERVICE_ACCEPT_STOP;
			SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
		}

		g_UnloadProtection = FALSE;
	}

	if (UnloadProtectionCount > 1 && theCore->m_bEngineMode)
	{
		// todo kill all protected processes
	}

	return OK;
}

void CServiceCore::CloseDriver()
{
	theCore->m_pDriver->Disconnect();

	if (theCore->m_bEngineMode)
	{
		RemoveDriver();
	}
}

STATUS CServiceCore::InstallDriver(bool bAutoStart)
{
	STATUS Status = CDriverAPI::InstallDrv(bAutoStart);
	if (!Status)
		return Status;
	
	CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
	DWORD disposition;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\" API_DRIVER_NAME L"\\Parameters", 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disposition) != ERROR_SUCCESS)
		return ERR(STATUS_UNSUCCESSFUL);
	
	std::wstring DataFolder = CConfigIni::GetAppDataFolder() + L"\\" + GROUP_NAME + L"\\" + APP_NAME;

	CBuffer Buffer;
	if (ReadFile(DataFolder + L"\\KernelIsolator.dat", Buffer))
	{
		StVariant DriverData;
		//try {
		auto ret = DriverData.FromPacket(&Buffer, true);

		StVariant ConfigData = DriverData[API_S_CONFIG];
		CBuffer ConfigBuff;
		ConfigData.ToPacket(&ConfigBuff);

		RegSet(hKey, L"Config", L"Data", StVariant(ConfigBuff));
		if (DriverData.Has(API_S_USER_KEY))
		{
			StVariant UserKey = DriverData[API_S_USER_KEY];
			RegSet(hKey, L"UserKey", L"PublicKey", UserKey[API_S_PUB_KEY]);
			if (UserKey.Has(API_S_KEY_BLOB)) RegSet(hKey, L"UserKey", L"KeyBlob", UserKey[API_S_KEY_BLOB]);
		}
		//}
		//catch (const CException&) {
		if (ret != StVariant::eErrNone)
			Status = ERR(STATUS_UNSUCCESSFUL);
		//}
	}
	// else file not found thats ok means first start

	return OK;
}

STATUS CServiceCore::StopDriver()
{
	STATUS Status = OK;
	SVC_STATE SvcState = GetServiceState(API_DRIVER_NAME);
	if ((SvcState & SVC_RUNNING) == SVC_RUNNING)
		Status = KillService(API_DRIVER_NAME);

	if (!Status)
	{
		for (int i = 0; i < 10; i++)
		{
			SVC_STATE SvcState = GetServiceState(API_DRIVER_NAME);
			if ((SvcState & SVC_RUNNING) != SVC_RUNNING)
				break;

			UNICODE_STRING uni;
			RtlInitUnicodeString(&uni, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" API_DRIVER_NAME);
			NtUnloadDriver(&uni);

			Sleep((i + 1) * 100);
		}
	}

	SvcState = GetServiceState(API_DRIVER_NAME);
	if ((SvcState & SVC_RUNNING) == SVC_RUNNING)
		return ERR(STATUS_UNSUCCESSFUL);

	return OK;
}


STATUS CServiceCore::RemoveDriver()
{
	STATUS Status = StopDriver();
	if(!Status)
		return Status;

	CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\" API_DRIVER_NAME L"\\Parameters", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return OK; // no key, nothing to do
	
	StVariant Config = RegQuery(hKey, L"Config", L"Data");
	CBuffer ConfigBuff = Config;
	StVariant ConfigData;
	ConfigData.FromPacket(&ConfigBuff, true);

	StVariant KeyBlob = RegQuery(hKey, L"UserKey", L"KeyBlob");
	StVariant PublicKey = RegQuery(hKey, L"UserKey", L"PublicKey");

	StVariant DriverData;
	DriverData[API_S_CONFIG] = ConfigData;
	if (PublicKey.GetSize() > 0) {
		StVariant UserKey;
		UserKey[API_S_PUB_KEY] = PublicKey;
		if (KeyBlob.GetSize() > 0) UserKey[API_S_KEY_BLOB] = KeyBlob;
		DriverData[API_S_USER_KEY] = UserKey;
	}

	std::wstring DataFolder = CConfigIni::GetAppDataFolder() + L"\\" + GROUP_NAME + L"\\" + APP_NAME;

	CBuffer Buffer;
	DriverData.ToPacket(&Buffer);
	WriteFile(DataFolder + L"\\KernelIsolator.dat", Buffer);

	return RemoveService(API_DRIVER_NAME);
}

DWORD CALLBACK CServiceCore__ThreadProc(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
	MySetThreadDescription(GetCurrentThread(), L"CServiceCore__ThreadProc");
#endif

	CServiceCore* This = (CServiceCore*)lpThreadParameter;

	ULONGLONG End, Start;

	//
	// init COM for this thread
	//

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	//
	// LoadEventLog
	// 

	This->m_pEventLog->Load();

	//
	// Initialize the Components
	//

	Start = GetTickCount64();

	This->m_InitStatus = This->m_pEnclaveManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Enclave Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	This->m_InitStatus = This->m_pHashDB->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init HashDB, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Enclave Manager init took %llu ms", End - Start);
	Start = End;

	This->m_InitStatus = This->m_pProgramManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Program Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Program Manager init took %llu ms", End - Start);
	Start = End;
	
	This->m_InitStatus = This->m_pProcessList->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Process List, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}
	
	End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Process List init took %llu ms", End - Start);
	Start = End;

	This->m_InitStatus = This->m_pAccessManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Access Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Enclave Access init took %llu ms", End - Start);
	Start = End;

	This->m_InitStatus = This->m_pNetworkManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Network Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Network Manager init took %llu ms", End - Start);
	Start = End;

	This->m_InitStatus = This->m_pVolumeManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Volume Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Volume Manager init took %llu ms", End - Start);
	Start = End;
	
	This->m_InitStatus = This->m_pTweakManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Tweak Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}
	
	This->m_InitStatus = This->m_pPresetManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Preset Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Tweak Manager init took %llu ms", End - Start);
	Start = End;

	if (This->Config()->GetBool("Service", "UseETW", true)) {
		if (!This->m_pEtwEventMonitor->Init())
			theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to initialize ETW Monitoring");
	}

	End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent ETW Monitoring init took %llu ms", End - Start);

	//
	// timer being created indicates to CServiceCore::Init() that it can continue
	//

	This->m_hTimer = CreateWaitableTimer(NULL, FALSE, FALSE);

    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -10000000LL; // let the timer start after 1 seconds and the repeat every 250 ms
	if (!SetWaitableTimer(This->m_hTimer, &liDueTime, DEF_CORE_TIMER_INTERVAL, CServiceCore__TimerProc, This, FALSE))
        return ERR(GetLastWin32ErrorAsNtStatus());

	//
	// Loop waiting for the timer to do something, or for other events
	//

	while (!This->m_Shutdown)
		WaitForSingleObjectEx(This->m_hTimer, INFINITE, TRUE);


	CloseHandle(theCore->m_hTimer);

	//if (This->IsConfigDirty())
	This->CommitConfig();

	This->Driver()->StoreConfigChanges(This->m_Shutdown == CServiceCore::eShutdown_System);

	This->m_LastStoreTime = GetTickCount64();
	This->StoreRecords();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent StoreRecords took %llu ms", GetTickCount64() - This->m_LastStoreTime);
	//DbgPrint(L"StoreRecords took %llu ms\n", GetTickCount64() - This->m_LastStoreTime);
	

	Start = GetTickCount64();
	This->m_pVolumeManager->DismountAll();
	End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent DismountAll took %llu ms", End - Start);

cleanup:

	This->m_pEventLog->Store();

	if (theCore->m_pDriver->IsConnected())
		This->CloseDriver();

	if (This->m_Shutdown == CServiceCore::eShutdown_NoWait) {
		delete theCore;
		theCore = NULL;
	}

	CoUninitialize();

	return 0;
}

STATUS CServiceCore::Init()
{
	/*{
		CJSEngine JSEngine(R"xxx(

        function func(arg) {
            return "test: " + arg.x;
        }

        //1 + 2;

        logLine("test");

        )xxx");

		JSEngine.RunScript();

		StVariant Var;
		Var["x"] = "test";

		auto Ret = JSEngine.CallFunction("func", Var);
		auto sRet = Ret.GetValue().AsStr();
		sRet.clear();
	}*/

	m_AppDir = GetApplicationDirectory();

	ConfigInit();

	//
	// Setup required privileges
	//

	CScopedHandle tokenHandle = CScopedHandle((HANDLE)0, NtClose);
	NTSTATUS status = NtOpenProcessToken(NtCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle);
	if (NT_SUCCESS(status))
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
	}
	if(!NT_SUCCESS(status))
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to set privileges, error: 0x%08X", status);

	//
	// init hooks
	//

	m_InitStatus = InitHooks();
	if (m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to initialize hooks, error: 0x%08X", m_InitStatus.GetStatus());
		return m_InitStatus;
	}

	//
	// Initialize the driver
	//

	ULONGLONG Start = GetTickCount64();

	m_InitStatus = InitDriver();
	if (m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to connect to driver, error: 0x%08X", m_InitStatus.GetStatus());
		return m_InitStatus;
	}

	ULONGLONG End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Driver init took %llu ms", End - Start);

	uint32 uDrvABI = m_pDriver->GetABIVersion();
	if (uDrvABI != MY_ABI_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Driver ABI version mismatch, expected: %06X, got %06X", MY_ABI_VERSION, uDrvABI);
		return m_InitStatus;
	}

#ifndef _DEBUG
	if (!theCore->Driver()->IsCurProcMaxSecurity() && (theCore->Driver()->IsCurProcHighSecurity() || theCore->Driver()->IsCurProcLowSecurity()))
		return ERR(STATUS_SYNCHRONIZATION_REQUIRED);
#endif

	if (!m_pDriver->TestDevAuthority())
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"MajorPrivacy's Service (PrivacyAgent) is not being recognized by the driver (KernelIsolator)!!!");

	//
	// Start Engien Thread and initialize Components
	//

	m_LastStoreTime = GetTickCount64();

	m_hThread = CreateThread(NULL, 0, CServiceCore__ThreadProc, (void *)this, 0, NULL);
    if (!m_hThread)
        return ERR(GetLastWin32ErrorAsNtStatus());

	//
	// Wait for the thread to initialize
	//

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

void CServiceCore::ConfigInit()
{
	// Initialize this variable here befor any threads are started and later access it without synchronisation for reading only
	m_DataFolder = CConfigIni::GetAppDataFolder() + L"\\" + GROUP_NAME + L"\\" + APP_NAME;
	if (!std::filesystem::exists(m_DataFolder)) 
	{
		if (std::filesystem::create_directories(m_DataFolder)) 
		{
			SetAdminFullControlAllowUsersRead(m_DataFolder);

			//std::wofstream file(m_DataFolder + L"\\Readme.txt");
			//if (file.is_open()) {
			//	file << L"This folder contains data used by " << APP_NAME << std::endl;
			//	file.close();
			//}
		}
	}

	m_pConfig = new CConfigIni(m_DataFolder + L"\\" + API_SERVICE_NAME + L".ini");

	//
	// Update config
	//

	if (m_pConfig->GetInt("Service", "ConfigLevel", 0) < 1)
	{
		m_pConfig->SetInt("Service", "ConfigLevel", 1);

		m_pConfig->SetValue("Service", "DnsResolvers", L"8.8.8.8;1.1.1.1");
		m_pConfig->SetValue("Service", "DnsTestHost", L"example.com");

		std::vector<std::wstring> DnsBlockLists;
		DnsBlockLists.push_back(L"https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts");
		DnsBlockLists.push_back(L"http://sysctl.org/cameleon/hosts");
		DnsBlockLists.push_back(L"https://s3.amazonaws.com/lists.disconnect.me/simple_tracking.txt");
		DnsBlockLists.push_back(L"https://s3.amazonaws.com/lists.disconnect.me/simple_ad.txt");

		m_pConfig->SetValue("Service", "DnsBlockLists", JoinStr(DnsBlockLists, L";"));
	}
}

void CServiceCore::Shutdown(EShutdownMode eMode)
{
	if (!theCore || !theCore->m_hThread)
		return;
	HANDLE hThread = theCore->m_hThread;

	theCore->m_Shutdown = eMode;
	if (theCore->m_Shutdown == eShutdown_NoWait) 
		return;

	if (WaitForSingleObject(hThread, 10 * 1000) != WAIT_OBJECT_0)
		TerminateThread(hThread, -1);
	CloseHandle(hThread);

	delete theCore;
	theCore = NULL;
}

void CServiceCore::StoreRecords(bool bAsync)
{
	m_pProgramManager->Store();

	if(bAsync)
		m_pProcessList->StoreAsync();
	else
		m_pProcessList->Store();

	if(bAsync)
		m_pAccessManager->StoreAsync();
	else
		m_pAccessManager->Store();

	if (bAsync)
		m_pNetworkManager->StoreAsync();
	else
		m_pNetworkManager->Store();
}

void CServiceCore::Reconfigure(const std::string& Key)
{
	if (Key == "LogRegistry")
		m_pProcessList->Reconfigure();

	//m_pProgramManager->Reconfigure();

	//m_pAccessManager->Reconfigure();

	if (Key == "FwAutoApprove" || Key == "FwAutoReject")
		m_pNetworkManager->Firewall()->UpdateAutoGuard();

	if(Key.substr(0, 3) == "Dns")
		m_pNetworkManager->Reconfigure(Key == "DnsResolvers", Key == "DnsBlockLists");

	//m_pEtwEventMonitor->Reconfigure();

	if (Key == "EncryptPageFile") {
		bool bNtfsEncryptPagingFile = m_pConfig->GetBool("Service", "EncryptPageFile", false);
		RegSetDWord(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\FileSystem", L"NtfsEncryptPagingFile", bNtfsEncryptPagingFile ? 1 : 0);
	}
}

void CServiceCore::RefreshConfig(const std::string& Key)
{
	if (Key == "EncryptPageFile") {
		m_pConfig->SetBool("Service", "EncryptPageFile", RegQueryDWord(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\FileSystem", L"NtfsEncryptPagingFile", 0) == 1);
	}
}

STATUS CServiceCore::CommitConfig()
{
	m_pProgramManager->Store();

	m_pNetworkManager->DnsFilter()->Store();
	m_pNetworkManager->Firewall()->Store();

	m_pTweakManager->Store();

	m_pPresetManager->Store();

	m_bConfigDirty = false;

	return OK;
}

STATUS CServiceCore::DiscardConfig()
{
	// todo: reload config from disk
	m_bConfigDirty = false;
	return OK;
}

#ifdef _DEBUG
size_t getHeapUsage() 
{
	_HEAPINFO heapInfo;
	heapInfo._pentry = nullptr;
	size_t usedMemory = 0;

	while (_heapwalk(&heapInfo) == _HEAPOK) {
		if (heapInfo._useflag == _USEDENTRY) {
			usedMemory += heapInfo._size;
		}
	}

	return usedMemory;
}
#endif

void CServiceCore::OnTimer()
{
	m_pProcessList->Update();

	m_pNetworkManager->Update();

	m_pProgramManager->Update();

	m_pAccessManager->Update();

	m_pEnclaveManager->Update();

	m_pHashDB->Update();

	if (m_NextLogTime && m_NextLogTime < GetTickCount64()) {
		m_NextLogTime = 0;
		m_pEventLog->Store();
	}

	if (m_LastStoreTime + 60 * 60 * 1000 < GetTickCount64()) // once per hours
	{
		m_LastStoreTime = GetTickCount64();
		StoreRecords(true);
		DbgPrint(L"Async StoreRecords took %llu ms\n", GetTickCount64() - m_LastStoreTime);
	}

	if (m_LastCheckTime + 5 * 60 * 1000 < GetTickCount64()) // every 5 minutes
	{
		m_LastCheckTime = GetTickCount64();
		m_pTweakManager->CheckTweaks();
		if(m_pTweakManager->AreTweaksDirty())
			m_pTweakManager->Store();
	}

#ifdef _DEBUG
	static uint64 LastObjectDump = GetTickCount64();
	if (LastObjectDump + 60 * 1000 < GetTickCount64()) {
		LastObjectDump = GetTickCount64();
//		ObjectTrackerBase::PrintCounts();

		//size_t memoryUsed = getHeapUsage();
		//DbgPrint(L"USED MEMORY: %llu bytes\n", memoryUsed);
	}
#endif
}

CJSEnginePtr CServiceCore::GetScript(const CFlexGuid& Guid, EItemType Type)
{
	switch (Type) {
	case EItemType::eEnclave:
		if(CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(Guid))
			return pEnclave->GetScriptEngine();
		break;
	case EItemType::eExecRule: 
		if(CProgramRulePtr pRule = theCore->ProgramManager()->GetRule(Guid))
			return pRule->GetScriptEngine();
		break;
	case EItemType::eResRule: 
		if (CAccessRulePtr pRule = theCore->AccessManager()->GetRule(Guid))
			return pRule->GetScriptEngine();
		break;
	case EItemType::eVolume:
		if (CVolumePtr pVolume = theCore->VolumeManager()->GetVolume(Guid))
			return pVolume->GetScriptEngine();
		break;
	case EItemType::ePreset: 
		if (CPresetPtr pPreset = theCore->PresetManager()->GetPreset(Guid))
			return pPreset->GetScriptEngine();
		break;
	}
	return nullptr;
}

void CServiceCore::DeviceChangedCallback(void* param)
{
	CServiceCore* This = (CServiceCore*)param;

	// todo: handle device change
}

// DOS Prefixes
// L"\\\\?\\" prefix is used to bypass the MAX_PATH limitation
// L"\\\\.\\" points to L"\Device\"

// NT Prefixes
// L"\\??\\" prefix (L"\\GLOBAL??\\") maps DOS letters and more

std::wstring CServiceCore::NormalizePath(std::wstring Path, bool bLowerCase)
{
	if(Path.empty() || Path[0] == L'*')
		return Path;

	if (Path.length() >= 7 && Path.compare(0, 4, L"\\??\\") == 0 && Path.compare(5, 2, L":\\") == 0) // \??\X:\...
		Path.erase(0, 4);
	else if (Path.length() >= 7 && Path.compare(0, 4, L"\\\\?\\") == 0 && Path.compare(5, 2, L":\\") == 0) // \\?\X:\...
		Path.erase(0, 4);

	if (Path.find(L'%') != std::wstring::npos)
		Path = ExpandEnvironmentVariablesInPath(Path);

	if (MatchPathPrefix(Path, L"\\SystemRoot")) {
		static WCHAR windir[MAX_PATH + 8] = { 0 };
		if (!*windir) GetWindowsDirectoryW(windir, MAX_PATH);
		Path = windir + Path.substr(11);
	}

	if (!CNtPathMgr::IsDosPath(Path) && !MatchPathPrefix(Path, L"\\REGISTRY"))
	{
		//if (MatchPathPrefix(Path, L"\\Device"))
		//	Path = L"\\\\.\\" + Path.substr(8);
		
		if (Path.length() >= 4 && Path.compare(0, 4, L"\\\\.\\") == 0) 
			Path = L"\\Device\\" + Path.substr(4);

		if (MatchPathPrefix(Path, L"\\Device"))
		{
			//std::wstring DosPath = CNtPathMgr::Instance()->TranslateNtToDosPath(L"\\Device\\" + Path.substr(4));
			std::wstring DosPath = CNtPathMgr::Instance()->TranslateNtToDosPath(Path);
			if (!DosPath.empty())
				Path = DosPath;
#ifdef _DEBUG
			else
				DbgPrint("Encountered Unresolved NtPath %S\n", Path.c_str());
#endif
		}
#ifdef _DEBUG
		else
			DbgPrint("Encountered Unecpected NtPath %S\n", Path.c_str());
#endif
	}

	return bLowerCase ? MkLower(Path) : Path;
}

void CServiceCore::ImpersonateCaller(uint32 CallerPID)
{
	if (CallerPID == 0)
		return;
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, FALSE, CallerPID);
	if (hProcess == NULL)
		return;
	HANDLE hToken = NULL;
	if (OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE, &hToken))
	{
		HANDLE hDupToken = NULL;
		if (DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &hDupToken))
		{
			SetThreadToken(NULL, hDupToken);
			CloseHandle(hDupToken);
		}
		CloseHandle(hToken);
	}
	CloseHandle(hProcess);
}

#include <sddl.h> // For ConvertSidToStringSid

std::wstring CServiceCore::GetCallerSID(uint32 CallerPID)
{
	CScopedHandle hProcess = CScopedHandle(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, CallerPID), CloseHandle);
	if (!hProcess)
		return L"";

	CScopedHandle hToken = CScopedHandle((HANDLE)0, CloseHandle);
	if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
		return L"";

	DWORD tokenInfoLength = 0;
	GetTokenInformation(hToken, TokenUser, NULL, 0, &tokenInfoLength);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return L"";

	CScoped tokenUserData = new BYTE[tokenInfoLength];
	if (!GetTokenInformation(hToken, TokenUser, tokenUserData, tokenInfoLength, &tokenInfoLength))
		return L"";

	std::wstring sidStringOut;

	LPWSTR sidString = nullptr;
	if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(tokenUserData.Val())->User.Sid, &sidString)) 
	{
		sidStringOut = sidString;
		LocalFree(sidString);
	}
	
	return sidStringOut;
}

STATUS CServiceCore::CreateUserProcess(const std::wstring& CommandLine, uint32 CallerPID, uint32 Flags, const std::wstring& WorkingDir, uint32* pProcessId, HANDLE* pProcessHandle)
{
	if (CommandLine.empty())
		return ERR(STATUS_INVALID_PARAMETER);

	STATUS Status = STATUS_SUCCESS;
	bool bIsServiceMode = !m_bEngineMode;
	bool bAsSystem = (Flags & eExec_AsSystem) != 0;

	CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hToken(NULL, CloseHandle);
	CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hCallerToken(NULL, CloseHandle);
	ULONG CallerSession = 0;

	if (bAsSystem)
	{
		// Get the caller's session ID
		DWORD dwCallerSession = 0;
		if (ProcessIdToSessionId(CallerPID, &dwCallerSession))
		{
			CallerSession = dwCallerSession;
		}

		const ULONG TOKEN_RIGHTS = TOKEN_QUERY | TOKEN_DUPLICATE
			| TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID
			| TOKEN_ADJUST_GROUPS | TOKEN_ASSIGN_PRIMARY;

		if (bIsServiceMode)
		{
			// We're already running as SYSTEM - use current process token
			if (!OpenProcessToken(GetCurrentProcess(), TOKEN_RIGHTS, &hToken))
				return ERR(GetLastWin32ErrorAsNtStatus());
		}
		else
		{
			// We're running as admin user - duplicate SYSTEM token from an existing SYSTEM process
			// This requires SeDebugPrivilege which is available to administrators and we already have

			// Try to find a SYSTEM process from our process list
			auto processList = theCore->ProcessList()->List();
			for (const auto& pair : processList)
			{
				CProcessPtr pProcess = pair.second;
				std::wstring name = pProcess->GetName();

				// Try winlogon.exe or services.exe
				if (_wcsicmp(name.c_str(), L"winlogon.exe") == 0 || _wcsicmp(name.c_str(), L"services.exe") == 0)
				{
					DWORD pid = (DWORD)pProcess->GetProcessId();
					HANDLE hSystemProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
					if (hSystemProcess)
					{
						HANDLE hSystemToken = NULL;
						if (OpenProcessToken(hSystemProcess, TOKEN_DUPLICATE | TOKEN_QUERY, &hSystemToken))
						{
							// Verify it's actually a SYSTEM token
							DWORD dwSize = 0;
							if (GetTokenInformation(hSystemToken, TokenUser, NULL, 0, &dwSize) == FALSE && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
							{
								BYTE* pTokenUser = new BYTE[dwSize];
								if (GetTokenInformation(hSystemToken, TokenUser, pTokenUser, dwSize, &dwSize))
								{
									PTOKEN_USER pUser = (PTOKEN_USER)pTokenUser;
									SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
									PSID pSystemSid = NULL;
									if (AllocateAndInitializeSid(&ntAuthority, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &pSystemSid))
									{
										if (EqualSid(pUser->User.Sid, pSystemSid))
										{
											// Found a SYSTEM token - duplicate it
											hToken.Set(hSystemToken);
											hSystemToken = NULL;
										}
										FreeSid(pSystemSid);
									}
								}
								delete[] pTokenUser;
							}

							if (hSystemToken)
								CloseHandle(hSystemToken);
						}
						CloseHandle(hSystemProcess);

						if (hToken)
							break;
					}
				}
			}

			if (!hToken)
				return ERR(STATUS_PRIVILEGE_NOT_HELD);
		}

		// Duplicate the SYSTEM token
		CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hNewToken(NULL, CloseHandle);
		if (!DuplicateTokenEx(hToken, TOKEN_RIGHTS, NULL, SecurityAnonymous, TokenPrimary, &hNewToken))
			return ERR(GetLastWin32ErrorAsNtStatus());

		hToken.Set(hNewToken.Detach());

		// Set the session ID to the caller's session
		if (!SetTokenInformation(hToken, TokenSessionId, &CallerSession, sizeof(ULONG)))
			return ERR(GetLastWin32ErrorAsNtStatus());
	}
	else
	{
		// Open the caller's process to get its token
		CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hCallerProcess(
			OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, FALSE, CallerPID),
			CloseHandle);
		if (!hCallerProcess)
			return ERR(GetLastWin32ErrorAsNtStatus());

		// Get the caller's token
		if (!OpenProcessToken(hCallerProcess, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, &hCallerToken))
			return ERR(GetLastWin32ErrorAsNtStatus());

		// Duplicate the token for modification
		if (!DuplicateTokenEx(hCallerToken, MAXIMUM_ALLOWED, NULL, SecurityAnonymous, TokenPrimary, &hToken))
			return ERR(GetLastWin32ErrorAsNtStatus());
	}

	// Check if we need to modify the token privileges
	bool bDropAdmin = (Flags & eExec_DropAdmin) != 0;
	bool bLowPrivilege = (Flags & eExec_LowPrivilege) != 0;
	bool bElevate = (Flags & eExec_Elevate) != 0;
	bool bCallerIsAdmin = hCallerToken ? TokenIsAdmin(hCallerToken, false) : false;

	CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hModifiedToken(NULL, CloseHandle);

	// If caller is not admin and service is not running as SYSTEM, drop admin rights
	if (!bAsSystem && bIsServiceMode && !bCallerIsAdmin)
		bDropAdmin = true;

	if (bLowPrivilege)
	{
		// Create a restricted token with low privileges
		SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
		SID_IDENTIFIER_AUTHORITY WorldAuthority = SECURITY_WORLD_SID_AUTHORITY;

		// SIDs to disable
		PSID pAdminSid = NULL, pUsersSid = NULL, pEveryoneSid = NULL;
		AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminSid);
		AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &pUsersSid);
		AllocateAndInitializeSid(&WorldAuthority, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pEveryoneSid);

		SID_AND_ATTRIBUTES SidsToDisable[3];
		DWORD dwSidCount = 0;

		if (pAdminSid) {
			SidsToDisable[dwSidCount].Sid = pAdminSid;
			SidsToDisable[dwSidCount].Attributes = 0;
			dwSidCount++;
		}

		// Create restricted token
		HANDLE hRestrictedToken = NULL;
		if (CreateRestrictedToken(hToken, DISABLE_MAX_PRIVILEGE, dwSidCount, SidsToDisable, 0, NULL, 0, NULL, &hRestrictedToken))
		{
			hModifiedToken.Set(hRestrictedToken);

			// Set integrity level to Low
			SID_IDENTIFIER_AUTHORITY MandatoryLabelAuthority = SECURITY_MANDATORY_LABEL_AUTHORITY;
			PSID pIntegritySid = NULL;
			if (AllocateAndInitializeSid(&MandatoryLabelAuthority, 1, SECURITY_MANDATORY_LOW_RID, 0, 0, 0, 0, 0, 0, 0, &pIntegritySid))
			{
				TOKEN_MANDATORY_LABEL tml = { 0 };
				tml.Label.Attributes = SE_GROUP_INTEGRITY;
				tml.Label.Sid = pIntegritySid;
				SetTokenInformation(hModifiedToken, TokenIntegrityLevel, &tml, sizeof(tml) + GetLengthSid(pIntegritySid));
				FreeSid(pIntegritySid);
			}
		}

		if (pAdminSid) FreeSid(pAdminSid);
		if (pUsersSid) FreeSid(pUsersSid);
		if (pEveryoneSid) FreeSid(pEveryoneSid);

		if (!hModifiedToken)
			return ERR(GetLastWin32ErrorAsNtStatus());
	}
	else if (bElevate && !bAsSystem)
	{
		// Elevate to admin rights by using the linked token (UAC split token)
		// This only works if the caller is in the Administrators group but running with a filtered token
		TOKEN_ELEVATION_TYPE elevationType;
		DWORD dwSize = 0;
		if (GetTokenInformation(hToken, TokenElevationType, &elevationType, sizeof(elevationType), &dwSize))
		{
			if (elevationType == TokenElevationTypeLimited)
			{
				// The token is limited - get the linked elevated token
				TOKEN_LINKED_TOKEN linkedToken = { 0 };
				if (GetTokenInformation(hToken, TokenLinkedToken, &linkedToken, sizeof(linkedToken), &dwSize))
				{
					hModifiedToken.Set(linkedToken.LinkedToken);
				}
			}
			else if (elevationType == TokenElevationTypeFull)
			{
				// Already elevated - use as is
				hModifiedToken.Set(hToken.Detach());
			}
		}

		// If we couldn't get linked token or not a split token, continue with original token
		if (!hModifiedToken)
			hModifiedToken.Set(hToken.Detach());
	}
	else if (bDropAdmin && bCallerIsAdmin)
	{
		// Drop admin rights by using the linked token (UAC split token)
		TOKEN_ELEVATION_TYPE elevationType;
		DWORD dwSize = 0;
		if (GetTokenInformation(hToken, TokenElevationType, &elevationType, sizeof(elevationType), &dwSize))
		{
			if (elevationType == TokenElevationTypeFull)
			{
				// Get the linked token (limited token)
				TOKEN_LINKED_TOKEN linkedToken = { 0 };
				if (GetTokenInformation(hToken, TokenLinkedToken, &linkedToken, sizeof(linkedToken), &dwSize))
				{
					hModifiedToken.Set(linkedToken.LinkedToken);
				}
			}
		}

		// If we couldn't get linked token, continue with original token
		if (!hModifiedToken)
			hModifiedToken.Set(hToken.Detach());
	}
	else
	{
		// Use the original token
		hModifiedToken.Set(hToken.Detach());
	}

	// Create environment block
	CScopedHandle lpEnvironment((LPVOID)0, DestroyEnvironmentBlock);
	if (!CreateEnvironmentBlock(&lpEnvironment, hModifiedToken, FALSE))
		return ERR(STATUS_VARIABLE_NOT_FOUND);

	// Setup process creation
	STARTUPINFOW si = { 0 };
	si.cb = sizeof(si);
	si.dwFlags = STARTF_FORCEOFFFEEDBACK;

	if (Flags & eExec_Hidden)
	{
		si.dwFlags |= STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
	}
	else
	{
		si.wShowWindow = SW_SHOWNORMAL;
	}

	PROCESS_INFORMATION pi = { 0 };

	// Prepare command line (CreateProcess may modify it)
	std::wstring cmdLine = CommandLine;

	// Create the process
	// Note: In engine mode, we don't have SeAssignPrimaryTokenPrivilege, so use NULL token
	BOOL bSuccess = CreateProcessAsUserW(
		bIsServiceMode ? *&hModifiedToken : NULL,
		NULL,
		(wchar_t*)cmdLine.c_str(),
		NULL,
		NULL,
		FALSE,
		CREATE_UNICODE_ENVIRONMENT,
		lpEnvironment,
		WorkingDir.empty() ? NULL : WorkingDir.c_str(),
		&si,
		&pi);

	if (bSuccess)
	{
		// Return process info if requested
		if (pProcessHandle)
			*pProcessHandle = pi.hProcess;
		else
			CloseHandle(pi.hProcess);

		if (pProcessId)
			*pProcessId = pi.dwProcessId;

		CloseHandle(pi.hThread);
	}
	else
	{
		Status = ERR(GetLastWin32ErrorAsNtStatus());
	}

	return Status;
}

void CServiceCore::EmitEvent(ELogLevels Level, int Type, const StVariant& Data)
{
	if(!m_NextLogTime)
		m_NextLogTime = GetTickCount64() + 1000;
	m_pEventLog->AddEvent(Level, Type, Data);
}

////////////////////////////////////////////////////////////////////////////////
// Hooks
//

// Hook: LoadLibraryExW

typedef HMODULE (*P_LoadLibraryExW)(
	LPCWSTR lpLibFileName,
	HANDLE hFile,
	DWORD dwFlags);

P_LoadLibraryExW LoadLibraryExWTramp = NULL;

HMODULE NTAPI MyLoadLibraryExW(
	LPCWSTR lpLibFileName,
	HANDLE hFile,
	DWORD dwFlags)
{
	bool bNonExecutable = ((dwFlags & LOAD_LIBRARY_AS_IMAGE_RESOURCE) && (dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE)));

	if (bNonExecutable)
		theCore->Driver()->SetIgnorePendingImageLoad(true);
	
	HMODULE hModule = LoadLibraryExWTramp(lpLibFileName, hFile, dwFlags);

	if (bNonExecutable)
		theCore->Driver()->SetIgnorePendingImageLoad(false);

	return hModule;
}

// Init

STATUS CServiceCore::InitHooks()
{
	//
	// On windows 7 the notifier set by PsSetLoadImageNotifyRoutine is als called for non executable image load
	// we need those loads to read resoruces and icons, so we need to tell the driver upfront to skip the image verificatoin for the upcomming load
	//

	if (g_WindowsVersion < WINDOWS_10)
		HookFunction(LoadLibraryExW, MyLoadLibraryExW, (VOID**)&LoadLibraryExWTramp);

	return OK;
}