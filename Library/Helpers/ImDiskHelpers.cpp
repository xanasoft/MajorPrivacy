/*
 * Copyright 2023-2026 David Xanatos, xanasoft.com
 *
 */

#include "pch.h"
#include "ImDiskHelpers.h"
#include "../Common/Strings.h"
#include "AppUtil.h"
#include "../../ImBox/ImBox.h"

#define FILE_SHARE_VALID_FLAGS          0x00000007

bool IsImDiskDriverReady()
{
    UNICODE_STRING objname;
    RtlInitUnicodeString(&objname, IMDISK_CTL_DEVICE_NAME);

    HANDLE Device = ImDiskOpenDeviceByName(IMDISK_CTL_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE);
    if (Device == INVALID_HANDLE_VALUE)
        return false;

    DWORD VersionCheck;
    DWORD BytesReturned;
    bool bRet = false;
    if (DeviceIoControl(Device, IOCTL_IMDISK_QUERY_VERSION, NULL, 0, &VersionCheck, sizeof(VersionCheck), &BytesReturned, NULL))
    {
        if (BytesReturned >= sizeof(VersionCheck))
            bRet = (VersionCheck == IMDISK_DRIVER_VERSION);
    }

    CloseHandle(Device);
    return bRet;
}

bool ImDiskGetDeviceList(std::vector<ULONG>& DeviceList)
{
    HANDLE driver = ImDiskOpenDeviceByName(IMDISK_CTL_DEVICE_NAME, GENERIC_READ);
    if (driver == INVALID_HANDLE_VALUE)
        return false;

    DeviceList.resize(3);

retry:
    DWORD dw;
    if (!DeviceIoControl(driver, IOCTL_IMDISK_QUERY_DRIVER, NULL, 0,
        DeviceList.data(), (ULONG)(DeviceList.size() * sizeof(ULONG)), &dw, NULL))
    {
        DWORD dwLastError = GetLastError();
        if (dwLastError == ERROR_MORE_DATA && DeviceList[0] > 0) {
            DeviceList.resize(DeviceList[0] + 1);
            goto retry;
        }
        CloseHandle(driver);
        SetLastError(dwLastError);
        return false;
    }

    if ((dw == sizeof(ULONG)) && (DeviceList[0] > 0)) {
        DeviceList.resize(DeviceList[0] + 1);
        goto retry;
    }

    CloseHandle(driver);

    // Resize to actual count (first element is count, rest are device numbers)
    if (DeviceList[0] > 0)
        DeviceList.resize(DeviceList[0] + 1);
    else
        DeviceList.resize(1);

    return true;
}

WCHAR ImDiskFindFreeDriveLetter()
{
    DWORD logical_drives = GetLogicalDrives();

    for (WCHAR search = L'Z'; search >= L'I'; search--)
    {
        if ((logical_drives & (1 << (search - L'A'))) == 0)
            return search;
    }

    return 0;
}

HANDLE ImDiskOpenDeviceByName(const std::wstring& DevicePath, DWORD AccessMode)
{
    UNICODE_STRING objname;
    RtlInitUnicodeString(&objname, DevicePath.c_str());

    OBJECT_ATTRIBUTES object_attrib;
    InitializeObjectAttributes(&object_attrib, &objname, OBJ_CASE_INSENSITIVE, NULL, NULL);

    IO_STATUS_BLOCK io_status;
    HANDLE handle;

    NTSTATUS status = NtOpenFile(&handle,
        SYNCHRONIZE | AccessMode,
        &object_attrib,
        &io_status,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT);

    if (!NT_SUCCESS(status))
    {
        SetLastError(RtlNtStatusToDosError(status));
        return INVALID_HANDLE_VALUE;
    }

    return handle;
}

HANDLE ImDiskOpenDeviceByNumber(ULONG DeviceNumber, DWORD AccessMode)
{
    std::wstring devicePath = IMDISK_DEVICE_BASE_NAME + std::to_wstring(DeviceNumber);
    return ImDiskOpenDeviceByName(devicePath, AccessMode);
}

