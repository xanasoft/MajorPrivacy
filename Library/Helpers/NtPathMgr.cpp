#include "pch.h"
#include "NtPathMgr.h"
#include <ph.h>
#include "NtObj.h"
#include "Scoped.h"
#include <dbt.h>


// DOS Prefixes
// L"\\\\?\\" prefix is used to bypass the MAX_PATH limitation
// L"\\\\.\\" points to L"\Device\"

// NT Prefixes
// L"\\??\\" prefix (L"\\GLOBAL??\\") maps DOS letters and more

static const WCHAR* CNtPathMgr__NamedPipe = L"\\device\\namedpipe\\";
//static const WCHAR* CNtPathMgr__MailSlot = L"\\device\\mailslot\\";

static const WCHAR* CNtPathMgr__MupRedir = L"\\device\\mup\\;lanmanredirector\\";
static const ULONG CNtPathMgr__MupRedirLen = 30;

static const WCHAR* CNtPathMgr__Mup = L"\\device\\mup\\";
static const ULONG CNtPathMgr__MupLen = 12;

static const WCHAR* CNtPathMgr__Device = L"\\device\\";
static const ULONG CNtPathMgr__DeviceLen = 8;

static const WCHAR* CNtPathMgr__BQQB = L"\\??\\";
static const ULONG CNtPathMgr__BQQBLen = 4;

static const WCHAR* CNtPathMgr__GuidPrefix = L"\\??\\Volume{";
static const ULONG CNtPathMgr__GuidPrefixLen = 11;

static const WCHAR* CNtPathMgr__BBQB = L"\\\\?\\";
static const ULONG CNtPathMgr__BBQBLen = 4;

static const WCHAR* CNtPathMgr__BBPB = L"\\\\.\\";
static const ULONG CNtPathMgr__BBPBLen = 4;


#define FILE_SHARE_VALID_FLAGS          0x00000007

#define MAX_DRIVES 26


CNtPathMgr* CNtPathMgr::m_pInstance = NULL;


CNtPathMgr::CNtPathMgr()
{
    m_Drives.resize(MAX_DRIVES);

    InitDosDrives();
    InitNtMountMgr();

    RegisterForDeviceNotifications();
}

CNtPathMgr::~CNtPathMgr()
{
    UnRegisterForDeviceNotifications();
}

CNtPathMgr* CNtPathMgr::Instance()
{
    // todo single init
    if (!m_pInstance)
        m_pInstance = new CNtPathMgr;
    return m_pInstance;
}

void CNtPathMgr::Dispose()
{
    if (m_pInstance)
    {
        delete m_pInstance;
        m_pInstance = NULL;
    }
}

std::wstring CNtPathMgr::TranslateDosToNtPath(std::wstring DosPath)
{
    //
    // Handle paths prefixes first
    //

    if (_wcsnicmp(DosPath.c_str(), CNtPathMgr__BBQB, CNtPathMgr__BBQBLen) == 0) // \\?\X:\...
        DosPath = DosPath.substr(4);

    if (_wcsnicmp(DosPath.c_str(), CNtPathMgr__BBPB, CNtPathMgr__BBPBLen) == 0) // \\.\HarddiskVolumeN\...
        return std::wstring(CNtPathMgr__Device) + DosPath.substr(4);

    //
    // Network path L"\\..."
    //

    if (DosPath.size() >= 2 && DosPath[0] == L'\\' && DosPath[1] == L'\\')
    {
        return CNtPathMgr__Mup + DosPath.substr(2);
    }

    //
	// Dos Path L"X:\..."
    //

    if (!(DosPath.size() >= 3 && DosPath[1] == L':' && (DosPath[2] == L'\\' || DosPath[2] == L'\0')))
		return L"";

    //
    // Mount Point path L"X:\MountPoint\..."
    //

	std::shared_ptr<const SMount> Mount = GetMountForPath(DosPath);
	if (Mount)
		return Mount->DevicePath + DosPath.substr(Mount->MountPoint.length());

    //
    // Drive Letter path L"X:\..."
    //

    std::shared_ptr<const SDrive> Drive = GetDriveForLetter(DosPath[0]);
    if (Drive)
        return Drive->DevicePath + DosPath.substr(2);

    //
    // Return empty string on failure
    //

    return L"";
}

