#include "pch.h"
#include "TokenUtil.h"

#include <phnt_windows.h>
#include <phnt.h>
#include <TlHelp32.h>
#include <taskschd.h>
#include <comdef.h>

#pragma comment(lib, "taskschd.lib")

STATUS QueryTokenVariable(HANDLE TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass, CBuffer& Buffer)
{
	NTSTATUS status;
	ULONG returnLength = 0;

	Buffer.AllocBuffer(0x80);
	status = NtQueryInformationToken(TokenHandle, TokenInformationClass, Buffer.GetBuffer(), (ULONG)Buffer.GetCapacity(), &returnLength);

	if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)
	{
		Buffer.AllocBuffer(returnLength);
		status = NtQueryInformationToken(TokenHandle, TokenInformationClass, Buffer.GetBuffer(), (ULONG)Buffer.GetCapacity(), &returnLength);
	}

	if (NT_SUCCESS(status))
		Buffer.SetSize(returnLength);

	return ERR(status);
}

#pragma warning(disable : 4996)

bool TokenIsAdmin(HANDLE hToken, bool OnlyFull)
{
	//
	// check if token is member of the Administrators group
	//

	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup;
	BOOL b = AllocateAndInitializeSid(
		&NtAuthority,
		2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&AdministratorsGroup);
	if (b) {
		if (! CheckTokenMembership(NULL, AdministratorsGroup, &b))
			b = FALSE;
		FreeSid(AdministratorsGroup);

		//
		// on Windows Vista, check for UAC split token
		//

		if (! b || OnlyFull) {
			OSVERSIONINFO osvi;
			osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			if (GetVersionEx(&osvi) && osvi.dwMajorVersion >= 6) {
				ULONG elevationType, len;
				b = GetTokenInformation(
					hToken, (TOKEN_INFORMATION_CLASS)TokenElevationType,
					&elevationType, sizeof(elevationType), &len);
				if (b && (elevationType != TokenElevationTypeFull &&
					(OnlyFull || elevationType != TokenElevationTypeLimited)))
					b = FALSE;
			}
		}
	}

	return b ? true : false;
}

bool IsRunningAsAdmin()
{
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
		return false;

	bool bIsAdmin = TokenIsAdmin(hToken, true);

	CloseHandle(hToken);
	return bIsAdmin;
}

bool IsRunningAsSystem()
{
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
		return false;

	DWORD dwSize = 0;
	GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		CloseHandle(hToken);
		return false;
	}

	BYTE* pTokenUser = new BYTE[dwSize];
	bool bIsSystem = false;

	if (GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize))
	{
		PTOKEN_USER pUser = (PTOKEN_USER)pTokenUser;
		SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
		PSID pSystemSid = NULL;

		if (AllocateAndInitializeSid(&ntAuthority, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &pSystemSid))
		{
			bIsSystem = EqualSid(pUser->User.Sid, pSystemSid) != FALSE;
			FreeSid(pSystemSid);
		}
	}

	delete[] pTokenUser;
	CloseHandle(hToken);
	return bIsSystem;
}

typedef enum _SECURITY_IMPERSONATION_LEVEL2 {
	SecurityAnonymous2,
	SecurityIdentification2,
	SecurityImpersonation2,
	SecurityDelegation2
} SECURITY_IMPERSONATION_LEVEL2;

typedef struct _SECURITY_QUALITY_OF_SERVICE2 {
	DWORD Length;
	SECURITY_IMPERSONATION_LEVEL2 ImpersonationLevel;
	BYTE ContextTrackingMode;
	BYTE EffectiveOnly;
} SECURITY_QUALITY_OF_SERVICE2, *PSECURITY_QUALITY_OF_SERVICE2;

typedef NTSTATUS (NTAPI* pfnNtImpersonateThread)(
	HANDLE ServerThreadHandle,
	HANDLE ClientThreadHandle,
	PSECURITY_QUALITY_OF_SERVICE2 SecurityQos
	);

static bool EnablePrivilege(const wchar_t* priv)
{
	HANDLE hToken = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return false;

	TOKEN_PRIVILEGES tp = {};
	tp.PrivilegeCount = 1;

	bool ok = false;

	if (LookupPrivilegeValueW(nullptr, priv, &tp.Privileges[0].Luid))
	{
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr))
			ok = (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
	}

	CloseHandle(hToken);
	return ok;
}

