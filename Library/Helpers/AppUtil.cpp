#include "pch.h"
#include "AppUtil.h"
#include "NtUtil.h"

#include <ph.h>
#include <appresolver.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>


std::wstring GetApplicationDirectory()
{
	wchar_t szPath[MAX_PATH];
	GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
	*wcsrchr(szPath, L'\\') = L'\0';
	return szPath;
}

std::wstring GetProgramFilesDirectory()
{
    std::wstring programFilesPath;

    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, NULL, &path))) {
        programFilesPath = path;
        CoTaskMemFree(path);
    }

    return programFilesPath;
}

std::wstring ExpandEnvStrings(const std::wstring &command)
{
    wchar_t commandValue[2 * MAX_PATH] = {0};
    DWORD returnValue = ExpandEnvironmentStringsW(command.c_str(), commandValue, 2 * MAX_PATH - 1);
    if (returnValue)
        return std::wstring(commandValue);
    return command;
}

std::wstring GetFileNameFromPath(const std::wstring &FilePath)
{
    size_t pos = FilePath.find_last_of(L"\\");
    if (pos != std::wstring::npos)
        return FilePath.substr(pos + 1);
    return FilePath;
}

std::wstring GetFileFromCommand(const std::wstring& Command, std::wstring* Arguments)
{
    std::wstring Path = Command;
    size_t End;

    if (Path[0] == '\"') {
        End = Path.find('\"', 1);
        if (End != std::wstring::npos)
            Path = Path.substr(1, End - 1);
    }
    else {
        End = Path.find(' ');
        if (End != std::wstring::npos)
            Path = Path.substr(0, End);
    }

    if (Arguments && End != std::wstring::npos)
        *Arguments = Command.substr(End + 1);

    return Path;
}


std::wstring PHStr2WStr(PPH_STRING phStr, bool bFree = false)
{
    std::wstring wStr;
    if (!PhIsNullOrEmptyString(phStr))
        wStr = std::wstring(phStr->Buffer, phStr->Length / sizeof(WCHAR));
    if(phStr && bFree)
        PhDereferenceObject(phStr);
    return wStr;
}

std::wstring GetAppContainerNameBySid(const SSid& Sid)
{
    PPH_STRING phName = PhGetAppContainerName((PSID)Sid.Get());
	return PHStr2WStr(phName, true);
}

std::wstring GetAppContainerSidFromName(const std::wstring& PackageFamilyName)
{
    PPH_STRING packageSidString = PhGetAppContainerSidFromName((WCHAR*)PackageFamilyName.c_str());
    return PHStr2WStr(packageSidString, true);
}

std::wstring GetAppContainerRootPath(const std::wstring& FullPackageName)
{
    PPH_STRING packageFullName = PhCreateStringEx((WCHAR*)FullPackageName.c_str(), FullPackageName.length() * sizeof(WCHAR));
    PPH_STRING phPath = PhGetPackagePath(packageFullName);
    PhDereferenceObject(packageFullName);
	return PHStr2WStr(phPath, true);
}

//RESULT(std::shared_ptr<std::list<SAppPackagePtr>>) EnumAllAppPackages()
//{
//    std::shared_ptr<std::list<SAppPackagePtr>> List = std::make_shared<std::list<SAppPackagePtr>>();
//
//    PPH_LIST apps = PhEnumPackageApplicationUserModelIds();
//    if (!apps)
//        return ERR(STATUS_UNSUCCESSFUL);
//
//    for (ULONG i = 0; i < apps->Count; i++)
//    {
//        PPH_APPUSERMODELID_ENUM_ENTRY pEntry = (PPH_APPUSERMODELID_ENUM_ENTRY)apps->Items[i];
//        
//        SAppPackagePtr pPackage = std::make_shared<SAppPackage>();
//
//        pPackage->AppUserModelId = PHStr2WStr(pEntry->AppUserModelId, false);
//
//        pPackage->PackageName = PHStr2WStr(pEntry->PackageName, false);
//        pPackage->PackageFullName = PHStr2WStr(pEntry->PackageFullName, false);
//        pPackage->PackageFamilyName = PHStr2WStr(pEntry->PackageFamilyName, false);
//        pPackage->PackageVersion = PHStr2WStr(pEntry->PackageVersion, false);
//
//        pPackage->PackageDisplayName = PHStr2WStr(pEntry->PackageDisplayName, false);
//        pPackage->SmallLogoPath = PHStr2WStr(pEntry->SmallLogoPath, false);
//        pPackage->PackageInstallPath = PHStr2WStr(pEntry->PackageInstallPath, false);
//
//        PPH_STRING packageSidString = PhGetAppContainerSidFromName((WCHAR*)pPackage->PackageFamilyName.c_str());
//        pPackage->PackageSID = PHStr2WStr(packageSidString, true);
//
//        List->push_back(pPackage);
//    }
//
//    PhDestroyEnumPackageApplicationUserModelIds(apps);
//
//    RETURN(List);
//}