std::wstring CNtPathMgr::TranslateNtToDosPath(std::wstring NtPath)
{
    // 
    // Handle Guid paths first
    // 

	if (NtPath.length() >= 48 && _wcsnicmp(NtPath.c_str(), CNtPathMgr__GuidPrefix, CNtPathMgr__GuidPrefixLen) == 0) // \??\Volume{...}\...
    {
        std::shared_ptr<const SGuid> guid = GetDeviceForGuid(NtPath.substr(10, 38));
        if (guid)
            NtPath = guid->DevicePath + NtPath.substr(48);
    }

    // 
    // Sometimes we get a DOS path with the \??\ prefix
    // in such cases we just quickly strip it and are done
    // 

	if (_wcsnicmp(NtPath.c_str(), CNtPathMgr__BQQB, CNtPathMgr__BQQBLen) == 0) // \??\X:\...
        return NtPath.substr(4);

    //
    // Network path L"\device\mup\..."
    //

    if (_wcsnicmp(NtPath.c_str(), CNtPathMgr__Mup, CNtPathMgr__MupLen) == 0)
    {
        return L"\\\\" + NtPath.substr(CNtPathMgr__MupLen);
    }

    //
    // Find Drive Letter
    //

    ULONG PrefixLen;
    std::shared_ptr<const SDrive> Drive = GetDriveForPath(NtPath);
    if (Drive)
        PrefixLen = (ULONG)Drive->DevicePath.length();
    else
        Drive = GetDriveForUncPath(NtPath, &PrefixLen);
    if (Drive)
        return std::wstring(&Drive->DriveLetter, 1) + L":" + NtPath.substr(PrefixLen);

    //
    // Find Mount Point
    //

    std::shared_ptr<const SMount> Mount = GetMountForDevice(NtPath);
    if (Mount)
        return Mount->MountPoint + NtPath.substr(Mount->DevicePath.length());

    //
    // Return empty string on failure
    //

    return L"";
}

