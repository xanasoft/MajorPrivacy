#include "pch.h"
#include "Service.h"
#include "Scoped.h"

#include <ph.h>

#include <subprocesstag.h>

#include <fltuser.h>


STATUS InstallService(PCWSTR Name, PCWSTR FilePath, PCWSTR Display, PCWSTR Group, uint32 Options, const CVariant& Params)
{
    CScopedHandle scmHandle(OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE), CloseServiceHandle);
    if (!scmHandle)
        return ERR(PhGetLastWin32ErrorAsNtStatus()); 
    
    DWORD ServiceType;
    DWORD StartType;
    std::wstring StrPath;

    if (Options & OPT_KERNEL_TYPE)
        ServiceType = SERVICE_KERNEL_DRIVER;
    else {
        ServiceType = SERVICE_WIN32_OWN_PROCESS;

        // quote path
        if (*FilePath != L'\"') {
            StrPath = L"\"" + std::wstring(FilePath) + L"\"";
            FilePath = StrPath.c_str();
        }
    }

    if (Options & OPT_SYSTEM_START)
        StartType = SERVICE_SYSTEM_START;
    else if (Options & OPT_AUTO_START)
        StartType = SERVICE_AUTO_START;
    else
        StartType = SERVICE_DEMAND_START;

    CScopedHandle serviceHandle(CreateServiceW(
        scmHandle,
        Name, Display,
        SERVICE_ALL_ACCESS,
        ServiceType,
        StartType,
        SERVICE_ERROR_IGNORE,
        FilePath,
        Group,
        NULL,                       // no tag identifier
        NULL,                       // no dependencies
        NULL,                       // LocalSystem account
        NULL
    ), CloseServiceHandle);

    if (!serviceHandle)
        return ERR(PhGetLastWin32ErrorAsNtStatus());
        
    KphSetServiceSecurity(serviceHandle);

    if (Params.IsMap())
    {
        std::wstring KeyName = L"System\\CurrentControlSet\\Services\\" + std::wstring(Name) + L"\\Parameters";

        CScopedHandle hKey((HANDLE)NULL, CloseHandle);
        ULONG disposition;
        PH_STRINGREF objectName;
        PhInitializeStringRef(&objectName, (WCHAR*)KeyName.c_str());
        NTSTATUS status = PhCreateKey(&hKey, KEY_WRITE | DELETE, PH_KEY_LOCAL_MACHINE, &objectName, 0, 0, &disposition);
        if (!NT_SUCCESS(status))
            return ERR(status);

        for (uint32 i = 0; i < Params.Count(); i++) 
        {
            std::wstring Name = Params.WKey(i);
            CVariant Value = Params[Params.Key(i)];
            switch (Value.GetType())
            {
                case VAR_TYPE_BYTES: {
                    CBuffer buff = Value.To<CBuffer>();
                    PH_STRINGREF valueName;
                    PhInitializeStringRef(&valueName, (WCHAR*)Name.c_str());
                    status = PhSetValueKey(hKey, &valueName, REG_BINARY, buff.GetBuffer(), (ULONG)buff.GetSize());
                    break;
                }

                case VAR_TYPE_UNICODE:
                case VAR_TYPE_UTF8:
                case VAR_TYPE_ASCII: {
                    std::wstring str = Value.AsStr();
                    PH_STRINGREF valueName;
                    PhInitializeStringRef(&valueName, (WCHAR*)Name.c_str());
                    status = PhSetValueKey(hKey, &valueName, REG_SZ, (PWSTR)str.c_str(), (ULONG)((str.size() + 1) * sizeof(WCHAR)));
                    break;
                }

                case VAR_TYPE_UINT:
                case VAR_TYPE_SINT: {
                    DWORD val = Value.To<uint32>();
                    PH_STRINGREF valueName;
                    PhInitializeStringRef(&valueName, (WCHAR*)Name.c_str());
                    status = PhSetValueKey(hKey, &valueName, REG_DWORD, &val, sizeof(val));
                    break;
                }

                default:
                    status = STATUS_INVALID_PARAMETER;
            }

            if(!NT_SUCCESS(status))
                return ERR(STATUS_INVALID_PARAMETER);
        }
    }
    else if (Params.IsValid()) // must be map or invalid
        return ERR(STATUS_INVALID_PARAMETER);

    if (Options & OPT_START_NOW)
    {
        if (!StartService(serviceHandle, 0, NULL))
            return ERR(PhGetLastWin32ErrorAsNtStatus());
    }
    
    return OK;
}

SVC_STATE GetServiceState(PCWSTR Name)
{
    CScopedHandle scmHandle(OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE), CloseServiceHandle);
    if (!scmHandle)
        return SVC_NOT_FOUND;

    CScopedHandle serviceHandle(OpenService(scmHandle, Name, SERVICE_INTERROGATE), CloseServiceHandle);
    if (!serviceHandle)
        return SVC_NOT_FOUND;

    /*DWORD dwBytesNeeded;
	SERVICE_STATUS_PROCESS svcStatus;
    if (!QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&svcStatus, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded))
        return SVC_INSTALLED;*/

    SERVICE_STATUS svcStatus;
    if (!ControlService(serviceHandle, SERVICE_CONTROL_INTERROGATE, &svcStatus)) 
    {
        if (GetLastError() == ERROR_SERVICE_NOT_ACTIVE)
            svcStatus.dwCurrentState = SERVICE_STOPPED;
        else
            return SVC_INSTALLED;
    }
    
    return svcStatus.dwCurrentState == SERVICE_RUNNING ? SVC_RUNNING : SVC_INSTALLED;

}