HANDLE ImDiskOpenDeviceByMountPoint(const std::wstring& MountPoint, DWORD AccessMode)
{
    std::wstring DeviceName;
    PREPARSE_DATA_BUFFER ReparseData = NULL;

    if ((MountPoint.length() >= 2) && (MountPoint[1] == L':') &&
        (MountPoint.length() <= 3))
    {
        // Drive letter format: "X:" or "X:\"
        DeviceName = L"\\DosDevices\\" + std::wstring(1, MountPoint[0]) + L":";
    }
    else if (((MountPoint.compare(0, 4, L"\\\\?\\") == 0) ||
              (MountPoint.compare(0, 4, L"\\\\.\\") == 0)) &&
             (MountPoint.find(L'\\', 4) == std::wstring::npos))
    {
        return CreateFileW(MountPoint.c_str(), AccessMode,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    }
    else
    {
        // Junction point
        HANDLE hDir = CreateFileW(MountPoint.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

        if (hDir == INVALID_HANDLE_VALUE)
            return INVALID_HANDLE_VALUE;

        DWORD buffer_size = FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer) +
            MAXIMUM_REPARSE_DATA_BUFFER_SIZE;

        ReparseData = (PREPARSE_DATA_BUFFER)HeapAlloc(GetProcessHeap(),
            HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, buffer_size);

        DWORD dw;
        if (!DeviceIoControl(hDir, FSCTL_GET_REPARSE_POINT, NULL, 0,
            ReparseData, buffer_size, &dw, NULL))
        {
            DWORD last_error = GetLastError();
            CloseHandle(hDir);
            HeapFree(GetProcessHeap(), 0, ReparseData);
            SetLastError(last_error);
            return INVALID_HANDLE_VALUE;
        }

        CloseHandle(hDir);

        if (ReparseData->ReparseTag != IO_REPARSE_TAG_MOUNT_POINT)
        {
            HeapFree(GetProcessHeap(), 0, ReparseData);
            SetLastError(ERROR_NOT_A_REPARSE_POINT);
            return INVALID_HANDLE_VALUE;
        }

        USHORT nameLen = ReparseData->MountPointReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
        WCHAR* namePtr = (WCHAR*)((PUCHAR)ReparseData->MountPointReparseBuffer.PathBuffer +
            ReparseData->MountPointReparseBuffer.SubstituteNameOffset);

        DeviceName = std::wstring(namePtr, nameLen);
    }

    // Remove trailing backslash if present
    if (!DeviceName.empty() && DeviceName.back() == L'\\')
        DeviceName.pop_back();

    HANDLE h = ImDiskOpenDeviceByName(DeviceName, AccessMode);

    if (ReparseData != NULL)
        HeapFree(GetProcessHeap(), 0, ReparseData);

    return h;
}

bool ImDiskQueryDevice(HANDLE Device, IMDISK_CREATE_DATA* CreateData, ULONG CreateDataSize)
{
    DWORD dw;
    if (!DeviceIoControl(Device, IOCTL_IMDISK_QUERY_DEVICE, NULL, 0,
        CreateData, CreateDataSize, &dw, NULL))
        return false;

    return dw >= (sizeof(IMDISK_CREATE_DATA) - sizeof(CreateData->FileName));
}

std::wstring ImDiskQueryDeviceProxy(const std::wstring& DevicePath)
{
    std::wstring proxy;

    HANDLE device = ImDiskOpenDeviceByName(DevicePath, FILE_READ_ATTRIBUTES);
    if (device != INVALID_HANDLE_VALUE)
    {
        union {
            IMDISK_CREATE_DATA create_data;
            BYTE buffer[sizeof(IMDISK_CREATE_DATA) + MAX_PATH * 4];
        } u;

        if (ImDiskQueryDevice(device, &u.create_data, sizeof(u.buffer)))
        {
            //if (IMDISK_TYPE(u.create_data.Flags) == IMDISK_TYPE_PROXY)
            proxy = std::wstring(u.create_data.FileName, u.create_data.FileNameLength / sizeof(wchar_t));
        }

        CloseHandle(device);
    }

    return proxy;
}

ULONGLONG ImDiskQueryDeviceSize(const std::wstring& DevicePath)
{
    ULONGLONG size = 0;

    HANDLE device = ImDiskOpenDeviceByName(DevicePath, FILE_READ_ATTRIBUTES);
    if (device != INVALID_HANDLE_VALUE)
    {
        union {
            IMDISK_CREATE_DATA create_data;
            BYTE buffer[sizeof(IMDISK_CREATE_DATA) + MAX_PATH * 4];
        } u;

        if (ImDiskQueryDevice(device, &u.create_data, sizeof(u.buffer)))
        {
            //if (IMDISK_TYPE(u.create_data.Flags) == IMDISK_TYPE_PROXY)
            size = u.create_data.DiskGeometry.Cylinders.QuadPart;
        }

        CloseHandle(device);
    }

    return size;
}

WCHAR ImDiskQueryDriveLetter(const std::wstring& DevicePath)
{
    WCHAR Letter = 0;

    HANDLE device = ImDiskOpenDeviceByName(DevicePath, FILE_READ_ATTRIBUTES);
    if (device != INVALID_HANDLE_VALUE)
    {
        union {
            IMDISK_CREATE_DATA create_data;
            BYTE buffer[sizeof(IMDISK_CREATE_DATA) + MAX_PATH * 4];
        } u;

        if (ImDiskQueryDevice(device, &u.create_data, sizeof(u.buffer)))
        {
            //if (IMDISK_TYPE(u.create_data.Flags) == IMDISK_TYPE_PROXY)
            Letter = u.create_data.DriveLetter;
        }

        CloseHandle(device);
    }

    return Letter;
}

bool ImDiskRemoveDevice(ULONG DeviceNumber, bool ForceDismount)
{
    // Try opening device with decreasing access rights
    HANDLE device = ImDiskOpenDeviceByNumber(DeviceNumber, GENERIC_READ | GENERIC_WRITE);
    if (device == INVALID_HANDLE_VALUE)
        device = ImDiskOpenDeviceByNumber(DeviceNumber, GENERIC_READ);
    if (device == INVALID_HANDLE_VALUE)
        device = ImDiskOpenDeviceByNumber(DeviceNumber, FILE_READ_ATTRIBUTES);
    if (device == INVALID_HANDLE_VALUE)
        return false;

    DWORD dw;
    bool success = true;

    // Flush file buffers
    FlushFileBuffers(device);

    // Try to lock volume
    if (!DeviceIoControl(device, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dw, NULL))
    {
        if (ForceDismount)
        {
            // Force dismount and try locking again
            DeviceIoControl(device, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dw, NULL);
            DeviceIoControl(device, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dw, NULL);
        }
        else
        {
            CloseHandle(device);
            return false;
        }
    }

    // Dismount filesystem
    DeviceIoControl(device, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dw, NULL);

    // Eject media
    if (!DeviceIoControl(device, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &dw, NULL))
    {
        if (ForceDismount)
            success = ImDiskForceRemoveDevice(device);
        else
            success = false;
    }

    // Unlock volume
    DeviceIoControl(device, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dw, NULL);

    CloseHandle(device);

    return success;
}

bool ImDiskForceRemoveDevice(ULONG DeviceNumber)
{
    HANDLE driver = ImDiskOpenDeviceByName(IMDISK_CTL_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE);
    if (driver == INVALID_HANDLE_VALUE)
        return false;

    DWORD dw;
    bool result = DeviceIoControl(driver, IOCTL_IMDISK_REMOVE_DEVICE,
        &DeviceNumber, sizeof(DeviceNumber), NULL, 0, &dw, NULL) != FALSE;

    CloseHandle(driver);
    return result;
}

bool ImDiskForceRemoveDevice(HANDLE Device)
{
    // Query device number from handle
    union {
        IMDISK_CREATE_DATA create_data;
        BYTE buffer[sizeof(IMDISK_CREATE_DATA) + MAX_PATH * 4];
    } u;

    if (!ImDiskQueryDevice(Device, &u.create_data, sizeof(u.buffer)))
        return false;

    return ImDiskForceRemoveDevice(u.create_data.DeviceNumber);
}

bool ImDiskExtendDevice(ULONG DeviceNumber, ULONGLONG ExtendSize)
{
    HANDLE device = ImDiskOpenDeviceByNumber(DeviceNumber, GENERIC_READ | GENERIC_WRITE);
    if (device == INVALID_HANDLE_VALUE)
        device = ImDiskOpenDeviceByNumber(DeviceNumber, GENERIC_READ);
    if (device == INVALID_HANDLE_VALUE)
        return false;

    DWORD dw;
    DISK_GROW_PARTITION grow_partition = { 0 };
    GET_LENGTH_INFORMATION length_information = { 0 };
    DISK_GEOMETRY disk_geometry = { 0 };

    // Extend disk size
    grow_partition.PartitionNumber = 1;
    grow_partition.BytesToGrow.QuadPart = ExtendSize;
    if (!DeviceIoControl(device, IOCTL_DISK_GROW_PARTITION,
        &grow_partition, sizeof(grow_partition), NULL, 0, &dw, NULL))
    {
        CloseHandle(device);
        return false;
    }

    // Get new length info
    if (!DeviceIoControl(device, IOCTL_DISK_GET_LENGTH_INFO,
        NULL, 0, &length_information, sizeof(length_information), &dw, NULL))
    {
        CloseHandle(device);
        return false;
    }

    // Get disk geometry for sector size
    if (!DeviceIoControl(device, IOCTL_DISK_GET_DRIVE_GEOMETRY,
        NULL, 0, &disk_geometry, sizeof(disk_geometry), &dw, NULL))
    {
        CloseHandle(device);
        return false;
    }

    // Calculate new filesystem size in sectors and extend filesystem
    LONGLONG new_filesystem_size = length_information.Length.QuadPart / disk_geometry.BytesPerSector;
    DeviceIoControl(device, FSCTL_EXTEND_VOLUME,
        &new_filesystem_size, sizeof(new_filesystem_size), NULL, 0, &dw, NULL);
    // Note: FSCTL_EXTEND_VOLUME may fail if filesystem doesn't support online extension,
    // but disk extension was still successful

    CloseHandle(device);
    return true;
}

#define VM_LOCK_1 0x0001   // This is used, when calling KERNEL32.DLL VirtualLock routine
#define VM_LOCK_2 0x0002   // This require SE_LOCK_MEMORY_NAME privilege

PVOID AllocSecureMemory(HANDLE hProcess, size_t uSize)
{
    NTSTATUS status;

    PVOID pMem = NULL;

    if(!NT_SUCCESS(status = NtAllocateVirtualMemory(hProcess, &pMem, 0, &uSize, MEM_COMMIT, PAGE_READWRITE)))
        return NULL;
    if(!NT_SUCCESS(status = NtLockVirtualMemory(hProcess, &pMem, &uSize, VM_LOCK_1)))
        return NULL;

    return pMem;
}

NTSTATUS UpdateCommandLine(HANDLE hProcess, NTSTATUS(*Update)(std::wstring& s, PVOID p), PVOID param)
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

NTSTATUS CreateSecureSection(HANDLE* phSection, PVOID* ppMapping, SIZE_T uSize)
{
    NTSTATUS status;
    PACL pDacl = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    OBJECT_ATTRIBUTES objAttr;
    LARGE_INTEGER liSize;
    SIZE_T viewSize = 0;

    //
    // Build a security descriptor with an empty DACL (deny all access).
    // Since the section is unnamed and access is via inherited handles only,
    // an empty DACL prevents anyone from opening or duplicating the section.
    //

    pDacl = (PACL)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ACL));
    if (!pDacl) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    if (!InitializeAcl(pDacl, sizeof(ACL), ACL_REVISION)) {
        status = STATUS_ACCESS_DENIED;
        goto cleanup;
    }

    pSD = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, SECURITY_DESCRIPTOR_MIN_LENGTH);
    if (!pSD) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION)) {
        status = STATUS_ACCESS_DENIED;
        goto cleanup;
    }

    // Set empty DACL - denies all access to anyone trying to open the section
    if (!SetSecurityDescriptorDacl(pSD, TRUE, pDacl, FALSE)) {
        status = STATUS_ACCESS_DENIED;
        goto cleanup;
    }

    // Create without OBJ_INHERIT - we'll make it inheritable temporarily during CreateProcess
    InitializeObjectAttributes(&objAttr, NULL, 0, NULL, pSD);

    liSize.QuadPart = uSize;

    //
    // Create an unnamed section with SEC_NO_CHANGE to prevent
    // changing the protection after mapping.
    //
    status = NtCreateSection(phSection, SECTION_ALL_ACCESS, &objAttr, &liSize, PAGE_READWRITE, SEC_COMMIT | SEC_NO_CHANGE, NULL);

    if (!NT_SUCCESS(status))
        goto cleanup;

    //
    // Map the section into the current process for writing
    //
    *ppMapping = NULL;
    viewSize = uSize;
    status = NtMapViewOfSection(*phSection, GetCurrentProcess(), ppMapping, 0, 0, NULL, &viewSize, ViewUnmap, 0, PAGE_READWRITE);

    if (!NT_SUCCESS(status)) {
        NtClose(*phSection);
        *phSection = NULL;
    }
    else {
        //
        // Lock the mapped view in physical memory to prevent paging to disk.
        // This protects sensitive data (passwords) from being written to pagefile.
        //
        PVOID lockAddr = *ppMapping;
        SIZE_T lockSize = uSize;

        NtLockVirtualMemory(GetCurrentProcess(), &lockAddr, &lockSize, VM_LOCK_1);
        // Note: failure is non-fatal - may fail due to working set quota limits
    }