static DWORD FindProcessByName(const wchar_t* name)
{
	DWORD pid = 0;

	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE)
		return 0;

	PROCESSENTRY32W pe = {};
	pe.dwSize = sizeof(pe);

	if (Process32FirstW(snap, &pe))
	{
		do
		{
			if (_wcsicmp(pe.szExeFile, name) == 0)
			{
				pid = pe.th32ProcessID;
				break;
			}
		}
		while (Process32NextW(snap, &pe));
	}

	CloseHandle(snap);
	return pid;
}

//static DWORD GetFirstThreadIdOfProcess(DWORD pid)
//{
//	DWORD tid = 0;
//
//	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
//	if (snap == INVALID_HANDLE_VALUE)
//		return 0;
//
//	THREADENTRY32 te = {};
//	te.dwSize = sizeof(te);
//
//	if (Thread32First(snap, &te))
//	{
//		do
//		{
//			if (te.th32OwnerProcessID == pid)
//			{
//				tid = te.th32ThreadID;
//				break;
//			}
//		}
//		while (Thread32Next(snap, &te));
//	}
//
//	CloseHandle(snap);
//	return tid;
//}

//static bool ImpersonateSystemViaWinlogon()
//{
//	DWORD pid = FindProcessByName(L"winlogon.exe");
//	if (!pid)
//		return false;
//
//	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, FALSE, pid);
//	if (!hProcess)
//		return false;
//
//	HANDLE hToken = nullptr;
//	bool ok = false;
//
//	if (OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE, &hToken))
//	{
//		ok = !!ImpersonateLoggedOnUser(hToken);
//		CloseHandle(hToken);
//	}
//
//	CloseHandle(hProcess);
//	return ok;
//}

//static DWORD StartTrustedInstallerAndGetPid()
//{
//	DWORD pid = 0;
//
//	SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
//	if (!scm)
//		return 0;
//
//	SC_HANDLE svc = OpenServiceW(
//		scm,
//		L"TrustedInstaller",
//		SERVICE_START | SERVICE_QUERY_STATUS);
//
//	if (!svc)
//	{
//		CloseServiceHandle(scm);
//		return 0;
//	}
//
//	SERVICE_STATUS_PROCESS ssp = {};
//	DWORD needed = 0;
//
//	for (;;)
//	{
//		if (!QueryServiceStatusEx(
//			svc,
//			SC_STATUS_PROCESS_INFO,
//			reinterpret_cast<LPBYTE>(&ssp),
//			sizeof(ssp),
//			&needed))
//		{
//			break;
//		}
//
//		if (ssp.dwCurrentState == SERVICE_RUNNING)
//		{
//			pid = ssp.dwProcessId;
//			break;
//		}
//
//		if (ssp.dwCurrentState == SERVICE_STOPPED)
//		{
//			if (!StartServiceW(svc, 0, nullptr))
//			{
//				DWORD err = GetLastError();
//				if (err != ERROR_SERVICE_ALREADY_RUNNING)
//					break;
//			}
//		}
//
//		DWORD waitMs = ssp.dwWaitHint;
//		if (waitMs < 250)
//			waitMs = 250;
//		if (waitMs > 3000)
//			waitMs = 3000;
//
//		Sleep(waitMs);
//	}
//
//	CloseServiceHandle(svc);
//	CloseServiceHandle(scm);
//
//	return pid;
//}

