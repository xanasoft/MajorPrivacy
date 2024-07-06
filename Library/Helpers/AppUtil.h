#pragma once
#include "../lib_global.h"
#include "../Status.h"
#include "SID.h"

LIBRARY_EXPORT std::wstring GetApplicationDirectory();

LIBRARY_EXPORT std::wstring GetProgramFilesDirectory();

LIBRARY_EXPORT std::wstring ExpandEnvStrings(const std::wstring& command);

LIBRARY_EXPORT std::wstring GetFileNameFromPath(const std::wstring& FilePath);

LIBRARY_EXPORT std::wstring GetFileFromCommand(const std::wstring& Command, std::wstring* Arguments = NULL);

LIBRARY_EXPORT std::wstring GetAppContainerNameBySid(const SSid& Sid);

LIBRARY_EXPORT std::wstring GetAppContainerSidFromName(const std::wstring& PackageFamilyName);

LIBRARY_EXPORT std::wstring GetAppContainerRootPath(const std::wstring& FullPackageName);

//struct LIBRARY_EXPORT SAppPackage
//{
//	std::wstring AppUserModelId;
//    std::wstring PackageName;
//    std::wstring PackageDisplayName;
//    std::wstring PackageFamilyName;
//    std::wstring PackageInstallPath;
//    std::wstring PackageFullName;
//    std::wstring PackageVersion;
//    std::wstring SmallLogoPath;
//    std::wstring PackageSID;
//};
//
//typedef std::shared_ptr<SAppPackage> SAppPackagePtr;
//
//LIBRARY_EXPORT RESULT(std::shared_ptr<std::list<SAppPackagePtr>>) EnumAllAppPackages();

LIBRARY_EXPORT VOID EnumAllAppPackages(BOOLEAN (*callBack)(PVOID param, void* AppPackage, void* AppPackage2), PVOID param);

LIBRARY_EXPORT VOID GetAppPackageInfos(void* AppPackage, void* AppPackage2,
    std::wstring *PackageName,
    std::wstring *PackageFullName,
    std::wstring *PackageFamilyName,
    std::wstring *PackageVersion,
    // below are the slow once
    std::wstring *PackageDisplayName,
    std::wstring *PackageLogo,
    std::wstring *PackageInstallLocation);

LIBRARY_EXPORT bool IsRunningElevated();

LIBRARY_EXPORT HANDLE RunElevated(const std::wstring& path, const std::wstring& params);

LIBRARY_EXPORT std::wstring ExpandEnvironmentVariablesInPath(const std::wstring& path);

LIBRARY_EXPORT std::wstring GetResourceStr(const std::wstring& str);

struct LIBRARY_EXPORT SImageVersionInfo
{
    std::wstring CompanyName;
    std::wstring FileDescription;
    std::wstring FileVersion;
    std::wstring ProductName;
};

typedef std::shared_ptr<SImageVersionInfo> SImageVersionInfoPtr;

LIBRARY_EXPORT SImageVersionInfoPtr GetImageVersionInfo(const std::wstring& FileName);
LIBRARY_EXPORT SImageVersionInfoPtr GetImageVersionInfoNt(const std::wstring& FileNameNt);

typedef enum _PEB_OFFSET
{
	AppCurrentDirectory,
	AppCommandLine,
} PEB_OFFSET;

LIBRARY_EXPORT std::wstring GetPebString(HANDLE ProcessHandle, PEB_OFFSET Offset);

LIBRARY_EXPORT std::wstring GetProcessImageFileNameByProcessId(HANDLE ProcessId);

#ifndef PTR_ADD_OFFSET
#define PTR_ADD_OFFSET(Pointer, Offset) ((PVOID)((ULONG_PTR)(Pointer) + (ULONG_PTR)(Offset)))
#endif

#define PH_FIRST_PROCESS(Processes) ((PSYSTEM_PROCESS_INFORMATION)(Processes))

#define PH_NEXT_PROCESS(Process) ( \
    ((PSYSTEM_PROCESS_INFORMATION)(Process))->NextEntryOffset ? \
    (PSYSTEM_PROCESS_INFORMATION)PTR_ADD_OFFSET((Process), \
    ((PSYSTEM_PROCESS_INFORMATION)(Process))->NextEntryOffset) : \
    NULL \
    )

#define PH_PROCESS_EXTENSION(Process) \
    ((PSYSTEM_PROCESS_INFORMATION_EXTENSION)PTR_ADD_OFFSET((Process), \
    UFIELD_OFFSET(SYSTEM_PROCESS_INFORMATION, Threads) + \
    sizeof(SYSTEM_THREAD_INFORMATION) * \
    ((PSYSTEM_PROCESS_INFORMATION)(Process))->NumberOfThreads))

#define PH_EXTENDED_PROCESS_EXTENSION(Process) \
    ((PSYSTEM_PROCESS_INFORMATION_EXTENSION)PTR_ADD_OFFSET((Process), \
    UFIELD_OFFSET(SYSTEM_PROCESS_INFORMATION, Threads) + \
    sizeof(SYSTEM_EXTENDED_THREAD_INFORMATION) * \
    ((PSYSTEM_PROCESS_INFORMATION)(Process))->NumberOfThreads))

LIBRARY_EXPORT NTSTATUS MyQuerySystemInformation(std::vector<BYTE>& Info, SYSTEM_INFORMATION_CLASS SystemInformationClass);