VOID EnumAllAppPackages(BOOLEAN(*callBack)(PVOID param, void* AppPackage, void* AppPackage2), PVOID param)
{
    PhEnumPackageApplicationUserModelIdsCB(NULL, callBack, param);
}

VOID GetAppPackageInfos(void* AppPackage, void* AppPackage2,
    std::wstring *PackageName,
    std::wstring *PackageFullName,
    std::wstring *PackageFamilyName,
    std::wstring *PackageVersion,
    // below are the slow once
    std::wstring *PackageDisplayName,
    std::wstring *PackageLogo,
    std::wstring *PackageInstallLocation)
{
    PPH_STRING packageInstallLocationString = NULL;
    PPH_STRING packageNameString = NULL;
    PPH_STRING packageFullNameString = NULL;
    PPH_STRING packageFamilyNameString = NULL;
    PPH_STRING packageDisplayNameString = NULL;
    PPH_STRING packageLogoString = NULL;
    PPH_STRING packageVersionString = NULL;

    PhQueryApplicationModelPackageInformation(AppPackage, AppPackage2, 
        PackageInstallLocation ? &packageInstallLocationString : NULL,
        PackageName ? &packageNameString : NULL,
        PackageFullName ? &packageFullNameString : NULL,
        PackageFamilyName ? &packageFamilyNameString : NULL,
        PackageDisplayName ? &packageDisplayNameString : NULL,
        PackageLogo ? &packageLogoString : NULL,
        PackageVersion ? &packageVersionString : NULL);
    
    if (PackageName)             *PackageName = PHStr2WStr(packageNameString, false);
    if (PackageFullName)         *PackageFullName = PHStr2WStr(packageFullNameString, false);
    if (PackageFamilyName)       *PackageFamilyName = PHStr2WStr(packageFamilyNameString, false);
    if (PackageVersion)          *PackageVersion = PHStr2WStr(packageVersionString, false);
    if (PackageDisplayName)      *PackageDisplayName = PHStr2WStr(packageDisplayNameString, false);
    if (PackageLogo)             *PackageLogo = PHStr2WStr(packageLogoString, false);
    if (PackageInstallLocation)  *PackageInstallLocation = PHStr2WStr(packageInstallLocationString, false);
        
    PhClearReference((PVOID*)&packageLogoString);
    PhClearReference((PVOID*)&packageVersionString);
    PhClearReference((PVOID*)&packageFullNameString);
    PhClearReference((PVOID*)&packageInstallLocationString);
    PhClearReference((PVOID*)&packageFamilyNameString);
    PhClearReference((PVOID*)&packageDisplayNameString);
    PhClearReference((PVOID*)&packageNameString);
}

bool IsRunningElevated()
{
    return !!PhGetOwnTokenAttributes().Elevated;
}

#if 1

static LPWSTR GetArgumentW(LPWSTR* pCmdLine)
{
    LPWSTR cmdLine = *pCmdLine;
    while (*cmdLine == L' ') cmdLine++;
    LPWSTR arg = NULL;
    if (*cmdLine == L'\"') {
        cmdLine++;
        arg = cmdLine;
        cmdLine = wcschr(cmdLine, L'\"');
    } else {
        arg = cmdLine;
        cmdLine = wcschr(cmdLine, L' ');
    }
    if (cmdLine) {
        *cmdLine++ = L'\0';
        while (*cmdLine == L' ') cmdLine++;
    }
    *pCmdLine = cmdLine;
    return arg;
}