//static HANDLE AcquireTrustedInstallerImpersonationToken()
//{
//	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
//	if (!ntdll)
//		return nullptr;
//
//	auto NtImpersonateThread =
//		reinterpret_cast<pfnNtImpersonateThread>(
//			GetProcAddress(ntdll, "NtImpersonateThread"));
//
//	if (!NtImpersonateThread)
//		return nullptr;
//
//	DWORD tiPid = StartTrustedInstallerAndGetPid();
//	if (!tiPid)
//		return nullptr;
//
//	DWORD tiTid = GetFirstThreadIdOfProcess(tiPid);
//	if (!tiTid)
//		return nullptr;
//
//	HANDLE hThread = OpenThread(THREAD_DIRECT_IMPERSONATION, FALSE, tiTid);
//	if (!hThread)
//		return nullptr;
//
//	SECURITY_QUALITY_OF_SERVICE2 sqos = {};
//	sqos.Length = sizeof(sqos);
//	sqos.ImpersonationLevel = SecurityImpersonation2;
//	sqos.ContextTrackingMode = SECURITY_STATIC_TRACKING;
//	sqos.EffectiveOnly = FALSE;
//
//	HANDLE hCurrentThread = GetCurrentThread();
//
//	NTSTATUS st = NtImpersonateThread(hCurrentThread, hThread, &sqos);
//	CloseHandle(hThread);
//
//	if (!NT_SUCCESS(st))
//		return nullptr;
//
//	HANDLE hTiToken = nullptr;
//
//	if (!OpenThreadToken(
//		hCurrentThread,
//		TOKEN_DUPLICATE |
//		TOKEN_ASSIGN_PRIMARY |
//		TOKEN_QUERY |
//		TOKEN_IMPERSONATE |
//		TOKEN_ADJUST_DEFAULT,
//		FALSE,
//		&hTiToken))
//	{
//		return nullptr;
//	}
//
//	return hTiToken;
//}

static bool EnableThreadPrivilege(const wchar_t* priv)
{
	HANDLE hToken = nullptr;
	if (!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken))
		return false;

	TOKEN_PRIVILEGES tp = {};
	tp.PrivilegeCount = 1;

	bool ok = false;

	if (LookupPrivilegeValueW(nullptr, priv, &tp.Privileges[0].Luid))
	{
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr))
			ok = (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
	}

	CloseHandle(hToken);
	return ok;
}

bool RunAsSystem(const std::wstring& CommandLine, HANDLE* phProcess)
{
	if (phProcess)
		*phProcess = nullptr;

	EnablePrivilege(SE_DEBUG_NAME);
	EnablePrivilege(SE_IMPERSONATE_NAME);

	DWORD pid = FindProcessByName(L"winlogon.exe");
	if (!pid)
		return false;

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, FALSE, pid);
	if (!hProcess)
		return false;

	HANDLE hToken = nullptr;

	if (OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE, &hToken))
	{
		if (!ImpersonateLoggedOnUser(hToken)) {
			CloseHandle(hToken);
			hToken = nullptr;
		}
	}

	CloseHandle(hProcess);

	if (!hToken)
		return false;

//	if (!ImpersonateSystemViaWinlogon())
//		return false;

	// While impersonating SYSTEM, enable privileges needed for CreateProcessAsUserW
	EnableThreadPrivilege(SE_ASSIGNPRIMARYTOKEN_NAME);
	EnableThreadPrivilege(SE_INCREASE_QUOTA_NAME);

//	HANDLE hToken = AcquireTrustedInstallerImpersonationToken();
//	if (!hToken)
//	{
//		RevertToSelf();
//		return false;
//	}

	HANDLE hLaunchToken = nullptr;

	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(sa);

	BOOL dupOk = DuplicateTokenEx(
		hToken,
		TOKEN_ALL_ACCESS,
		&sa,
		SecurityImpersonation,
		TokenPrimary,
		&hLaunchToken);

	CloseHandle(hToken);

	if (!dupOk)
	{
		RevertToSelf();
		return false;
	}

	// Set session ID to current session so the process can interact with the desktop
	DWORD dwSessionId = 0;
	ProcessIdToSessionId(GetCurrentProcessId(), &dwSessionId);
	SetTokenInformation(hLaunchToken, TokenSessionId, &dwSessionId, sizeof(DWORD));

//	// Revert to SYSTEM impersonation (not TrustedInstaller) for CreateProcessAsUserW
//	RevertToSelf();
//	if (!ImpersonateSystemViaWinlogon())
//	{
//		CloseHandle(hLaunchToken);
//		return false;
//	}
//	EnableThreadPrivilege(SE_ASSIGNPRIMARYTOKEN_NAME);
//	EnableThreadPrivilege(SE_INCREASE_QUOTA_NAME);

	std::wstring cmd = CommandLine;

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	si.lpDesktop = const_cast<LPWSTR>(L"WinSta0\\Default");

	PROCESS_INFORMATION pi = {};

	// Use CreateProcessAsUserW - doesn't require seclogon service
	BOOL ok = CreateProcessAsUserW(
		hLaunchToken,
		nullptr,
		cmd.data(),
		nullptr,
		nullptr,
		FALSE,
		CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE,
		nullptr,
		nullptr,
		&si,
		&pi);

	DWORD err = ok ? ERROR_SUCCESS : GetLastError();

	CloseHandle(hLaunchToken);
	RevertToSelf();

	if (!ok)
	{
		SetLastError(err);
		return false;
	}

	if (phProcess)
		*phProcess = pi.hProcess;
	else
		CloseHandle(pi.hProcess);

	CloseHandle(pi.hThread);
	return true;
}

