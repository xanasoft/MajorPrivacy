#include "pch.h"

#include <objbase.h>

#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/AppUtil.h"
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

CServiceCore* theCore = NULL;

FW::DefaultMemPool g_DefaultMemPool;

extern BOOLEAN				 g_UnloadProtection;
extern SERVICE_STATUS        g_ServiceStatus;
extern SERVICE_STATUS_HANDLE g_ServiceStatusHandle;

CServiceCore::CServiceCore()
	: m_Pool(4)
{
	m_pSysLog = new CEventLogger(API_SERVICE_NAME);
	m_pEventLog = new CEventLog();

	m_pUserPipe = new CPipeServer();
	m_pUserPort = new CAlpcPortServer();

	m_pEnclaveManager = new CEnclaveManager();

	m_pProgramManager = new CProgramManager();

	m_pProcessList = new CProcessList();

	m_pAccessManager = new CAccessManager();

	m_pNetworkManager = new CNetworkManager();

	m_pVolumeManager = new CVolumeManager();

	m_pTweakManager = new CTweakManager();

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

	delete m_pTweakManager;

	delete m_pVolumeManager;

	delete m_pNetworkManager;

	delete m_pAccessManager;

	delete m_pProcessList;

	delete m_pProgramManager;

	delete m_pEnclaveManager;

	delete m_pUserPipe;
	delete m_pUserPort;

	delete m_pSysLog;
	delete m_pEventLog;

	delete m_pConfig; // could be NULL


	CNtPathMgr::Instance()->UnRegisterDeviceChangeCallback(DeviceChangedCallback, this);
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
	if (Status.IsError())
		Shutdown();

	ULONGLONG End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Startup took %llu ms", End - Start);

	return Status;
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
		InstallDriver();
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
			g_ServiceStatus.dwControlsAccepted = 0;
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
			g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
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

STATUS CServiceCore::InstallDriver()
{
	STATUS Status = CDriverAPI::InstallDrv();
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

STATUS CServiceCore::RemoveDriver()
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
	SetThreadDescription(GetCurrentThread(), L"CServiceCore__ThreadProc");
#endif

	CServiceCore* This = (CServiceCore*)lpThreadParameter;

	NTSTATUS status;
	uint32 uDrvABI;
	ULONGLONG End, Start;

	//
	// Setup required privileges
	//
	
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
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to set privileges, error: 0x%08X", status);

	//
	// init COM for this thread
	//

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	//
	// LoadEventLog
	// 

	This->m_pEventLog->Load();

	//
	// Initialize the driver
	//

	Start = GetTickCount64();

	This->m_InitStatus = This->InitDriver();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to connect to driver, error: 0x%08X", This->m_InitStatus.GetStatus());
		goto cleanup;
	}

	uDrvABI = This->m_pDriver->GetABIVersion();
	if(uDrvABI != MY_ABI_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Driver ABI version mismatch, expected: %06X, got %06X", MY_ABI_VERSION, uDrvABI);
		goto cleanup;
	}

	End = GetTickCount64();
	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"PrivacyAgent Driver init took %llu ms", End - Start);

	//
	// Initialize the rest of the components
	//

	Start = End;

	This->m_InitStatus = This->m_pEnclaveManager->Init();
	if (This->m_InitStatus.IsError()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to init Enclave Manager, error: 0x%08X", This->m_InitStatus.GetStatus());
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
	This->Driver()->CommitConfigChanges();

	This->m_pTweakManager->Store();

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

	if (This->m_Shutdown == 2) {
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

		auto Ret = JSEngine.CallFunc("func", 1, Var);
		auto sRet = Ret.GetValue().AsStr();
		sRet.clear();
	}*/

	m_AppDir = GetApplicationDirectory();

	// Initialize this variable here befor any threads are started and later access it without synchronisation for reading only
	m_DataFolder = CConfigIni::GetAppDataFolder() + L"\\" + GROUP_NAME + L"\\" + APP_NAME;
	if (!std::filesystem::exists(m_DataFolder)) 
	{
		if (std::filesystem::create_directories(m_DataFolder)) 
		{
			SetAdminFullControl(m_DataFolder);

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

void CServiceCore::Shutdown(bool bWait)
{
	if (!theCore || !theCore->m_hThread)
		return;
	HANDLE hThread = theCore->m_hThread;

	theCore->m_Shutdown = bWait ? 1 : 2;
	if (!bWait) 
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
	if(Key == "LogRegistry")
		m_pProcessList->Reconfigure();

	//m_pProgramManager->Reconfigure();

	//m_pAccessManager->Reconfigure();

	if(Key.substr(0, 3) == "Dns")
		m_pNetworkManager->Reconfigure(Key == "DnsResolvers", Key == "DnsBlockLists");

	//m_pEtwEventMonitor->Reconfigure();
}

STATUS CServiceCore::CommitConfig()
{
	m_pProgramManager->Store();
	m_pNetworkManager->DnsFilter()->Store();
	m_pNetworkManager->Firewall()->Store();
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
		ObjectTrackerBase::PrintCounts();

		//size_t memoryUsed = getHeapUsage();
		//DbgPrint(L"USED MEMORY: %llu bytes\n", memoryUsed);
	}
#endif
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

void CServiceCore::EmitEvent(ELogLevels Level, int Type, const StVariant& Data)
{
	if(!m_NextLogTime)
		m_NextLogTime = GetTickCount64() + 1000;
	m_pEventLog->AddEvent(Level, Type, Data);
}