cleanup:
    if (pSD) HeapFree(GetProcessHeap(), 0, pSD);
    if (pDacl) HeapFree(GetProcessHeap(), 0, pDacl);

    return status;
}

/* ---------------------------------------------------------------------------
 * CImBoxSession - see ImDiskHelpers.h
 * ------------------------------------------------------------------------ */

CImBoxSession::CImBoxSession()
{
}

CImBoxSession::~CImBoxSession()
{
	if (m_pMapping) {
		//
		// The section has held a password, and for the header editor a volume's
		// master key. Wiping before unmapping is the whole reason this class
		// owns the lifetime rather than leaving it to a caller.
		//
		SecureZeroMemory(m_pMapping, m_uSize);
		NtUnmapViewOfSection(GetCurrentProcess(), m_pMapping);
	}
	if (m_hSection)
		NtClose(m_hSection);
}

NTSTATUS CImBoxSession::Create(ULONG uHeaderLen)
{
	if (m_pSection)
		return STATUS_INVALID_DEVICE_STATE;

	ULONG uSize = uHeaderLen ? (ULONG)SECTION_SIZE_FOR_HEADER(uHeaderLen) : (ULONG)sizeof(SSection);

	NTSTATUS status = CreateSecureSection(&m_hSection, &m_pMapping, uSize);
	if (!NT_SUCCESS(status))
		return status;

	m_uSize = uSize;
	m_pSection = (SSection*)m_pMapping;
	memset(m_pSection, 0, uSize);
	return STATUS_SUCCESS;
}

