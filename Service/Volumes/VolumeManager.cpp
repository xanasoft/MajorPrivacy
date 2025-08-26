#include "pch.h"

#include "ServiceCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "VolumeManager.h"
#include "../../Library/Common/Strings.h"
#include "../../ImBox/ImBox.h"
#include "../Access/AccessManager.h"
#include "../../Library/API/DriverAPI.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../../Library/Helpers/NtIo.h"
#include "../../Library/Helpers/MiscHelpers.h"

#define FILE_SHARE_VALID_FLAGS          0x00000007

CVolumeManager::CVolumeManager()
{
}

CVolumeManager::~CVolumeManager()
{
}

STATUS CVolumeManager::CVolumeManager::Init()
{
    Update();

    return STATUS_SUCCESS;
}

#define REPARSE_MOUNTPOINT_HEADER_SIZE 8

#define IMDISK_DEVICE_LEN 14
#define IMDISK_DEVICE L"\\Device\\ImDisk"
#define DISK_LABEL L"PrivateDisk"
#define RAMDISK_IMAGE L"RamDisk"

#define IMBOX_PROXY L"ImBox_Proxy"
#define IMBOX_EVENT L"ImBox_Event"
#define IMBOX_SECTION L"ImBox_Section"

extern "C" {

    HANDLE WINAPI ImDiskOpenDeviceByMountPoint(LPCWSTR MountPoint, DWORD AccessMode);
    BOOL WINAPI IsImDiskDriverReady();
    BOOL WINAPI ImDiskGetDeviceListEx(IN ULONG ListLength, OUT ULONG* DeviceList);
    WCHAR WINAPI ImDiskFindFreeDriveLetter();
}


std::wstring GetVolumeLabel(const std::wstring& NtPath);
std::wstring ImDiskQueryDeviceProxy(const std::wstring& FileName);
ULONGLONG ImDiskQueryDeviceSize(const std::wstring& FileName);
WCHAR ImDiskQueryDriveLetter(const std::wstring& FileName);

std::wstring GetProxyName(const std::wstring& ImageFile)
{
    std::wstring ProxyName;
    if (ImageFile.empty())
        ProxyName = RAMDISK_IMAGE;
    else {
        ProxyName = ImageFile;
        std::replace(ProxyName.begin(), ProxyName.end(), L'\\', L'/');
    }
    return ProxyName;
}

//RESULT(std::shared_ptr<CVolume>) FindImDisk(const std::wstring& ImageFile)
//{
//    std::wstring TargetNtPath;
//
//    std::wstring ProxyName = GetProxyName(ImageFile);
//
//    //
//    // Find an already mounted RamDisk,
//    // we inspect the volume label to determine if its ours
//    // 
//
//    std::vector<ULONG> DeviceList;
//    DeviceList.resize(3);
//
//retry:
//    if (!ImDiskGetDeviceListEx((ULONG)DeviceList.size(), &DeviceList.front())) {
//        switch (GetLastError())
//        {
//        case ERROR_FILE_NOT_FOUND:
//            return ERR(STATUS_NOT_FOUND);
//
//        case ERROR_MORE_DATA:
//            DeviceList.resize(DeviceList[0] + 1);
//            goto retry;
//
//        default:
//            return ERR(STATUS_UNSUCCESSFUL);
//        }
//    }
//
//    for (ULONG counter = 1; counter <= DeviceList[0]; counter++) {
//
//        std::wstring proxy = ImDiskQueryDeviceProxy(IMDISK_DEVICE + std::to_wstring(DeviceList[counter]));
//        if (!MatchPathPrefix(proxy, L"\\BaseNamedObjects\\Global\\" IMBOX_PROXY))
//            continue;
//        std::size_t pos = proxy.find_first_of(L'!');
//        if (pos == std::wstring::npos || _wcsicmp(proxy.c_str() + (pos + 1), ProxyName.c_str()) != 0)
//            continue;
//
//        //if (GetVolumeLabel(IMDISK_DEVICE + std::to_wstring(DeviceList[counter]) + L"\\") != DISK_LABEL) 
//        //  continue;
//
//        TargetNtPath = IMDISK_DEVICE + std::to_wstring(DeviceList[counter]);
//        break;
//    }
//
//    std::shared_ptr<CVolume> pMount;
//    if (!TargetNtPath.empty()) {
//        pMount = std::make_shared<CVolume>();
//        pMount->m_DevicePath = TargetNtPath;
//        pMount->m_ImageDosPath = ImageFile;
//        pMount->m_VolumeSize = ImDiskQueryDeviceSize(pMount->m_DevicePath);
//        pMount->m_MountPoint = ImDiskQueryDriveLetter(pMount->m_DevicePath) + std::wstring(L":\\");
//    }
//    return pMount;
//}

ULONG FindNextFreeDevice()
{
    std::vector<ULONG> DeviceList;
    DeviceList.resize(3);

retry:
    if (!ImDiskGetDeviceListEx((ULONG)DeviceList.size(), &DeviceList.front())) {
        switch (GetLastError())
        {
        case ERROR_FILE_NOT_FOUND:
            return 0;

        case ERROR_MORE_DATA:
            DeviceList.resize(DeviceList[0] + 1);
            goto retry;

        default:
            return 0;
        }
    }

    if (DeviceList[0] == 0)
		return 1;

	return DeviceList[1] + 1;    
}