void CNtPathMgr::InitDosDrives()
{
    NTSTATUS status;

    ULONG DriveMask = -1;

    for (ULONG DriveIndex = 0; DriveIndex < MAX_DRIVES; DriveIndex++)
    {
        std::shared_ptr<SDrive> OldDrive;

        //
        // process this drive if (1) it was specified in the mask,
        // or (2) if it is currently an incomplete drive in the form
        // \??\X:\ and drive X was specified in the mask
        //

        if (!(DriveMask & (1 << DriveIndex))) {

            std::shared_ptr<SDrive> Drive = m_Drives[DriveIndex];
            if (Drive) {
                std::wstring path = Drive->DevicePath;
                if (wmemcmp(path.c_str(), CNtPathMgr__BQQB, CNtPathMgr__BQQBLen) == 0 && path[4] && path[5] == L':' && path[6] == L'\\') {

                    WCHAR OtherIndex = path[4];
                    if (OtherIndex >= L'A' && OtherIndex <= L'Z')
                        OtherIndex = OtherIndex - L'A' + L'a';
                    if (OtherIndex >= L'a' && OtherIndex <= L'z') 
                    {
                        OtherIndex -= L'a';
                        if (DriveMask & (1 << OtherIndex)) 
                            DriveMask |= (1 << DriveIndex);
                    }
                }
            }

            if (!(DriveMask & (1 << DriveIndex)))
                continue;
        }

        if (m_Drives[DriveIndex]) {

            OldDrive = m_Drives[DriveIndex];
            m_Drives[DriveIndex].reset();

            //RemovePermLinks(OldDrive->path); // todo
        }

        //
        // translate the DosDevices symbolic link "\??\x:" into its
        // device object.  note that sometimes the DosDevices name
        // translate to symbolic link itself.
        //

        std::wstring DriveStr = CNtPathMgr__BQQB;
        DriveStr += (WCHAR)(DriveIndex + L'A');
        DriveStr += L':';

        CScopedHandle handle = CScopedHandle((HANDLE)0, NtClose);
        status = NtOpenSymbolicLinkObject(&handle, SYMBOLIC_LINK_QUERY, SNtObject(DriveStr).Get());

        // if (!NT_SUCCESS(status)) // todo on access denided

        //
        // errors indicate the drive does not exist, is no longer valid
        // or was never valid.  we can also get access denied, if the
        // drive appears as ClosedFilePath.  in any case, silently ignore
        //

        if (!NT_SUCCESS(status)) 
        {
            if (status != STATUS_OBJECT_NAME_NOT_FOUND && status != STATUS_OBJECT_PATH_NOT_FOUND && status != STATUS_OBJECT_TYPE_MISMATCH && status != STATUS_ACCESS_DENIED)
				DbgPrint("NtOpenSymbolicLinkObject(%S) failed: %08X\r\n", DriveStr.c_str(), status);

            if (OldDrive)
                AdjustDrives(DriveIndex, OldDrive->Subst, OldDrive->DevicePath);

            continue;
        }

        //
        // get the target of the symbolic link
        //

        if (handle) 
        {
            ULONG PathLen;
            status = NtQuerySymbolicLinkObject(handle, SNtString(0).Get(), &PathLen);
            if (status == STATUS_BUFFER_TOO_SMALL) 
            {
                SNtString objname((PathLen + 32) / sizeof(WCHAR));

                status = NtQuerySymbolicLinkObject(handle, objname.Get(), NULL);
                if (NT_SUCCESS(status))
                {
                    std::wstring Path = objname.Value();

                    //
                    // add a new drive entry
                    //

                    if (!Path.empty())
                    {
                        // SUBST drives translate to \??\X:...
                        bool subst = (wmemcmp(Path.c_str(), CNtPathMgr__BQQB, CNtPathMgr__BQQBLen) == 0 && Path[4] && Path[5] == L':');

                        bool translated = false;
                        Path = GetName_TranslateSymlinks(Path, &translated);

                        AddDrive((WCHAR)(L'A' + DriveIndex), Path, subst);
                    }
                }
            }
            else if (NT_SUCCESS(status))
                status = STATUS_UNSUCCESSFUL;
        }

        if (!NT_SUCCESS(status)) {

			DbgPrint("NtQuerySymbolicLinkObject(%S) failed: %08X\r\n", DriveStr.c_str(), status);
        }
    }
}