SHeaderData* CImBoxSession::Header() const
{
	if (!m_pSection || m_uSize < sizeof(SSection) + sizeof(SHeaderData))
		return NULL;
	return SECTION_HEADER_DATA(m_pSection);
}

void CImBoxSession::SetPassword(const SPassword* pPassword)
{
	if (!m_pSection)
		return;

	//
	// in.pw and out.info are the same kilobyte, so a previous run's results go
	// with it. That is deliberate: nothing from an earlier run should survive
	// into the next one by accident.
	//
	memset(&m_pSection->in, 0, sizeof(m_pSection->in));
	if (pPassword)
		memcpy(&m_pSection->in.pw, pPassword, sizeof(SPassword));
}

void CImBoxSession::SetPassword(const wchar_t* pPassword, size_t uSize, int iKdf, int iSlot)
{
	if (!m_pSection)
		return;

	memset(&m_pSection->in, 0, sizeof(m_pSection->in));

	if (uSize > sizeof(m_pSection->in.pw.pass))
		uSize = sizeof(m_pSection->in.pw.pass);
	if (pPassword && uSize)
		memcpy(m_pSection->in.pw.pass, pPassword, uSize);

	m_pSection->in.pw.size = (int)uSize;
	m_pSection->in.pw.kdf = iKdf;
	m_pSection->in.pw.slot = iSlot;
}