/*HANDLE OpenOrCreateNtFolder(const WCHAR* NtPath)
{
    UNICODE_STRING objname;
    RtlInitUnicodeString(&objname, NtPath);

    OBJECT_ATTRIBUTES objattrs;
    InitializeObjectAttributes(&objattrs, &objname, OBJ_CASE_INSENSITIVE, NULL, NULL);

    HANDLE handle = NULL;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS status;

    status = NtCreateFile(&handle, GENERIC_READ | GENERIC_WRITE, &objattrs, &IoStatusBlock, NULL, 0, FILE_SHARE_VALID_FLAGS, FILE_OPEN_IF, FILE_DIRECTORY_FILE | FILE_OPEN_REPARSE_POINT, NULL, 0);
    //if (status == STATUS_OBJECT_PATH_NOT_FOUND) {

    //    std::wstring DosPath = NtPath;
    //    if (!SbieDll_TranslateNtToDosPath((WCHAR*)DosPath.c_str()))
    //        return NULL;

    //    WCHAR* dosPath = (WCHAR*)DosPath.c_str();
    //    *wcsrchr(dosPath, L'\\') = L'\0'; // truncate path as we want the last folder to be created with SbieDll_GetPublicSD
    //    if (__sys_SHCreateDirectoryExW(NULL, dosPath, NULL) != ERROR_SUCCESS)
    //        return NULL;

    //    status = NtCreateFile(&handle, GENERIC_READ | GENERIC_WRITE, &objattrs, &IoStatusBlock, NULL, 0, FILE_SHARE_VALID_FLAGS, FILE_OPEN_IF, FILE_DIRECTORY_FILE | FILE_OPEN_REPARSE_POINT, NULL, 0);
    //}
    if (!NT_SUCCESS(status))
        return NULL;

    return handle;
}

int CreateJunction(const std::wstring& TargetNtPath, const std::wstring& FileRootPath, ULONG session_id)
{
    ULONG errlvl = 0;
    HANDLE handle;

    //
    // open root folder, if not exist, create it
    //

    handle = OpenOrCreateNtFolder(FileRootPath.c_str());
    if(!handle)
        errlvl = 0x33;

    //
    // get the junction target
    //

    BYTE buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];  // We need a large buffer
    REPARSE_DATA_BUFFER& ReparseBuffer = (REPARSE_DATA_BUFFER&)buf;
    DWORD dwRet;

    std::wstring JunctionTarget;
    if (errlvl == 0) {
        if (::DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, NULL, 0, &ReparseBuffer, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dwRet, NULL)) {
            JunctionTarget = ReparseBuffer.MountPointReparseBuffer.PathBuffer;
        }
        // else not a junction
    }

    //
    // check if the junction target is right, if not remove it
    //

    if (errlvl == 0 && !JunctionTarget.empty()) {
        if (_wcsicmp(JunctionTarget.c_str(), TargetNtPath.c_str()) != 0) {
            //SbieApi_LogEx(session_id, 2231, L"%S != %S", JunctionTarget.c_str(), TargetNtPath.c_str());

            memset(buf, 0, REPARSE_MOUNTPOINT_HEADER_SIZE);
            ReparseBuffer.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
            if (!::DeviceIoControl(handle, FSCTL_DELETE_REPARSE_POINT, &ReparseBuffer, REPARSE_MOUNTPOINT_HEADER_SIZE, NULL, 0, &dwRet, NULL))
                errlvl = 0x55; // failed to remove invalid junction target

            JunctionTarget.clear();
        }
    }

    //
    // set junction target, if needed
    //

    if (errlvl == 0 && JunctionTarget.empty()) {

        memset(&ReparseBuffer, 0, sizeof(buf));
        ReparseBuffer.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
        ReparseBuffer.ReparseDataLength = 4 * sizeof(USHORT);

        ReparseBuffer.MountPointReparseBuffer.SubstituteNameOffset = 0;
        ReparseBuffer.MountPointReparseBuffer.SubstituteNameLength = TargetNtPath.length() * sizeof(WCHAR);
        USHORT SubstituteNameSize = ReparseBuffer.MountPointReparseBuffer.SubstituteNameLength + sizeof(WCHAR);
        memcpy(ReparseBuffer.MountPointReparseBuffer.PathBuffer, TargetNtPath.c_str(), SubstituteNameSize);
        ReparseBuffer.ReparseDataLength += SubstituteNameSize;

        ReparseBuffer.MountPointReparseBuffer.PrintNameOffset = SubstituteNameSize;
        ReparseBuffer.MountPointReparseBuffer.PrintNameLength = TargetNtPath.length() * sizeof(WCHAR);
        USHORT PrintNameSize = ReparseBuffer.MountPointReparseBuffer.PrintNameLength + sizeof(WCHAR);
        memcpy(ReparseBuffer.MountPointReparseBuffer.PathBuffer + SubstituteNameSize/sizeof(WCHAR), TargetNtPath.c_str(), PrintNameSize);
        ReparseBuffer.ReparseDataLength += PrintNameSize;

        if (!::DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &ReparseBuffer, ReparseBuffer.ReparseDataLength + REPARSE_MOUNTPOINT_HEADER_SIZE, NULL, 0, &dwRet, NULL))
            errlvl = 0x44;
    }

    if (handle)
        CloseHandle(handle);

    return errlvl;
}

bool RemoveJunction(const std::wstring& FileRootPath, ULONG session_id)
{
    bool ok = false;

    UNICODE_STRING objname;
    RtlInitUnicodeString(&objname, FileRootPath.c_str());

    OBJECT_ATTRIBUTES objattrs;
    InitializeObjectAttributes(&objattrs, &objname, OBJ_CASE_INSENSITIVE, NULL, NULL);

    HANDLE handle = NULL;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS status;

    status = NtCreateFile(&handle, GENERIC_READ | GENERIC_WRITE, &objattrs, &IoStatusBlock, NULL, 0, FILE_SHARE_VALID_FLAGS, FILE_OPEN, FILE_DIRECTORY_FILE | FILE_OPEN_REPARSE_POINT, NULL, 0);
    if (NT_SUCCESS(status)) {

        BYTE buf[REPARSE_MOUNTPOINT_HEADER_SIZE];
        REPARSE_DATA_BUFFER& ReparseBuffer = (REPARSE_DATA_BUFFER&)buf;
        DWORD dwRet;

        //
        // remove junction
        //

        memset(&ReparseBuffer, 0, REPARSE_MOUNTPOINT_HEADER_SIZE);
        ReparseBuffer.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
        ok = !!::DeviceIoControl(handle, FSCTL_DELETE_REPARSE_POINT, &ReparseBuffer, REPARSE_MOUNTPOINT_HEADER_SIZE, NULL, 0, &dwRet, NULL);

        CloseHandle(handle);

        NtDeleteFile(&objattrs);
    }

    return ok;
}*/