extern "C" LIBRARY_EXPORT VOID RunElevatedW(HWND hwnd, HINSTANCE hinst, LPWSTR pszCmdLine, int nCmdShow)
{
	LPWSTR path = GetArgumentW(&pszCmdLine);

	DWORD dwTimeOut = 0;

    while (path && (path[0] == L'-' || path[0] == L'/'))
    {
		LPWSTR arg = path;
        path = GetArgumentW(&pszCmdLine);

        if(wcsnicmp(arg+1, L"timeout", 7) == 0 && (arg[8] == L':' || arg[8] == L'='))
			dwTimeOut = _wtoi(arg + 9);
    }

	LPWSTR params = pszCmdLine;

    SHELLEXECUTEINFO shex;
    memset(&shex, 0, sizeof(SHELLEXECUTEINFOW));
    shex.cbSize = sizeof(SHELLEXECUTEINFO);
    shex.fMask = SEE_MASK_NOCLOSEPROCESS;
    shex.hwnd = NULL;
    shex.lpFile = path;
    shex.lpParameters = params;
    shex.nShow = SW_SHOWNORMAL;
    shex.lpVerb = L"runas";

	DWORD dwExitCode = -1;

    if (ShellExecuteEx(&shex)) {
        if (dwTimeOut) {
            if (WaitForSingleObject(shex.hProcess, dwTimeOut) == WAIT_OBJECT_0)
				GetExitCodeProcess(shex.hProcess, &dwExitCode);
            else
                dwExitCode = STILL_ACTIVE;
        }
        CloseHandle(shex.hProcess);
    }
    else
		dwExitCode = GetLastError();

    ExitProcess(dwExitCode);
}

//
// When ShellExecuteEx with the "runas" verb the appinfo service is used to create the process, and give us back a handle to it.
// KernelIsolator driver blocks this at two levels:
// 1. appinfo won't be able to duplicate a driver into the calling process
// 2. appinfo fails to create a process which is created with protection
// 
// As a workaround we use rundll32.exe to call back into our own DLL which will create the elevated process for us.
// And the driver when it sees that a dev signed process is not being created by its parent (creatorPID != parentPID) will not enable any protection
// The driver will those also not trust the process, hence after elevation the process will need to restart irself to get protection and trust.
//

HANDLE RunElevatedEx(const std::wstring& path, const std::wstring& params, DWORD dwTimeOut, int nCmdShow)
{
    // rundll32.exe GeneralLibrary.dll,RunElevated "C:\WIndows\system32\cmd.exe" /c ping.exe
	std::wstring command = L"\"" + GetApplicationDirectory() + L"\\GeneralLibrary.dll\",RunElevated ";
    
    if (dwTimeOut) {
        command += L"-timeout:";
        command += std::to_wstring(dwTimeOut);
		command += L" ";
    }

    command += L"\"" + path + L"\" " + params;

    SHELLEXECUTEINFO shex;
    memset(&shex, 0, sizeof(SHELLEXECUTEINFOW));
    shex.cbSize = sizeof(SHELLEXECUTEINFO);
    shex.fMask = SEE_MASK_NOCLOSEPROCESS;
    shex.hwnd = NULL;
    shex.lpFile = L"rundll32.exe";
    shex.lpParameters = command.c_str();
    shex.nShow = nCmdShow;

    if(ShellExecuteEx(&shex))
        return shex.hProcess;
    return NULL;
}

DWORD RunElevated(const std::wstring& path, const std::wstring& params, DWORD dwTimeOut, int nCmdShow)
{
    DWORD dwExitCode = -1;

	HANDLE hProcess = RunElevatedEx(path, params, dwTimeOut, nCmdShow);

    if (hProcess) {
        if (WaitForSingleObject(hProcess, dwTimeOut + 30 * 1000) == WAIT_OBJECT_0) //  alwais wait extra 30 seconds for the user to approve the UAC prompt
            GetExitCodeProcess(hProcess, &dwExitCode);
        CloseHandle(hProcess);
    }

    return dwExitCode;
}

#else