void CNtPathMgr::InitNtMountMgr()
{
    m_Mounts.clear();
    m_Guids.clear();

    NTSTATUS status;
    IO_STATUS_BLOCK IoStatusBlock;

    CScopedHandle hMountMgr = CScopedHandle((HANDLE)0, NtClose);
    status = NtCreateFile(&hMountMgr, FILE_GENERIC_EXECUTE, SNtObject(L"\\Device\\MountPointManager").Get(), &IoStatusBlock,
        NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_VALID_FLAGS, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, NULL, 0);
    if (!NT_SUCCESS(status))
        return;

    std::vector<BYTE> Buffer1(0x2000);
    std::vector<BYTE> Buffer2(0x4000);
    
    MOUNTMGR_MOUNT_POINT Input1 = { 0 };

    status = NtDeviceIoControlFile(hMountMgr, NULL, NULL, NULL, &IoStatusBlock, IOCTL_MOUNTMGR_QUERY_POINTS,
        &Input1, sizeof(Input1), Buffer1.data(), (ULONG)Buffer1.size());
    if (!NT_SUCCESS(status))
        return;

    MOUNTMGR_MOUNT_POINTS* MountPoints = (MOUNTMGR_MOUNT_POINTS*)Buffer1.data();

    for (ULONG i = 0; i < MountPoints->NumberOfMountPoints; i++) {

        MOUNTMGR_MOUNT_POINT* MountPoint = &MountPoints->MountPoints[i];
        
        WCHAR* DeviceName = (WCHAR*)((UCHAR*)MountPoints + MountPoint->DeviceNameOffset);
        ULONG DeviceNameLen = MountPoint->DeviceNameLength / sizeof(WCHAR);

        WCHAR* VolumeName = (WCHAR*)((UCHAR*)MountPoints + MountPoint->SymbolicLinkNameOffset);
        ULONG VolumeNameLen = MountPoint->SymbolicLinkNameLength / sizeof(WCHAR);

        DbgPrint("Found MountPoint: %.*S -> DevicePath: %.*S\r\n", VolumeNameLen, VolumeName, DeviceNameLen, DeviceName);

        if (VolumeNameLen != 48 && VolumeNameLen != 49)
            continue;
        if (_wcsnicmp(VolumeName, CNtPathMgr__GuidPrefix, CNtPathMgr__GuidPrefixLen) != 0)
            continue;
        if (GetDeviceForGuid(std::wstring(&VolumeName[10], 38)))
            continue; // duplicate ???

        //
		// Add guid to nt device association
        //

		AddGuid(&VolumeName[10], std::wstring(DeviceName, DeviceNameLen));

        //
		// Get all the DOS paths where the volume is mounted
        //

        union {
            MOUNTDEV_NAME Input2;
            BYTE Input2_Space[128];
        };

        Input2.NameLength = 48 * sizeof(WCHAR);
        wmemcpy(Input2.Name, VolumeName, 48);
        Input2.Name[48] = L'\0';

        status = NtDeviceIoControlFile(hMountMgr, NULL, NULL, NULL, &IoStatusBlock, IOCTL_MOUNTMGR_QUERY_DOS_VOLUME_PATHS,
            &Input2, sizeof(Input2_Space), Buffer2.data(), (ULONG)Buffer2.size());
        if (!NT_SUCCESS(status))
            continue;

        MOUNTMGR_VOLUME_PATHS* VolumePaths = (MOUNTMGR_VOLUME_PATHS*)Buffer2.data();

        if (VolumePaths->MultiSzLength && *VolumePaths->MultiSz)
        {
            WCHAR* DosPath = VolumePaths->MultiSz;
            
            //
			// Handle the case where the volume is mounted as a drive letter, 
			// skip first entry, drive letters are handled by InitDosDrives
            //

            ULONG DosPathLen = (ULONG)wcslen(DosPath);
            if (DosPathLen <= 3)
                DosPath += DosPathLen + 1;

            //
			// Add mout point nt device association
            //

            while(*DosPath)
            {
                DosPathLen = (ULONG)wcslen(DosPath);
                AddMount(std::wstring(DosPath, DosPathLen), std::wstring(DeviceName, DeviceNameLen));
                DosPath += DosPathLen + 1;
            }
        }
    }
}