RESULT(PVOID) AllocPasswordMemory(HANDLE hProcess, const wchar_t* pPassword)
{
    NTSTATUS status;

    PVOID pMem = NULL;
    SIZE_T uSize = 0x1000;

    if(!NT_SUCCESS(status = NtAllocateVirtualMemory(hProcess, &pMem, 0, &uSize, MEM_COMMIT, PAGE_READWRITE)))
        return ERR(status);

#define VM_LOCK_1                0x0001   // This is used, when calling KERNEL32.DLL VirtualLock routine
#define VM_LOCK_2                0x0002   // This require SE_LOCK_MEMORY_NAME privilege
    if(!NT_SUCCESS(status = NtLockVirtualMemory(hProcess, &pMem, &uSize, VM_LOCK_1)))
        return ERR(status);

    if (pPassword && *pPassword)
    {
        if(!NT_SUCCESS(status = NtWriteVirtualMemory(hProcess, pMem, (PVOID)pPassword, (wcslen(pPassword) + 1) * 2, NULL)))
            return ERR(status);
    }

    RETURN(pMem);
}

STATUS UpdateCommandLine(HANDLE hProcess, NTSTATUS(*Update)(std::wstring& s, PVOID p), PVOID param)
{
    NTSTATUS status;

    ULONG processParametersOffset = 0x20;
    ULONG appCommandLineOffset = 0x70;

    PROCESS_BASIC_INFORMATION pbi;
    if (!NT_SUCCESS(status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(PROCESS_BASIC_INFORMATION), NULL)))
        return status;
    
    ULONG_PTR procParams;
    if (!NT_SUCCESS(status = NtReadVirtualMemory(hProcess, (PVOID)((ULONG64)pbi.PebBaseAddress + processParametersOffset), &procParams, sizeof(ULONG_PTR), NULL)))
        return status;
        
    UNICODE_STRING us;
    if (!NT_SUCCESS(status = NtReadVirtualMemory(hProcess, (PVOID)(procParams + appCommandLineOffset), &us, sizeof(UNICODE_STRING), NULL)))
        return status;
            
    if ((us.Buffer == 0) || (us.Length == 0))
        return STATUS_UNSUCCESSFUL;
                
    std::wstring s;
    s.resize(us.Length / 2);
    if (!NT_SUCCESS(status = NtReadVirtualMemory(hProcess, (PVOID)us.Buffer, (PVOID)s.c_str(), s.length() * 2, NULL)))
        return status;

    if (!NT_SUCCESS(status = Update(s, param)))
        return status;

    if (!NT_SUCCESS(status = NtWriteVirtualMemory(hProcess, (PVOID)us.Buffer, (PVOID)s.c_str(), s.length() * 2, NULL)))
        return status;

    return STATUS_SUCCESS;
}