HANDLE RunElevated(const std::wstring& path, const std::wstring& params, int nCmdShow)
{
    SHELLEXECUTEINFO shex;
    memset(&shex, 0, sizeof(SHELLEXECUTEINFOW));
    shex.cbSize = sizeof(SHELLEXECUTEINFO);
    shex.fMask = SEE_MASK_NOCLOSEPROCESS;
    shex.hwnd = NULL;
    shex.lpFile = path.c_str();
    shex.lpParameters = params.c_str();
    shex.nShow = nCmdShow;
    shex.lpVerb = L"runas";

    if(ShellExecuteEx(&shex))
        return shex.hProcess;
    return NULL;
}

#endif

std::wstring ExpandEnvironmentVariablesInPath(const std::wstring& path) 
{
    DWORD requiredSize = ExpandEnvironmentStrings(path.c_str(), nullptr, 0);
    if (requiredSize == 0)
        return path;

    std::vector<wchar_t> buffer(requiredSize);
    DWORD actualSize = ExpandEnvironmentStrings(path.c_str(), &buffer[0], (DWORD)buffer.size());
    if (actualSize == 0 || actualSize > requiredSize)
        return path; // Expansion failed or buffer was somehow still too small

    return std::wstring(&buffer[0]);
}

std::wstring GetResourceStr(const std::wstring& str)
{
    if (str.empty() || str[0] != L'@')
        return str;

    static std::mutex Mutex;
    static std::map<std::wstring, std::wstring> Map;

    std::unique_lock Lock(Mutex);

    auto F = Map.find(str);
    if (F != Map.end())
        return F->second;

    DWORD requiredSize = 0x1000; // 4 kb must be anough
    std::vector<wchar_t> buffer(requiredSize);
    int result = SHLoadIndirectString(str.c_str(),  &buffer[0], (UINT)buffer.size(), NULL);
    if (result == S_OK)
        return Map.insert(std::make_pair(str, &buffer[0])).first->second;

    return str;
}

SImageVersionInfoPtr GetImageVersionInfo(const std::wstring& FileName)
{
    SImageVersionInfoPtr pVersionInfo;
    PH_IMAGE_VERSION_INFO PhVersionInfo = { NULL, NULL, NULL, NULL };
    PPH_STRING FileNameWin32 = PhCreateStringEx((WCHAR*)FileName.c_str(), FileName.length() * sizeof(WCHAR));
    if (PhInitializeImageVersionInfoCached(&PhVersionInfo, FileNameWin32, FALSE, FALSE)) 
    {
        // todo: fix me PhInitializeImageVersionInfoCached returns to long strings
        pVersionInfo = std::make_shared<SImageVersionInfo>();
        pVersionInfo->CompanyName = PHStr2WStr(PhVersionInfo.CompanyName, true).c_str();
        pVersionInfo->FileDescription = PHStr2WStr(PhVersionInfo.FileDescription, true).c_str();
        pVersionInfo->FileVersion = PHStr2WStr(PhVersionInfo.FileVersion, true).c_str();
        pVersionInfo->ProductName = PHStr2WStr(PhVersionInfo.ProductName, true).c_str();
    }
    PhDereferenceObject(FileNameWin32);
    return pVersionInfo;
}

/*SImageVersionInfoPtr GetImageVersionInfoNt(const std::wstring& FileName)
{
    return GetImageVersionInfo(NtPathToDosPath(FileName));
}*/


//typedef struct _STRING32
//{
//	USHORT Length;
//	USHORT MaximumLength;
//	ULONG Buffer;
//} UNICODE_STRING32, * PUNICODE_STRING32;

//typedef struct _STRING64 {
//  USHORT Length;
//  USHORT MaximumLength;
//  PVOID64 Buffer;
//} UNICODE_STRING64, * PUNICODE_STRING64;


//// PROCESS_BASIC_INFORMATION for pure 32 and 64-bit processes
//typedef struct _PROCESS_BASIC_INFORMATION {
//    PVOID Reserved1;
//    PVOID PebBaseAddress;
//    PVOID Reserved2[2];
//    ULONG_PTR UniqueProcessId;
//    PVOID Reserved3;
//} PROCESS_BASIC_INFORMATION;

// PROCESS_BASIC_INFORMATION for 32-bit process on WOW64
typedef struct _PROCESS_BASIC_INFORMATION_WOW64 {
    PVOID Reserved1[2];
    PVOID64 PebBaseAddress;
    PVOID Reserved2[4];
    ULONG_PTR UniqueProcessId[2];
    PVOID Reserved3[2];
} PROCESS_BASIC_INFORMATION_WOW64;