void CImBoxSession::SetRawKey(const void* pKey, size_t uSize)
{
	if (!m_pSection)
		return;

	memset(&m_pSection->in, 0, sizeof(m_pSection->in));

	if (uSize > sizeof(m_pSection->in.pw.pass))
		uSize = sizeof(m_pSection->in.pw.pass);
	if (pKey && uSize)
		memcpy(m_pSection->in.pw.pass, pKey, uSize);

	//
	// The size is the full key length whatever was copied: that is what tells
	// the derivation this is a raw key and not a short password.
	//
	m_pSection->in.pw.size = SPASS_RAW_KEY_SIZE;
	m_pSection->in.pw.kdf = SPASS_KDF_RAW;
	m_pSection->in.pw.slot = SPASS_SLOT_HEADER;
}

NTSTATUS CImBoxSession::Run(const std::wstring& ImageFile, const std::wstring& Command)
{
	if (!m_pSection)
		return STATUS_INVALID_DEVICE_STATE;

	NTSTATUS status = STATUS_UNSUCCESSFUL;

	//
	// Build the command line. The section travels as an inherited handle value
	// and its length alongside it, because a mapped view cannot be asked how
	// long it is.
	//
	WCHAR sSection[64];
	wsprintf(sSection, L" section=0x%p sec_size=0x%X", m_hSection, m_uSize);

	std::wstring cmd = L"ImBox \"image=" + ImageFile + L"\" " + Command + sSection;

	std::wstring app = GetApplicationDirectory() + L"\\ImBox.exe";
	PROCESS_INFORMATION pi = { 0 };

	//
	// STARTUPINFOEX with PROC_THREAD_ATTRIBUTE_HANDLE_LIST, so that the child
	// inherits this one handle and nothing else that happens to be inheritable.
	//
	SIZE_T attrListSize = 0;
	InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);

	LPPROC_THREAD_ATTRIBUTE_LIST pAttrList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrListSize);
	if (!pAttrList)
		return STATUS_INSUFFICIENT_RESOURCES;

	STARTUPINFOEXW siex = { 0 };
	siex.StartupInfo.cb = sizeof(STARTUPINFOEXW);
	siex.lpAttributeList = pAttrList;

	BOOL bAttrOk = FALSE;
	if (InitializeProcThreadAttributeList(pAttrList, 1, 0, &attrListSize)) {
		SetHandleInformation(m_hSection, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
		if (UpdateProcThreadAttribute(pAttrList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &m_hSection, sizeof(HANDLE), NULL, NULL))
			bAttrOk = TRUE;
	}

	BOOL bCreated = bAttrOk && CreateProcessW(app.c_str(), (WCHAR*)cmd.c_str(), NULL, NULL, TRUE,
		CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &siex.StartupInfo, &pi);

	// inheritable only for the length of that call
	SetHandleInformation(m_hSection, HANDLE_FLAG_INHERIT, 0);

	if (bCreated)
	{
		ResumeThread(pi.hThread);

		HANDLE hEvents[] = { pi.hProcess };
		WaitForMultipleObjects(ARRAYSIZE(hEvents), hEvents, FALSE, 15 * 60 * 1000); // 15 minutes

		DWORD ret;
		GetExitCodeProcess(pi.hProcess, &ret);
		if (ret == STILL_ACTIVE)
			status = STATUS_TIMEOUT;
		else if (ret != ERR_OK)
			status = STATUS_UNSUCCESSFUL;
		else
			status = STATUS_SUCCESS;

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	if (bAttrOk)
		DeleteProcThreadAttributeList(pAttrList);
	HeapFree(GetProcessHeap(), 0, pAttrList);

	return status;
}

/* ------------------------------------------------------------------------ */

NTSTATUS ExecImDisk(const std::wstring& ImageFile, const wchar_t* pPassword, int iKdf, const std::wstring& Command, bool bWrite, CBuffer* pBuffer, USHORT uId)
{
	CImBoxSession Session;

	NTSTATUS status = Session.Create();
	if (!NT_SUCCESS(status))
		return status;

	SSection* pSection = Session.Section();

	pSection->in.pw.size = (int)(wcslen(pPassword) * sizeof(wchar_t));
	memcpy(pSection->in.pw.pass, pPassword, pSection->in.pw.size);
	pSection->in.pw.kdf = iKdf;
	// try the header password first, then every key slot
	pSection->in.pw.slot = SPASS_SLOT_ANY;

	if (pBuffer && bWrite)
	{
		pSection->id = uId;
		pSection->size = (USHORT)pBuffer->GetSize();
		memcpy(pSection->data, pBuffer->GetBuffer(), pSection->size);
	}

	return Session.Run(ImageFile, Command);
}