STATUS RunService(PCWSTR Name)
{
    CScopedHandle scmHandle(OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE), CloseServiceHandle);
    if (!scmHandle)
        return ERR(PhGetLastWin32ErrorAsNtStatus());

    CScopedHandle serviceHandle(OpenService(scmHandle, Name, SERVICE_INTERROGATE | SERVICE_START), CloseServiceHandle);
    if (!serviceHandle)
        return ERR(PhGetLastWin32ErrorAsNtStatus());

    if(!StartService(serviceHandle, 0, NULL))
        return ERR(PhGetLastWin32ErrorAsNtStatus());

    SERVICE_STATUS svcStatus;
    while (ControlService(serviceHandle, SERVICE_CONTROL_INTERROGATE, &svcStatus) && svcStatus.dwCurrentState == SERVICE_START_PENDING)
        Sleep(100);

    return OK;
}

STATUS KillService(PCWSTR Name)
{
    CScopedHandle scmHandle(OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE), CloseServiceHandle);
    if (!scmHandle)
        return ERR(PhGetLastWin32ErrorAsNtStatus());

    CScopedHandle serviceHandle(OpenService(scmHandle, Name, SERVICE_STOP), CloseServiceHandle);
    if (!serviceHandle)
        return ERR(PhGetLastWin32ErrorAsNtStatus());

	SERVICE_STATUS svcStatus;
    if(!ControlService(serviceHandle, SERVICE_CONTROL_STOP, &svcStatus))
        return ERR(PhGetLastWin32ErrorAsNtStatus());
    return OK;
}

DWORD GetServiceStart(PCWSTR Name)
{
    CScopedHandle scmHandle(OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE), CloseServiceHandle);
	if (!scmHandle)
		return ERR(PhGetLastWin32ErrorAsNtStatus());

	CScopedHandle serviceHandle(OpenService(scmHandle, Name, SERVICE_QUERY_CONFIG), CloseServiceHandle);
	if (!serviceHandle)
		return ERR(PhGetLastWin32ErrorAsNtStatus());

	DWORD bytesNeeded;
	QUERY_SERVICE_CONFIG* config = NULL;
	DWORD startType = 0;
	if (!QueryServiceConfig(serviceHandle, config, 0, &bytesNeeded) && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		config = (QUERY_SERVICE_CONFIG*)malloc(bytesNeeded);
		if (QueryServiceConfig(serviceHandle, config, bytesNeeded, &bytesNeeded))
			startType = config->dwStartType;
		free(config);
	}
	return startType;
}

STATUS SetServiceStart(PCWSTR Name, DWORD StartValue)
{
	CScopedHandle scmHandle(OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE), CloseServiceHandle);
	if (!scmHandle)
		return ERR(PhGetLastWin32ErrorAsNtStatus());

	CScopedHandle serviceHandle(OpenService(scmHandle, Name, SERVICE_CHANGE_CONFIG), CloseServiceHandle);
	if (!serviceHandle)
		return ERR(PhGetLastWin32ErrorAsNtStatus());

    if (!ChangeServiceConfig(serviceHandle, SERVICE_NO_CHANGE, StartValue, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
        return ERR(PhGetLastWin32ErrorAsNtStatus());
    return OK;
}

std::wstring GetServiceNameFromTag(HANDLE ProcessId, PVOID ServiceTag)
{
    static PQUERY_TAG_INFORMATION I_QueryTagInformation = NULL;
    std::wstring serviceName;
    TAG_INFO_NAME_FROM_TAG nameFromTag;

    if (!I_QueryTagInformation)
    {
        I_QueryTagInformation = (PQUERY_TAG_INFORMATION)GetProcAddress(LoadLibraryW(L"sechost.dll"), "I_QueryTagInformation");

        if (!I_QueryTagInformation)
            I_QueryTagInformation = (PQUERY_TAG_INFORMATION)GetProcAddress(LoadLibraryW(L"advapi32.dll"), "I_QueryTagInformation");

        if (!I_QueryTagInformation)
            return L"";
    }

    memset(&nameFromTag, 0, sizeof(TAG_INFO_NAME_FROM_TAG));
    nameFromTag.InParams.dwPid = HandleToUlong(ProcessId);
    nameFromTag.InParams.dwTag = PtrToUlong(ServiceTag);

    I_QueryTagInformation(NULL, eTagInfoLevelNameFromTag, &nameFromTag);

    if (nameFromTag.OutParams.pszName)
    {
        serviceName = nameFromTag.OutParams.pszName;
        LocalFree(nameFromTag.OutParams.pszName);
    }

    return serviceName;
}