typedef NTSTATUS (NTAPI *_NtQueryInformationProcess)(IN HANDLE ProcessHandle, ULONG ProcessInformationClass,
    OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength, OUT PULONG ReturnLength OPTIONAL );

//typedef NTSTATUS (NTAPI *_NtReadVirtualMemory)(IN HANDLE ProcessHandle, IN PVOID BaseAddress,
//    OUT PVOID Buffer, IN SIZE_T Size, OUT PSIZE_T NumberOfBytesRead);

typedef NTSTATUS (NTAPI *_NtWow64ReadVirtualMemory64)(IN HANDLE ProcessHandle,IN PVOID64 BaseAddress,
    OUT PVOID Buffer, IN ULONG64 Size, OUT PULONG64 NumberOfBytesRead);


std::wstring GetPebString(HANDLE ProcessHandle, PEB_OFFSET Offset)
{
	BOOL is64BitOperatingSystem;
	BOOL isWow64Process = FALSE;
#ifdef _WIN64
	is64BitOperatingSystem = TRUE;
#else // ! _WIN64
	isWow64Process = IsWow64();
	is64BitOperatingSystem = isWow64Process;
#endif _WIN64

	BOOL isTargetWow64Process = FALSE;
	IsWow64Process(ProcessHandle, &isTargetWow64Process);
	BOOL isTarget64BitProcess = is64BitOperatingSystem && !isTargetWow64Process;

	ULONG processParametersOffset = isTarget64BitProcess ? 0x20 : 0x10;

	ULONG offset = 0;
	switch (Offset)
	{
	case AppCurrentDirectory:	offset = isTarget64BitProcess ? 0x38 : 0x24; break;
	case AppCommandLine:		offset = isTarget64BitProcess ? 0x70 : 0x40; break;
	default:
		return L"";
	}

	std::wstring s;
	if (isTargetWow64Process) // OS : 64Bit, Cur : 32 or 64, Tar: 32bit
	{
		PVOID peb32;
		if (!NT_SUCCESS(NtQueryInformationProcess(ProcessHandle, ProcessWow64Information, &peb32, sizeof(PVOID), NULL))) 
			return L"";

		ULONG procParams;
		if (!NT_SUCCESS(NtReadVirtualMemory(ProcessHandle, (PVOID)((ULONG64)peb32 + processParametersOffset), &procParams, sizeof(ULONG), NULL)))
			return L"";

		UNICODE_STRING32 us;
		if (!NT_SUCCESS(NtReadVirtualMemory(ProcessHandle, (PVOID)(ULONG_PTR)(procParams + offset), &us, sizeof(UNICODE_STRING32), NULL)))
			return L"";

		if ((us.Buffer == 0) || (us.Length == 0))
			return L"";

		s.resize(us.Length / 2);
		if (!NT_SUCCESS(NtReadVirtualMemory(ProcessHandle, (PVOID)(ULONG_PTR)us.Buffer, (PVOID)s.c_str(), s.length() * 2, NULL)))
			return L"";
	}
	else if (isWow64Process) //Os : 64Bit, Cur 32, Tar 64
	{
		static _NtQueryInformationProcess query = (_NtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWow64QueryInformationProcess64");
		static _NtWow64ReadVirtualMemory64 read = (_NtWow64ReadVirtualMemory64)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWow64ReadVirtualMemory64");

        PROCESS_BASIC_INFORMATION_WOW64 pbi;
		if (!NT_SUCCESS(query(ProcessHandle, ProcessBasicInformation, &pbi, sizeof(PROCESS_BASIC_INFORMATION_WOW64), NULL))) 
			return L"";
        
		ULONGLONG procParams;
		if (!NT_SUCCESS(read(ProcessHandle, (PVOID64)((ULONGLONG)pbi.PebBaseAddress + processParametersOffset), &procParams, sizeof(ULONGLONG), NULL)))
			return L"";

		UNICODE_STRING64 us;
		if (!NT_SUCCESS(read(ProcessHandle, (PVOID64)(procParams + offset), &us, sizeof(UNICODE_STRING64), NULL)))
			return L"";

		if ((us.Buffer == 0) || (us.Length == 0))
			return L"";
		
		s.resize(us.Length / 2);
		if (!NT_SUCCESS(read(ProcessHandle, (PVOID64)us.Buffer, (PVOID64)s.c_str(), s.length() * 2, NULL)))
			return L"";
	}
	else // Os,Cur,Tar : 64 or 32
	{
		PROCESS_BASIC_INFORMATION pbi;
		if (!NT_SUCCESS(NtQueryInformationProcess(ProcessHandle, ProcessBasicInformation, &pbi, sizeof(PROCESS_BASIC_INFORMATION), NULL))) 
			return L"";

		ULONG_PTR procParams;
		if (!NT_SUCCESS(NtReadVirtualMemory(ProcessHandle, (PVOID)((ULONG64)pbi.PebBaseAddress + processParametersOffset), &procParams, sizeof(ULONG_PTR), NULL)))
			return L"";

		UNICODE_STRING us;
		if (!NT_SUCCESS(NtReadVirtualMemory(ProcessHandle, (PVOID)(procParams + offset), &us, sizeof(UNICODE_STRING), NULL)))
			return L"";

		if ((us.Buffer == 0) || (us.Length == 0))
			return L"";
		
		s.resize(us.Length / 2);
		if (!NT_SUCCESS(NtReadVirtualMemory(ProcessHandle, (PVOID)us.Buffer, (PVOID)s.c_str(), s.length() * 2, NULL)))
			return L"";
	}

	return s;
}

std::wstring GetProcessImageFileNameByProcessId(HANDLE ProcessId)
{
    USHORT bufferSize = 0x100;

    std::wstring buffer;
    buffer.resize(bufferSize/sizeof(wchar_t));

    SYSTEM_PROCESS_ID_INFORMATION processIdInfo;
    processIdInfo.ProcessId = ProcessId;
    processIdInfo.ImageName.Length = 0;
    processIdInfo.ImageName.MaximumLength = bufferSize;
    processIdInfo.ImageName.Buffer = buffer.data();

    NTSTATUS status = NtQuerySystemInformation(SystemProcessIdInformation, &processIdInfo, sizeof(SYSTEM_PROCESS_ID_INFORMATION), NULL);

    if (status == STATUS_INFO_LENGTH_MISMATCH)
    {
        buffer.resize(processIdInfo.ImageName.MaximumLength/sizeof(wchar_t) + 1);
        processIdInfo.ImageName.Buffer = buffer.data();

        status = NtQuerySystemInformation(SystemProcessIdInformation, &processIdInfo, sizeof(SYSTEM_PROCESS_ID_INFORMATION), NULL);
    }

    if (!NT_SUCCESS(status))
        return L"";

    buffer.resize(processIdInfo.ImageName.Length / sizeof(wchar_t));
    return buffer;
}


NTSTATUS MyQuerySystemInformation(std::vector<BYTE>& Info, SYSTEM_INFORMATION_CLASS SystemInformationClass)
{
    NTSTATUS status;
    ULONG bufferSize = 0x00100000;
    Info.resize(bufferSize);

    for(;;)
    {
        status = NtQuerySystemInformation(SystemInformationClass, Info.data(), bufferSize, &bufferSize);

        if (status == STATUS_BUFFER_TOO_SMALL || status == STATUS_INFO_LENGTH_MISMATCH) {
            bufferSize = bufferSize * 10 / 8;
            Info.resize(bufferSize);
        } else
            break;
    }

    return status;
}

bool IsOnARM64()
{
    HMODULE Kernel32 = GetModuleHandleW(L"Kernel32.dll");
    typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS2)(HANDLE, PUSHORT, PUSHORT);
    LPFN_ISWOW64PROCESS2 pIsWow64Process2 = (LPFN_ISWOW64PROCESS2)GetProcAddress(Kernel32, "IsWow64Process2");
    if (pIsWow64Process2)
    {
        USHORT ProcessMachine = 0xFFFF;
        USHORT NativeMachine = 0xFFFF;
        BOOL ok = pIsWow64Process2(GetCurrentProcess(), &ProcessMachine, &NativeMachine);

        if (NativeMachine == IMAGE_FILE_MACHINE_ARM64)
            return true;
    }
    return false;
}