ULONG CNtPathMgr::GetVolumeSN(const std::wstring& DriveNtPath)
{
    ULONG sn = 0;
    CScopedHandle handle = CScopedHandle((HANDLE)0, NtClose);
    IO_STATUS_BLOCK iosb;

    ULONG OldMode;
    RtlSetThreadErrorMode(0x10u, &OldMode);
    NTSTATUS status = NtCreateFile(&handle, GENERIC_READ | SYNCHRONIZE, SNtObject(DriveNtPath + L"\\").Get(),
        &iosb, NULL, 0, FILE_SHARE_VALID_FLAGS, FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    RtlSetThreadErrorMode(OldMode, 0i64);

    if (NT_SUCCESS(status))
    {
        union {
            FILE_FS_VOLUME_INFORMATION volumeInfo;
            BYTE volumeInfoBuff[64];
        } u;
        if (NT_SUCCESS(NtQueryVolumeInformationFile(handle, &iosb, &u.volumeInfo, sizeof(u), FileFsVolumeInformation)))
            sn = u.volumeInfo.VolumeSerialNumber;
    }

    return sn;
}

void CNtPathMgr::AddDrive(WCHAR DriveLetter, const std::wstring& DevicePath, bool subst)
{
    std::unique_lock lock(m_Mutex);

    std::shared_ptr<SDrive> Drive = std::make_shared<SDrive>();
    Drive->DriveLetter = DriveLetter;
    //*Drive->SN = 0;
    //if (1) { // todo
    //    ULONG sn = GetVolumeSN(pDrive->DevicePath);
    //    if (sn != 0)
    //        _snwprintf(pDrive->SN, 10, L"%04X-%04X", HIWORD(sn), LOWORD(sn));
    //}
    Drive->DevicePath = DevicePath;
    Drive->Subst = subst;
    DbgPrint("Add Guid: %c: -> DevicePath: %S\r\n", (char)Drive->DriveLetter, Drive->DevicePath.c_str());
    m_Drives[DriveLetter - L'A'] = Drive;
}

void CNtPathMgr::AdjustDrives(ULONG Index, bool Subst, const std::wstring& Path)
{
    std::unique_lock lock(m_Mutex);

    //
    // if the path of any other drives references the path being removed,
    // then convert the path of the other drive to the form \\X:\...
    //
    // if the drive being removed is a SUBST drive, then we only look at
    // other SUBST drives here, to prevent the removal of the SUBST drive
    // from having an effect on drives that are not related to the SUBST
    //

    ULONG PathLen = (ULONG)Path.length();

    for (ULONG DriveIndex = 0; DriveIndex < MAX_DRIVES; DriveIndex++) 
    {
        std::shared_ptr<SDrive> Drive = m_Drives[DriveIndex];
        if (!Drive)
            continue;
        bool OldDriveSubst = Drive->Subst;
        if (Subst && !OldDriveSubst)
            continue;

        if (Drive->DevicePath.length() >= PathLen && (Drive->DevicePath[PathLen] == L'\0' || Drive->DevicePath[PathLen] == L'\\') && _wcsnicmp(Drive->DevicePath.c_str(), Path.c_str(), PathLen) == 0) 
        {
            std::wstring Suffix = Drive->DevicePath.substr(PathLen);

            Drive = std::make_shared<SDrive>();
            Drive->DriveLetter = (wchar_t)(L'A' + DriveIndex);
            Drive->Subst = OldDriveSubst;
            Drive->DevicePath = CNtPathMgr__BQQB;
            Drive->DevicePath += (WCHAR)(Index + L'A');
            Drive->DevicePath += L':';
            Drive->DevicePath += Suffix;

            m_Drives[DriveIndex] = Drive;
        }
    }
}

std::shared_ptr<const CNtPathMgr::SDrive> CNtPathMgr::GetDriveForPath(const std::wstring& Path)
{
    std::shared_ptr<const SDrive> Drive;

    std::unique_lock lock(m_Mutex);

    for (ULONG DriveIndex = 0; DriveIndex < MAX_DRIVES; DriveIndex++) 
    {
        Drive = m_Drives[DriveIndex];
        if (Drive) {

            if (Path.length() >= Drive->DevicePath.length() && _wcsnicmp(Path.c_str(), Drive->DevicePath.c_str(), Drive->DevicePath.length()) == 0) {

                //
                // make sure access to \Device\HarddiskVolume10 (for M:),
                // for instance, is not matched by \Device\HarddiskVolume1
                // (for C:), by requiring a backslash or null character
                // to follow the matching drive path
                //

                const WCHAR* ptr = Path.c_str() + Drive->DevicePath.length();
                if (*ptr == L'\\' || *ptr == L'\0')
                    break;
            }

            Drive.reset();
        }
    }

    return Drive;
}

std::shared_ptr<const CNtPathMgr::SDrive> CNtPathMgr::GetDriveForUncPath(const std::wstring& Path, ULONG* OutPrefixLen)
{
    //
    // given an input path like
    // \Device\LanmanRedirector\server\share\file.txt
    // find the corresponding drive even if it is stored as
    // \Device\LanmanRedirector\;Q:0000000000001234\server\share
    //

    *OutPrefixLen = 0;

    if (Path.length() <= CNtPathMgr__DeviceLen || _wcsnicmp(Path.c_str(), CNtPathMgr__Device, CNtPathMgr__DeviceLen) != 0)
        return NULL;
    WCHAR* ptr1a = (WCHAR*)wcschr(Path.c_str() + CNtPathMgr__DeviceLen, L'\\');
    if(!ptr1a)
        return NULL;
    ULONG pfx_len = (ULONG)(ptr1a - Path.c_str());
    ULONG sfx_len = (ULONG)wcslen(ptr1a);

    std::shared_ptr<SDrive> Drive;

    std::unique_lock lock(m_Mutex);

    for (ULONG DriveIndex = 0; DriveIndex < MAX_DRIVES; DriveIndex++) 
    {
        Drive = m_Drives[DriveIndex];
        if (Drive && Drive->DevicePath.length() > CNtPathMgr__DeviceLen && // make sure it starts with \device
            _wcsnicmp(Drive->DevicePath.c_str(), CNtPathMgr__Device, CNtPathMgr__DeviceLen) == 0) 
        {
            //
            // make sure the device component is the same
            //

            const WCHAR* ptr1b = wcschr(Drive->DevicePath.c_str() + CNtPathMgr__DeviceLen, L'\\');
            if (ptr1b && (ptr1b - Drive->DevicePath.c_str()) == pfx_len && *(ptr1b + 1) == L';' && _wcsnicmp(Path.c_str(), Drive->DevicePath.c_str(), pfx_len) == 0) 
            {
                const WCHAR* ptr2b = wcschr(ptr1b + 1, L'\\');
                if (ptr2b) 
                {
                    ULONG sfx_len_2 = (ULONG)wcslen(ptr2b);
                    if (sfx_len >= sfx_len_2 && _wcsnicmp(ptr1a, ptr2b, sfx_len_2) == 0) 
                    {
                        const WCHAR* ptr2a = ptr1a + sfx_len_2;
                        if (*ptr2a == L'\\' || *ptr2a == L'\0') 
                        {
                            *OutPrefixLen = (ULONG)(ptr2a - Path.c_str());
                            break;
                        }
                    }
                }
            }

            Drive.reset();
        }
    }

    return Drive;
}

std::shared_ptr<const CNtPathMgr::SDrive> CNtPathMgr::GetDriveForLetter(WCHAR DriveLetter)
{
    std::unique_lock lock(m_Mutex);

    if (DriveLetter >= L'A' && DriveLetter <= L'Z')
        return m_Drives[DriveLetter - L'A'];
    if (DriveLetter >= L'a' && DriveLetter <= L'z')
        return m_Drives[DriveLetter - L'a'];
    return NULL;
}

void CNtPathMgr::AddMount(const std::wstring& MountPoint, const std::wstring& DevicePath)
{
    std::unique_lock lock(m_Mutex);

	std::shared_ptr<SMount> Mount = std::make_shared<SMount>();
	Mount->MountPoint = MountPoint;
	Mount->DevicePath = DevicePath;
    DbgPrint("Add MountPoint: %S -> DevicePath: %S\r\n", Mount->MountPoint.c_str(), Mount->DevicePath.c_str());
	m_Mounts.push_back(Mount);
}

std::shared_ptr<const CNtPathMgr::SMount> CNtPathMgr::GetMountForPath(const std::wstring& DosPath)
{
    std::unique_lock lock(m_Mutex);

    std::shared_ptr<SMount> FinalMount;
	for (auto I = m_Mounts.begin(); I != m_Mounts.end(); ++I)
	{
		std::shared_ptr<SMount> Mount = *I;
		if (DosPath.length() >= Mount->MountPoint.length() && _wcsnicmp(DosPath.c_str(), Mount->MountPoint.c_str(), Mount->MountPoint.length()) == 0)
		{
			if (!FinalMount || Mount->MountPoint.length() > FinalMount->MountPoint.length())
                FinalMount = Mount;
		}
	}
    return FinalMount;
}

std::shared_ptr<const CNtPathMgr::SMount> CNtPathMgr::GetMountForDevice(const std::wstring& NtPath)
{
    std::unique_lock lock(m_Mutex);

	for (auto I = m_Mounts.begin(); I != m_Mounts.end(); ++I)
	{
		std::shared_ptr<SMount> Mount = *I;
		if (NtPath.length() >= Mount->DevicePath.length() && _wcsnicmp(NtPath.c_str(), Mount->DevicePath.c_str(), Mount->DevicePath.length()) == 0)
		{
            return Mount;
		}
	}
	return NULL;
}

void CNtPathMgr::AddGuid(const wchar_t* sGuid, const std::wstring& DevicePath)
{
    std::unique_lock lock(m_Mutex);

    std::shared_ptr<SGuid> Guid = std::make_shared<SGuid>();
    wmemcpy(Guid->Guid, sGuid, 38);
    Guid->Guid[38] = 0;
    Guid->DevicePath = DevicePath;
    DbgPrint("Add Guid: %S -> DevicePath: %S\r\n", Guid->Guid, DevicePath.c_str());
    m_Guids.push_back(Guid);
}

std::shared_ptr<const CNtPathMgr::SGuid> CNtPathMgr::GetGuidOfDevice(const std::wstring& DevicePath)
{
    std::unique_lock lock(m_Mutex);

    for (auto I = m_Guids.begin(); I != m_Guids.end(); ++I)
    {
        std::shared_ptr<SGuid> Guid = *I;

        if (DevicePath.length() >= Guid->DevicePath.length() && _wcsnicmp(DevicePath.c_str(), Guid->DevicePath.c_str(), Guid->DevicePath.length()) == 0) {

            const WCHAR* ptr = DevicePath.c_str() + Guid->DevicePath.length();
            if (*ptr == L'\\' || *ptr == L'\0')
                return Guid;
        }
    }

    return NULL;
}


std::shared_ptr<const CNtPathMgr::SGuid> CNtPathMgr::GetDeviceForGuid(const std::wstring& sGuid)
{
    std::unique_lock lock(m_Mutex);

    for (auto I = m_Guids.begin(); I != m_Guids.end(); ++I)
    {
        std::shared_ptr<SGuid> Guid = *I;

        if (_wcsnicmp(Guid->Guid, sGuid.c_str(), 38) == 0)
            return Guid;
    }

    return NULL;
}

std::wstring CNtPathMgr::GetName_TranslateSymlinks(const std::wstring& Path, bool* translated)
{
    NTSTATUS status;

    //
    // try to open the longest symbolic link we find.  for instance,
    // if the object name is \??\PIPE\MyPipe, we will open the link
    // "\??\PIPE" even though "\??\" itself is also a link
    //

    CScopedHandle handle = CScopedHandle((HANDLE)0, NtClose);
	std::wstring Prefix = Path;
    while (1) {

        status = NtOpenSymbolicLinkObject(&handle, SYMBOLIC_LINK_QUERY, SNtObject(Prefix).Get());
        if (NT_SUCCESS(status))
            break;

        // if (status == STATUS_ACCESS_DENIED // todo

        //
        // path was not a symbolic link, so chop off the
        // last path component before checking the path again
        //

		size_t last_bs = Prefix.find_last_of(L'\\');
		if (last_bs == std::wstring::npos)
			break;
        Prefix.resize(last_bs);
    }

    //
    // if we couldn't locate a symbolic link then we're done
    //

    //not_link:

    if (!handle)
        return Path;

    //
    // otherwise query the symbolic link into the true name buffer
    //

	std::wstring Suffix = Path.substr(Prefix.length());
    
    ULONG PathLen = 0;
    status = NtQuerySymbolicLinkObject(handle, SNtString(0).Get(), &PathLen);
    if (status != STATUS_BUFFER_TOO_SMALL)
        return L"";

    SNtString objname(PathLen / sizeof(WCHAR) + Suffix.length());

    status = NtQuerySymbolicLinkObject(handle, objname.Get(), NULL);
    if (!NT_SUCCESS(status))
        return L"";

    std::wstring Name = objname.Value();

    //
    // name string contains the expansion of the symbolic link,
    // copy the rest of it (the suffix) and invoke ourselves recursively
    //

    //copy_suffix:

    Name = GetName_TranslateSymlinks(Name + Suffix, translated);
    if (!Name.empty())
        *translated = TRUE;

    return Name;
}

//////////////////////////////////////////////////////////////////////////
// Device Shange Notificaiton

// Hidden window procedure
static LRESULT CALLBACK HiddenWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CNtPathMgr* This = (CNtPathMgr*)GetWindowLongPtr(hwnd, 0);

    if (uMsg == WM_DEVICECHANGE) {
        switch (wParam) {
        case DBT_DEVICEARRIVAL:
        case DBT_DEVICEREMOVECOMPLETE: {
            This->UpdateDevices();
            break;
        }
        default:
            break;
        }
    } else if (uMsg == WM_CLOSE) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Notification thread
DWORD WINAPI NotificationThread(LPVOID lpParam)
{
    CNtPathMgr* This = (CNtPathMgr*)lpParam;

	static const wchar_t* WindowClassName = L"NtPathMgr_HiddenWindowClass";

    WNDCLASSEXW wc;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_GLOBALCLASS;
    wc.lpfnWndProc = HiddenWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(ULONG_PTR);
    wc.hInstance = NULL; // (HINSTANCE)::GetModuleHandle(NULL);
    wc.hIcon = NULL; // ::LoadIcon(wc.hInstance, L"AAAPPICON");
    wc.hCursor = NULL; // ::LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WindowClassName;
    wc.hIconSm = NULL;
    if (ATOM lpClassName = RegisterClassEx(&wc))
    {
        This->m_HiddenWindow = CreateWindowExW(0, (LPCWSTR)lpClassName, WindowClassName, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    }
    if (!This->m_HiddenWindow) {
        return 1;
    }

    ::ShowWindow(This->m_HiddenWindow, SW_HIDE);

    SetWindowLongPtr(This->m_HiddenWindow, 0, ULONG_PTR(This));

    // Register for device notifications
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = { 0 };
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    This->m_DeviceNotifyHandle = RegisterDeviceNotificationW(This->m_HiddenWindow, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

    if (!This->m_DeviceNotifyHandle) {
        //printf("RegisterDeviceNotification failed (%lu)\n", GetLastError());
        DestroyWindow(This->m_HiddenWindow);
        This->m_HiddenWindow = NULL;
        return 2;
    }

	// Message loop
    MSG  msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    if (This->m_DeviceNotifyHandle) {
        UnregisterDeviceNotification(This->m_DeviceNotifyHandle);
        This->m_DeviceNotifyHandle = NULL;
    }
    DestroyWindow(This->m_HiddenWindow);
    This->m_HiddenWindow = NULL;

    return 0;
}

void CNtPathMgr::RegisterForDeviceNotifications()
{
    if(m_NotificationThread)
        return;

    // Start the notification thread
    m_NotificationThread = CreateThread(NULL, 0, NotificationThread, this, 0, NULL);
}

void CNtPathMgr::UnRegisterForDeviceNotifications()
{
    // Send a message to close the hidden window and terminate the thread
    if (m_HiddenWindow)
        PostMessage(m_HiddenWindow, WM_CLOSE, 0, 0);

    if (m_NotificationThread) {
        WaitForSingleObject(m_NotificationThread, INFINITE);
        CloseHandle(m_NotificationThread);
    }
}

void CNtPathMgr::RegisterDeviceChangeCallback(void (*func)(void*), void* param)
{
	std::unique_lock lock(m_Mutex);

	m_DeviceChangeCallbacks.push_back(std::make_pair(func, param));
}

void CNtPathMgr::UnRegisterDeviceChangeCallback(void (*func)(void*), void* param)
{
    std::unique_lock lock(m_Mutex);

	for (auto I = m_DeviceChangeCallbacks.begin(); I != m_DeviceChangeCallbacks.end(); ++I)
	{
		if (I->first == func && I->second == param)
		{
			m_DeviceChangeCallbacks.erase(I);
			break;
		}
	}
}

void CNtPathMgr::UpdateDevices()
{
    std::unique_lock lock(m_Mutex);
    
    InitDosDrives();
    InitNtMountMgr();

    auto DeviceChangeCallbacks = m_DeviceChangeCallbacks;
    lock.unlock();

    for (auto I = DeviceChangeCallbacks.begin(); I != DeviceChangeCallbacks.end(); ++I)
    {
        I->first(I->second);
    }
}