bool RunAsSystemTask(const std::wstring& CommandLine, HANDLE* phProcess)
{
	//
	// Use Task Scheduler to run a process as SYSTEM.
	// This is the standard technique used by PsExec and similar tools.
	// Requires administrator privileges.
	//

	bool bSuccess = false;
	HRESULT hr = S_OK;
	std::wstring exePath;

	// Parse executable path from command line
	{
		const wchar_t* cmd = CommandLine.c_str();
		while (*cmd && iswspace(*cmd)) cmd++;

		if (*cmd == L'"') {
			cmd++;
			const wchar_t* end = wcschr(cmd, L'"');
			if (end)
				exePath.assign(cmd, end - cmd);
		} else {
			const wchar_t* end = cmd;
			while (*end && !iswspace(*end)) end++;
			exePath.assign(cmd, end - cmd);
		}
	}

	// Get existing PIDs for this executable before starting
	std::vector<DWORD> existingPids;
	{
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot != INVALID_HANDLE_VALUE)
		{
			PROCESSENTRY32W pe32;
			pe32.dwSize = sizeof(PROCESSENTRY32W);
			if (Process32FirstW(hSnapshot, &pe32))
			{
				do
				{
					// Get full path and compare
					HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
					if (hProc)
					{
						wchar_t szPath[MAX_PATH];
						DWORD dwSize = MAX_PATH;
						if (QueryFullProcessImageNameW(hProc, 0, szPath, &dwSize))
						{
							if (_wcsicmp(szPath, exePath.c_str()) == 0)
								existingPids.push_back(pe32.th32ProcessID);
						}
						CloseHandle(hProc);
					}
				} while (Process32NextW(hSnapshot, &pe32));
			}
			CloseHandle(hSnapshot);
		}
	}

	// Generate a unique task name
	GUID guid;
	CoCreateGuid(&guid);
	wchar_t szTaskName[64];
	swprintf_s(szTaskName, L"MajorPrivacy_RunAsSystem_%08X%04X%04X",
		guid.Data1, guid.Data2, guid.Data3);

	ITaskService* pService = NULL;
	ITaskFolder* pRootFolder = NULL;
	ITaskDefinition* pTask = NULL;
	IRegistrationInfo* pRegInfo = NULL;
	IPrincipal* pPrincipal = NULL;
	ITaskSettings* pSettings = NULL;
	IActionCollection* pActionCollection = NULL;
	IAction* pAction = NULL;
	IExecAction* pExecAction = NULL;
	IRegisteredTask* pRegisteredTask = NULL;
	IRunningTask* pRunningTask = NULL;

	// Initialize COM (should already be initialized, but just in case)
	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	bool bComInitialized = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
	if (hr == RPC_E_CHANGED_MODE)
		hr = S_OK;

	// Create Task Service
	hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
	if (FAILED(hr)) goto cleanup;

	// Connect to local Task Scheduler
	hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
	if (FAILED(hr)) goto cleanup;

	// Get root folder
	hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
	if (FAILED(hr)) goto cleanup;

	// Create task definition
	hr = pService->NewTask(0, &pTask);
	if (FAILED(hr)) goto cleanup;

	// Set registration info
	hr = pTask->get_RegistrationInfo(&pRegInfo);
	if (SUCCEEDED(hr)) {
		pRegInfo->put_Author(_bstr_t(L"MajorPrivacy"));
		pRegInfo->Release();
	}

	// Set principal to run as SYSTEM with highest privileges
	hr = pTask->get_Principal(&pPrincipal);
	if (FAILED(hr)) goto cleanup;

	hr = pPrincipal->put_UserId(_bstr_t(L"S-1-5-18")); // SYSTEM SID
	if (FAILED(hr)) goto cleanup;

	hr = pPrincipal->put_LogonType(TASK_LOGON_SERVICE_ACCOUNT);
	if (FAILED(hr)) goto cleanup;

	hr = pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
	if (FAILED(hr)) goto cleanup;

	// Set settings
	hr = pTask->get_Settings(&pSettings);
	if (SUCCEEDED(hr)) {
		pSettings->put_StartWhenAvailable(VARIANT_TRUE);
		pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
		pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
		pSettings->put_AllowHardTerminate(VARIANT_TRUE);
		pSettings->put_ExecutionTimeLimit(_bstr_t(L"PT0S")); // No time limit
		pSettings->Release();
	}

	// Create action
	hr = pTask->get_Actions(&pActionCollection);
	if (FAILED(hr)) goto cleanup;

	hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
	if (FAILED(hr)) goto cleanup;

	hr = pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction);
	if (FAILED(hr)) goto cleanup;

	// Set executable and arguments
	{
		std::wstring args;
		const wchar_t* cmd = CommandLine.c_str();

		// Skip executable part
		while (*cmd && iswspace(*cmd)) cmd++;
		if (*cmd == L'"') {
			cmd++;
			const wchar_t* end = wcschr(cmd, L'"');
			if (end) cmd = end + 1;
		} else {
			while (*cmd && !iswspace(*cmd)) cmd++;
		}
		while (*cmd && iswspace(*cmd)) cmd++;
		args = cmd;

		hr = pExecAction->put_Path(_bstr_t(exePath.c_str()));
		if (FAILED(hr)) goto cleanup;

		if (!args.empty()) {
			hr = pExecAction->put_Arguments(_bstr_t(args.c_str()));
			if (FAILED(hr)) goto cleanup;
		}
	}

	// Register the task
	hr = pRootFolder->RegisterTaskDefinition(
		_bstr_t(szTaskName),
		pTask,
		TASK_CREATE_OR_UPDATE,
		_variant_t(L"S-1-5-18"),
		_variant_t(),
		TASK_LOGON_SERVICE_ACCOUNT,
		_variant_t(L""),
		&pRegisteredTask);
	if (FAILED(hr)) goto cleanup;

	// Run the task immediately
	hr = pRegisteredTask->Run(_variant_t(), &pRunningTask);
	if (FAILED(hr)) goto cleanup;

	if (phProcess)
	{
		HANDLE hProcess = NULL;

		// Wait briefly for task to start, then find the new process
		for (int retry = 0; retry < 50 && !hProcess; retry++) // 5 seconds max
		{
			Sleep(100);

			HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (hSnapshot == INVALID_HANDLE_VALUE)
				continue;

			PROCESSENTRY32W pe32;
			pe32.dwSize = sizeof(PROCESSENTRY32W);
			if (Process32FirstW(hSnapshot, &pe32))
			{
				do
				{
					// Check if this PID existed before
					bool bExisted = false;
					for (DWORD pid : existingPids)
					{
						if (pid == pe32.th32ProcessID)
						{
							bExisted = true;
							break;
						}
					}
					if (bExisted)
						continue;

					// Check if this is our executable
					HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pe32.th32ProcessID);
					if (hProc)
					{
						wchar_t szPath[MAX_PATH];
						DWORD dwSize = MAX_PATH;
						if (QueryFullProcessImageNameW(hProc, 0, szPath, &dwSize))
						{
							if (_wcsicmp(szPath, exePath.c_str()) == 0)
							{
								hProcess = hProc;
								hProc = NULL;
							}
						}
						if (hProc)
							CloseHandle(hProc);
					}
				} while (Process32NextW(hSnapshot, &pe32) && !hProcess);
			}
			CloseHandle(hSnapshot);
		}

		*phProcess = hProcess;

		bSuccess = (hProcess != NULL);
	}
	else
		bSuccess = true;

cleanup:
	// Delete the task (doesn't kill the running process)
	if (pRootFolder)
		pRootFolder->DeleteTask(_bstr_t(szTaskName), 0);

	// Release COM objects
	if (pRunningTask) pRunningTask->Release();
	if (pRegisteredTask) pRegisteredTask->Release();
	if (pExecAction) pExecAction->Release();
	if (pAction) pAction->Release();
	if (pActionCollection) pActionCollection->Release();
	if (pPrincipal) pPrincipal->Release();
	if (pTask) pTask->Release();
	if (pRootFolder) pRootFolder->Release();
	if (pService) pService->Release();

	if (bComInitialized && hr != RPC_E_CHANGED_MODE)
		CoUninitialize();

	return bSuccess;
}