RESULT(std::shared_ptr<CVolume>) MountImDisk(const std::wstring& ImageFile, const wchar_t* pPassword, ULONG64 sizeKb, const std::wstring& MountPoint = L"", ULONG DeviceNumber = 0)
{
    std::shared_ptr<CVolume> pMount = std::make_shared<CVolume>();

    std::wstring ProxyName = GetProxyName(ImageFile);

    //
    // mount a new disk
    // we need to use a temporary drive letter in order to format the volume using the fmifs.dll API
    //

    // todo allow mounting without mount

    const wchar_t* drvLetter = NULL;
    if(!MountPoint.empty() && MountPoint.size() <= 3 && MountPoint[1] == L':')
		drvLetter = MountPoint.c_str();

    WCHAR Drive[4] = L"\0:";
    if (drvLetter) {
        WCHAR letter = towupper(drvLetter[0]);
        if (letter >= L'A' && letter <= L'Z' && drvLetter[1] == L':') {
            DWORD logical_drives = GetLogicalDrives();
            if((logical_drives & (1 << (letter - L'A'))) == 0)
                Drive[0] = letter;
        }
    }
    else
        Drive[0] = ImDiskFindFreeDriveLetter();
    if (Drive[0] == 0)
        return ERR(STATUS_INSUFFICIENT_RESOURCES);

    std::wstring cmd;
    if (ImageFile.empty()) cmd = L"ImBox type=ram";
    else cmd = L"ImBox type=img image=\"" + ImageFile + L"\"";
    if (pPassword && *pPassword) cmd += L" cipher=AES";
    //cmd += L" size=" + std::to_wstring(sizeKb) + L" mount=" + std::wstring(Drive) + L" format=ntfs:" DISK_LABEL;
    cmd += L" size=" + std::to_wstring(sizeKb) + L" mount=" + std::wstring(Drive) + L" format=ntfs";
    if(DeviceNumber) cmd += L" number=" + std::to_wstring(DeviceNumber);

#ifdef _M_ARM64
    ULONG64 ctr = _ReadStatusReg(ARM64_CNTVCT);
#else
    ULONG64 ctr = __rdtsc();
#endif

    WCHAR sName[32];
    wsprintf(sName, L"_%08X_%08X%08X", GetCurrentProcessId(), (ULONG)(ctr >> 32), (ULONG)ctr);

    cmd += L" proxy=" IMBOX_PROXY + std::wstring(sName) + L"!" + ProxyName;

    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, (IMBOX_EVENT + std::wstring(sName)).c_str());
    cmd += L" event=" IMBOX_EVENT + std::wstring(sName);

    //SSection* pSection = NULL;
    //HANDLE hMapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x1000, (IMBOX_SECTION + std::wstring(sName)).c_str());
    //if (hMapping) {
    //    pSection = (SSection*)MapViewOfFile(hMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0x1000);
    //    memset(pSection, 0, 0x1000);
    //}
    //cmd += L" section=" IMBOX_SECTION + std::wstring(sName);
    //
    //if (pPassword && *pPassword)
    //    wmemcpy(pSection->in.pass, pPassword, wcslen(pPassword) + 1);

    cmd += L" mem=0x0000000000000000";

    VOID* pMem = NULL;
    RESULT(std::shared_ptr<CVolume>) Result;

    std::wstring app = theCore->GetAppDir() + L"\\ImBox.exe";
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi = { 0 };
    if (CreateProcess(app.c_str(), (WCHAR*)cmd.c_str(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
    {
        NTSTATUS status = STATUS_SUCCESS;

        auto Mem = AllocPasswordMemory(pi.hProcess, pPassword);

        if (Mem.IsError()) 
            status = Mem.GetStatus();
        else
        {
            pMem = Mem.GetValue();

            status = UpdateCommandLine(pi.hProcess, [](std::wstring& s, PVOID p) {
                // Note: Do not change the string length, that would require a different update mechanism.
				size_t pos = s.find(L"mem=0x0000000000000000");
				if (pos != std::wstring::npos) {
					wchar_t Addr[17];
                    toHexadecimal((ULONG64)p, Addr);
					s.replace(pos + 6, 16, Addr);
                    return STATUS_SUCCESS;
				}
                return STATUS_UNSUCCESSFUL;
			}, pMem);
        }

        if (!NT_SUCCESS(status)) {
            TerminateProcess(pi.hProcess, -1);
            pMem = NULL;
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            Result = status;
        }
    }

    if (pMem)
    {
        ResumeThread(pi.hThread);

        //
        // Wait for ImDisk to be mounted
        //

        HANDLE hEvents[] = { hEvent, pi.hProcess };
        DWORD dwEvent = WaitForMultipleObjects(ARRAYSIZE(hEvents), hEvents, FALSE, 40 * 1000);
        if (dwEvent != WAIT_OBJECT_0) {
            DWORD ret;
            GetExitCodeProcess(pi.hProcess, &ret);
            if (ret == STILL_ACTIVE) {
                Result = ERR(STATUS_TIMEOUT);
                //TerminateProcess(pi.hProcess, -1);
            }
            else {
                switch (ret) {
                case ERR_FILE_NOT_OPENED:   Result = ERR(STATUS_NOT_FOUND); break;
                case ERR_UNKNOWN_CIPHER:    Result = ERR(STATUS_INVALID_PARAMETER_1); break;
                case ERR_WRONG_PASSWORD:    Result = ERR(STATUS_WRONG_PASSWORD); break;
                case ERR_KEY_REQUIRED:      Result = ERR(STATUS_INVALID_PARAMETER_2); break;
                case ERR_IMDISK_FAILED:     Result = ERR(STATUS_INTERNAL_ERROR); break;
                case ERR_IMDISK_TIMEOUT:    Result = ERR(STATUS_TIMEOUT); break;
                default:                    Result = ERR(STATUS_UNSUCCESSFUL); break;
                }
            }
        }
        else 
        {
            union {
                SSection pSection[1];
                BYTE pSpace[0x1000];
            };
            if (NT_SUCCESS(NtReadVirtualMemory(pi.hProcess, (PVOID)pMem, pSection, 0x1000, NULL)))
            {
                if (_wcsnicmp(pSection->out.mount, IMDISK_DEVICE, IMDISK_DEVICE_LEN) == 0)
                    pMount->m_DevicePath = std::wstring(pSection->out.mount);
            }

            //if (_wcsnicmp(pSection->out.mount, IMDISK_DEVICE, IMDISK_DEVICE_LEN) == 0)
            //    pMount->m_DevicePath = std::wstring(pSection->out.mount);
            //else { // fallback
            //    HANDLE handle = ImDiskOpenDeviceByMountPoint(Drive, 0);
            //    if (handle != INVALID_HANDLE_VALUE) {
            //        BYTE buffer[MAX_PATH];
            //        DWORD length = sizeof(buffer);
            //        if (NT_SUCCESS(NtQueryObject(handle, ObjectNameInformation, buffer, length, &length))) {
            //            UNICODE_STRING* uni = &((OBJECT_NAME_INFORMATION*)buffer)->Name;
            //            length = uni->Length / sizeof(WCHAR);
            //            if (uni->Buffer) {
            //                uni->Buffer[length] = 0;
            //                pMount->m_DevicePath = uni->Buffer;
            //            }
            //        }
            //        CloseHandle(handle);
            //    }
            //}

            if (pMount->m_DevicePath.empty())
                Result = ERR(STATUS_UNSUCCESSFUL);
            else
            {
                Result = pMount;

                if (!drvLetter) {
                    if (!DefineDosDevice(DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE | DDD_RAW_TARGET_PATH, Drive, pMount->m_DevicePath.c_str())) {
                        //todo log error
                    }
                }
            }
        }

        if(!Result.IsError())
            pMount->m_ProcessHandle = pi.hProcess;
        else
            CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    //if (pSection) {
    //    memset(pSection, 0, 0x1000);
    //    UnmapViewOfFile(pSection);
    //}
    //if(hMapping) 
    //    CloseHandle(hMapping);

    if(hEvent) 
        CloseHandle(hEvent);

    if (Result.IsError())
        return Result;

    pMount->m_ImageDosPath = ImageFile;
    pMount->m_VolumeSize = ImDiskQueryDeviceSize(pMount->m_DevicePath);
    pMount->m_MountPoint = MountPoint;

    return Result;
}

bool TryUnmountImDisk(const std::wstring& Device, HANDLE hProcess, int iMode)
{
    bool ok = false;

    if (Device.size() <= IMDISK_DEVICE_LEN)
        return false; // not an imdisk path
    std::wstring cmd;
    switch (iMode)
    {
    case 0: cmd = L"imdisk -d -u "; break;  // graceful
    case 1: cmd = L"imdisk -D -u "; break;  // forced
    case 2: cmd = L"imdisk -R -u "; break;  // emergency
    }
    cmd += Device.substr(IMDISK_DEVICE_LEN);

    STARTUPINFO si = { sizeof(STARTUPINFO) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };
    if (CreateProcess(NULL, (WCHAR*)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        if (WaitForSingleObject(pi.hProcess, 10 * 1000) == WAIT_OBJECT_0) {
            DWORD ret = 0;
            GetExitCodeProcess(pi.hProcess, &ret);
            ok = (ret == 0 || ret == 1); // 0 - ok // 1 - device not found (already unmounted?)
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        if (ok && hProcess) {
            WaitForSingleObject(hProcess, 10 * 1000);
            CloseHandle(hProcess);
        }
    }

    return ok;
}

bool UnmountImDisk(const std::wstring& Device, HANDLE hProcess) 
{
    for (int i = 0; i < 7; i++) { // 5 attempt forced and 2 emergency
        if (TryUnmountImDisk(Device, hProcess, i > 4 ? 2 : 1))
            return true;
        Sleep(1000);
    }
    // last emergency attempt
    TryUnmountImDisk(Device, hProcess, 2);
    return false; // report an error when emergency unmoutn was needed
}

STATUS ExecImDisk(const std::wstring& ImageFile, const wchar_t* pPassword, const std::wstring& Command, bool bWrite, CBuffer* pBuffer, USHORT uId)
{
    STATUS Status = ERR(STATUS_UNSUCCESSFUL);

    std::wstring cmd;
    /*if (ImageFile.empty()) cmd = L"ImBox type=ram";
    else*/ cmd = L"ImBox type=img image=\"" + ImageFile + L"\"";
    cmd += L" " + Command;

#ifdef _M_ARM64
    ULONG64 ctr = _ReadStatusReg(ARM64_CNTVCT);
#else
    ULONG64 ctr = __rdtsc();
#endif

    WCHAR sName[32];
    wsprintf(sName, L"_%08X_%08X%08X", GetCurrentProcessId(), (ULONG)(ctr >> 32), (ULONG)ctr);

    cmd += L" mem=0x0000000000000000";

    VOID* pMem = NULL;

    std::wstring app = theCore->GetAppDir() + L"\\ImBox.exe";
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi = { 0 };
    if (CreateProcessW(app.c_str(), (WCHAR*)cmd.c_str(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
    {
        NTSTATUS status = STATUS_SUCCESS;

        auto Mem = AllocPasswordMemory(pi.hProcess, pPassword);

        if (Mem.IsError()) 
            status = Mem.GetStatus();
        else
        {
            pMem = Mem.GetValue();

            status = UpdateCommandLine(pi.hProcess, [](std::wstring& s, PVOID p) {
                // Note: Do not change the string length, that would require a different update mechanism.
                size_t pos = s.find(L"mem=0x0000000000000000");
                if (pos != std::wstring::npos) {
                    wchar_t Addr[17];
                    toHexadecimal((ULONG64)p, Addr);
                    s.replace(pos + 6, 16, Addr);
                    return STATUS_SUCCESS;
                }
                return STATUS_UNSUCCESSFUL;
                }, pMem);
        }

        if (NT_SUCCESS(status) && pBuffer && bWrite) 
        {
            union {
                SSection pSection[1];
                BYTE pSpace[0x1000];
            };
            memset(pSpace, 0, sizeof(pSpace));

            memcpy(pSection->in.pass, pPassword, (wcslen(pPassword) + 1) * sizeof(wchar_t));
            pSection->magic = SECTION_MAGIC;
            pSection->id = uId;
            pSection->size = (USHORT)pBuffer->GetSize();
            memcpy(pSection->data, pBuffer->GetBuffer(), pSection->size);

            status = NtWriteVirtualMemory(pi.hProcess, pMem, (PVOID)pSpace, sizeof(pSpace), NULL);
        }

        if (!NT_SUCCESS(status)) {
            TerminateProcess(pi.hProcess, -1);
            pMem = NULL;
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }

    if (pMem)
    {
        ResumeThread(pi.hThread);


        HANDLE hEvents[] = { /*hEvent,*/ pi.hProcess };
        DWORD dwEvent = WaitForMultipleObjects(ARRAYSIZE(hEvents), hEvents, FALSE, 40 * 1000);
        //if (dwEvent != WAIT_OBJECT_0)

        DWORD ret;
        GetExitCodeProcess(pi.hProcess, &ret);
        if (ret == STILL_ACTIVE)
            Status = ERR(STATUS_TIMEOUT);
        else if(ret != ERR_OK)
            Status = ERR(STATUS_UNSUCCESSFUL);
        else
        {
            if (pBuffer && !bWrite) 
            {
                union {
                    SSection pSection[1];
                    BYTE pSpace[0x1000];
                };
                if (NT_SUCCESS(NtReadVirtualMemory(pi.hProcess, (PVOID)pMem, pSpace, sizeof(pSpace), NULL)))
                {
                    // to do, don't care
                }
            }
            Status = OK;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return Status;
}


STATUS CVolumeManager::Update()
{
    std::vector<ULONG> DeviceList;
    DeviceList.resize(3);

retry:
    if (!ImDiskGetDeviceListEx((ULONG)DeviceList.size(), &DeviceList.front())) {
        switch (GetLastError())
        {
        case ERROR_FILE_NOT_FOUND:
            return STATUS_DEVICE_NOT_READY;

        case ERROR_MORE_DATA:
            DeviceList.resize(DeviceList[0] + 1);
            goto retry;

        default:
            return STATUS_DEVICE_NOT_READY;
        }
    }

    std::unique_lock Lock(m_Mutex);

    std::map<std::wstring, CVolumePtr> OldVolumes = m_Volumes;

    for (ULONG counter = 1; counter <= DeviceList[0]; counter++)
    {
        std::wstring DevicePath = IMDISK_DEVICE + std::to_wstring(DeviceList[counter]);

        auto F = OldVolumes.find(MkLower(DevicePath));
        if (F != OldVolumes.end()) {
            OldVolumes.erase(F);
            continue;
        }

        std::wstring proxy = ImDiskQueryDeviceProxy(IMDISK_DEVICE + std::to_wstring(DeviceList[counter]));
        if (!MatchPathPrefix(proxy, L"\\BaseNamedObjects\\Global\\" IMBOX_PROXY))
            continue;
        std::size_t pos = proxy.find_first_of(L'!');
        if (pos == std::wstring::npos)
            continue;

        //if (GetVolumeLabel(IMDISK_DEVICE + std::to_wstring(DeviceList[counter]) + L"\\") != DISK_LABEL) 
        //  continue;

        auto pMount = std::make_shared<CVolume>();
        pMount->m_DevicePath = DevicePath;
        pMount->m_ImageDosPath = proxy.c_str() + (pos + 1);
        pMount->m_VolumeSize = ImDiskQueryDeviceSize(pMount->m_DevicePath);
        pMount->m_MountPoint = ImDiskQueryDriveLetter(pMount->m_DevicePath) + std::wstring(L":\\");
        std::replace(pMount->m_ImageDosPath.begin(), pMount->m_ImageDosPath.end(), L'/', L'\\');

        m_Volumes[MkLower(pMount->m_DevicePath)] = pMount;
        m_VolumesByPath[MkLower(pMount->m_ImageDosPath)] = pMount;
    }

    for (auto& pMount : OldVolumes)
    {
        m_VolumesByPath.erase(MkLower(pMount.second->m_ImageDosPath));
        m_Volumes.erase(pMount.first);
    }

    //
    // Apply pending volume data saves
    //

    for (auto I = m_Volumes.begin(); I != m_Volumes.end(); ++I)
    {
        if (!I->second->m_bDataDirty) 
            continue;
        I->second->m_bDataDirty = false;

        STATUS status = SaveVolumeRules(I->second);
        if (!NT_SUCCESS(status))
            theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_VOL_PROTECT_ERROR, L"Failed to Save Volume Rules, error: 0x%08X, Volume: %s", status, I->second->ImageDosPath());
    }

    return STATUS_SUCCESS;
}

STATUS CVolumeManager::CreateImage(const std::wstring& Path, const std::wstring& Password, uint64 uSize, const std::wstring& Cipher)
{
    // todo: Cipher
    auto Res = MountImDisk(Path, Password.c_str(), uSize);
    if(Res.IsError())
        return Res;

    std::shared_ptr<CVolume> pMount =  Res.GetValue();
    UnmountImDisk(pMount->m_DevicePath, pMount->m_ProcessHandle);
    return STATUS_SUCCESS;
}

STATUS CVolumeManager::ChangeImagePassword(const std::wstring& Path, const std::wstring& OldPassword, const std::wstring& NewPassword)
{
    CBuffer Data(1024);
    Data.WriteData(NewPassword.c_str(), NewPassword.length() * sizeof(wchar_t));
    return ExecImDisk(Path, OldPassword.c_str(), L"new_key", true, &Data, SECTION_PARAM_ID_KEY);
}

//STATUS CVolumeManager::DeleteImage(const std::wstring& Path)
//{
//	return STATUS_NOT_IMPLEMENTED;
//}

STATUS CVolumeManager::MountImage(const std::wstring& Path, const std::wstring& MountPoint, const std::wstring& Password, bool bProtect)
{
    std::unique_lock Lock(m_Mutex);

    auto F = m_VolumesByPath.find(MkLower(Path));
    if(F != m_VolumesByPath.end())
		return STATUS_ALREADY_COMPLETE;

    if (MountPoint.size() > 3)
        return STATUS_NOT_IMPLEMENTED;

    ULONG Number = FindNextFreeDevice();
    if(!Number)
        return STATUS_DEVICE_NOT_READY;
    std::wstring DevicePath = IMDISK_DEVICE + std::to_wstring(Number);

    //
    // Set up protection for the selected device number before actually mounting it
    //

    std::wstring ruleGuid;
    if (bProtect) 
    {
        CProgramID ID(EProgramType::eAllPrograms);
        CAccessRulePtr pRule = std::make_shared<CAccessRule>(ID);
        pRule->SetName(L"#Protect," + Path);
        pRule->SetAccessPath(DevicePath + L"\\*");
        pRule->SetType(EAccessRuleType::eProtect);
        pRule->SetTemporary(true);
        auto Res = theCore->AccessManager()->AddRule(pRule);
        if(!Res.IsError()) 
            ruleGuid = Res.GetValue();
        
        STATUS Status = ProtectVolume(Path, DevicePath);
        //Status = theCore->Driver()->SetupRuleAlias(DosPathToNtPath(Path), DevicePath); 
        if (Status.IsError())
            return Status;
    }

    //
    // Mount Volume
    //

    auto Res = MountImDisk(Path, Password.c_str(), 0, MountPoint, Number);
    if (Res.IsError()) {
        if(!ruleGuid.empty())
            theCore->AccessManager()->RemoveRule(ruleGuid);
        return Res;
    }

    if (theCore->Config()->GetBool("Service", "GuardHibernation", false) && !m_NoHibernation)
    {
        if(theCore->Driver()->AcquireHibernationPrevention())
            m_NoHibernation = true;
    }

    std::shared_ptr<CVolume> pMount = Res.GetValue();

    /*if (MountPoint.size() > 3) {
        NTSTATUS status = CreateJunction(pMount->m_DevicePath + L"\\", DosPathToNtPath(pMount->m_MountPoint), 0);
        if (!NT_SUCCESS(status)) {
            DismountVolume(pMount);
            return status;
        }
    }*/

    pMount->m_bProtected = bProtect;

    m_Volumes[MkLower(pMount->m_DevicePath)] = pMount;
    m_VolumesByPath[MkLower(pMount->m_ImageDosPath)] = pMount;

    //
    // Load rules for the mounted volume
    //

    if (bProtect)
    {
        STATUS status = LoadVolumeRules(pMount);
        if(!NT_SUCCESS(status))
            theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_VOL_PROTECT_ERROR, L"Failed to Load Volume Rules, error: 0x%08X, Volume: %s", status, pMount->ImageDosPath());
    }

    //
    // Clean temporary protection rule
    //

    if(!ruleGuid.empty())
        theCore->AccessManager()->RemoveRule(ruleGuid);

    return OK;
}

STATUS CVolumeManager::DismountVolume(const std::shared_ptr<CVolume>& pMount)
{
    if (pMount->m_bDataDirty) {
        STATUS status = SaveVolumeRules(pMount);
        if (!NT_SUCCESS(status))
            theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_VOL_PROTECT_ERROR, L"Failed to Save Volume Rules, error: 0x%08X, Volume: %s", status, pMount->ImageDosPath());
    }

    //
    // When unmounting imdisk.exe needs to be able to lock the volume
    //

    CProgramID ID(theCore->NormalizePath( L"\\SystemRoot\\System32\\imdisk.exe"));
    CAccessRulePtr pRule = std::make_shared<CAccessRule>(ID);
    pRule->SetName(L"#Unmount," + pMount->ImageDosPath());
    pRule->SetAccessPath(pMount->DevicePath());
    pRule->SetType(EAccessRuleType::eAllow);
    pRule->SetTemporary(true);
    auto Res = theCore->AccessManager()->AddRule(pRule); // returns guid on success
    std::wstring ruleGuid;
    if(!Res.IsError()) 
        ruleGuid = Res.GetValue();
    //
    // Inmount the volume
    //

    bool bOk = UnmountImDisk(pMount->m_DevicePath, pMount->m_ProcessHandle);

    //
    // Clean up rules
    //

    if (!bOk) {
        if(!ruleGuid.empty())
            theCore->AccessManager()->RemoveRule(ruleGuid);
        return STATUS_UNSUCCESSFUL;
    }
    // else UnProtectVolume wil clean up all aplicable rules

    UnProtectVolume(pMount->m_DevicePath);
    //theCore->Driver()->ClearRuleAlias(pMount->m_DevicePath);

    return OK;
}

STATUS CVolumeManager::DismountVolume(const std::wstring& DevicePath)
{
    std::unique_lock Lock(m_Mutex);

	auto F = m_Volumes.find(MkLower(DevicePath));
	if(F == m_Volumes.end())
		return STATUS_NOT_FOUND;

    STATUS Status = DismountVolume(F->second);
    if (Status.IsError())
        return Status;

    m_VolumesByPath.erase(MkLower(F->second->m_ImageDosPath));
    m_Volumes.erase(F);

    if (m_Volumes.empty() && m_NoHibernation)
    {
        if (theCore->Driver()->ReleaseHibernationPrevention())
            m_NoHibernation = false;
    }
    
	return OK;
}

STATUS CVolumeManager::DismountAll()
{
    std::unique_lock Lock(m_Mutex);

    for(auto I = m_Volumes.begin(); I != m_Volumes.end();)
	{
        STATUS Status = DismountVolume(I->second);
        if (Status.IsError())
            I++;
        else {
            m_VolumesByPath.erase(MkLower(I->second->m_ImageDosPath));
            I = m_Volumes.erase(I);
        }
	}

    if (!m_Volumes.empty())
        return STATUS_UNSUCCESSFUL;

    if (m_NoHibernation)
    {
        if (theCore->Driver()->ReleaseHibernationPrevention())
            m_NoHibernation = false;
    }

    return STATUS_SUCCESS;
}

RESULT(std::vector<std::wstring>) CVolumeManager::GetVolumeList()
{
    Update();

    std::unique_lock Lock(m_Mutex);

	std::vector<std::wstring> Volumes;
	for (auto& V : m_Volumes)
		Volumes.push_back(V.second->m_DevicePath);
	return Volumes;	
}

RESULT(std::vector<CVolumePtr>) CVolumeManager::GetAllVolumes()
{
    Update();

	std::unique_lock Lock(m_Mutex);

	std::vector<CVolumePtr> Volumes;
	for (auto& V : m_Volumes)
		Volumes.push_back(V.second);
	return Volumes;
}

RESULT(CVolumePtr) CVolumeManager::GetVolumeInfo(const std::wstring& DevicePath)
{
    std::unique_lock Lock(m_Mutex);

    auto F = m_Volumes.find(MkLower(DevicePath));
    if (F != m_Volumes.end())
		return F->second;
	return STATUS_NOT_IMPLEMENTED;
}

STATUS CVolumeManager::ProtectVolume(const std::wstring& Path, const std::wstring& DevicePath)
{
    auto Rules = theCore->AccessManager()->GetAllRules();
    for (auto I : Rules) 
    {
        if(!I.second->IsEnabled())
            continue;
        TryAddRule(I.second, Path, DevicePath);
    }

    return OK; // todo
}

STATUS CVolumeManager::UnProtectVolume(const std::wstring& DevicePath)
{
    auto Rules = theCore->AccessManager()->GetAllRules();
    for(auto I: Rules) 
    {
        auto pRule = I.second;

        std::wstring RulePath = pRule->GetAccessPath();
        if (MatchPathPrefix(RulePath, DevicePath.c_str()))
            theCore->AccessManager()->RemoveRule(pRule->GetGuid());
    }

    return OK;
}

STATUS CVolumeManager::TryAddRule(const CAccessRulePtr& pRule, const std::wstring& Path, const std::wstring& DevicePath)
{
    std::wstring RulePath = pRule->GetAccessPath();
    auto SPos = RulePath.find(L"/");
    if(SPos == std::wstring::npos)
        return OK;

    // todo allow for patterns

    std::wstring Prefix = RulePath.substr(0,SPos);
    if (_wcsicmp(Path.c_str(), Prefix.c_str()) == 0)
    {
        std::wstring AccessPath = DevicePath + L"\\" + RulePath.substr(SPos + 1);

        auto pClone = pRule->Clone();
        pClone->SetTemporary(true);
        pClone->SetData(API_S_RULE_REF_GUID, pRule->GetGuid().ToVariant(true));
        pClone->SetAccessPath(AccessPath);
        return theCore->AccessManager()->AddRule(pClone);
    }

    return OK;
}

void CVolumeManager::UpdateRule(const CAccessRulePtr& pRule, enum class EConfigEvent Event, uint64 PID)
{
    if (PID == GetCurrentProcessId())
        return;

    //
    // Volume Rules
    //

    if (!pRule->IsTemporary() && pRule->GetAccessPath().find(IMDISK_DEVICE) == 0)
    {
        size_t pos = pRule->GetAccessPath().find(L"\\", IMDISK_DEVICE_LEN);
        std::wstring DevicePath = pRule->GetAccessPath().substr(0, pos);

        auto F = m_Volumes.find(MkLower(DevicePath));
        if (F != m_Volumes.end()) {
            if (F->second->m_bProtected) 
                F->second->m_bDataDirty = true;
        }
    }

    //
    // Image Rules
    //

    // remove old rule
    auto Rules = theCore->AccessManager()->GetAllRules();
    for(auto I: Rules) 
    {
        auto pRule = I.second;
        std::wstring RulePath = pRule->GetAccessPath();
        if (pRule->IsTemporary()) {
            CFlexGuid Guid;
            Guid.FromVariant(pRule->GetData(API_S_RULE_REF_GUID));
            if(Guid == pRule->GetGuid())
                theCore->AccessManager()->RemoveRule(pRule->GetGuid());
        }
    }

    if(Event == EConfigEvent::eRemoved)
        return;
    
    // add new or updated rule
    if (pRule->IsEnabled() && pRule->GetAccessPath().find(L"/") != std::wstring::npos)
    {
        std::unique_lock Lock(m_Mutex);

        for (auto I = m_Volumes.begin(); I != m_Volumes.end(); ++I)
        {
            std::wstring Path = I->second->m_ImageDosPath;
            std::wstring DevicePath = I->second->m_DevicePath;

            TryAddRule(pRule, Path, DevicePath);
        }
    }
}

STATUS CVolumeManager::LoadVolumeRules(const std::shared_ptr<CVolume>& pMount)
{
    CBuffer Buffer;
    NTSTATUS status = NtIo_ReadFile(pMount->DevicePath() + DEF_MP_SYS_FILE, &Buffer);
    if (!NT_SUCCESS(status)) {
        if(status == STATUS_OBJECT_NAME_NOT_FOUND)
            return STATUS_SUCCESS; // file not (yet) being there is ok
        return status;
    }
    
    StVariant Data;
    if (Data.FromPacket(&Buffer, true) != StVariant::eErrNone)
        return STATUS_UNSUCCESSFUL;

    pMount->m_Data = Data.Clone(); // we want the m_Data to be writable

    const StVariant& RuleList = pMount->m_Data[API_S_ACCESS_RULES];

    for (uint32 i = 0; i < RuleList.Count(); i++)
    {
        StVariant Rule = RuleList[i];

        //std::wstring Guid = Rule[API_S_GUID].AsStr();

        std::wstring ProgramPath = theCore->NormalizePath(Rule[API_S_FILE_PATH].AsStr());
        CProgramID ID(ProgramPath);

        CAccessRulePtr pRule = std::make_shared<CAccessRule>(ID);
        pRule->FromVariant(Rule);
        pRule->SetAccessPath(pMount->m_DevicePath + L"\\" + pRule->GetAccessPath());
		pRule->SetVolumeRule(true);

        theCore->AccessManager()->AddRule(pRule);
    }

    return STATUS_SUCCESS;
}

STATUS CVolumeManager::SaveVolumeRules(const std::shared_ptr<CVolume>& pMount)
{
    StVariant RuleList;

    SVarWriteOpt Opts;
    Opts.Format = SVarWriteOpt::eMap;
    Opts.Flags = SVarWriteOpt::eTextGuids;

    auto Rules = theCore->AccessManager()->GetAllRules();
    for(auto I: Rules) 
    {
        auto pRule = I.second;
        if(pRule->IsTemporary())
            continue;

        std::wstring RulePath = pRule->GetAccessPath();
        if (MatchPathPrefix(RulePath, pMount->DevicePath().c_str()))
        {
            auto pClone = pRule->Clone(true);
            pClone->SetAccessPath(RulePath.substr(pMount->DevicePath().length() + 1));
            RuleList.Append(pClone->ToVariant(Opts));
        }
    }

    pMount->m_Data[API_S_ACCESS_RULES] = RuleList;

    CBuffer Buffer;
    pMount->m_Data.ToPacket(&Buffer);
    return NtIo_WriteFile(pMount->DevicePath() + DEF_MP_SYS_FILE, &Buffer);
}