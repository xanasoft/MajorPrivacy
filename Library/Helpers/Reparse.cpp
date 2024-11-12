#include "pch.h"
#include "Reparse.h"
#include <ph.h>
#include "NtObj.h"


const WCHAR *File_NamedPipe = L"\\device\\namedpipe\\";
const WCHAR *File_MailSlot  = L"\\device\\mailslot\\";

// Windows 2000 and Windows XP name for the LanmanRedirector device
const WCHAR *File_Redirector = L"\\device\\lanmanredirector\\";
const ULONG File_RedirectorLen = 25;

// Windows Vista name for the LanmanRedirector device
const WCHAR *File_MupRedir = L"\\device\\mup\\;lanmanredirector\\";
const ULONG File_MupRedirLen = 30;

const WCHAR *File_DfsClientRedir = L"\\device\\mup\\dfsclient\\";
const ULONG  File_DfsClientRedirLen = 22;

const WCHAR *File_HgfsRedir = L"\\device\\mup\\;hgfs\\";
const ULONG  File_HgfsRedirLen = 18;

const WCHAR *File_Mup = L"\\device\\mup\\";
const ULONG File_MupLen = 12;

const WCHAR *File_BQQB = L"\\??\\";


#define FILE_SHARE_VALID_FLAGS          0x00000007

CNtPathMgr* CNtPathMgr::m_pInstance = NULL;

#define MAX_DRIVES 26


CNtPathMgr::CNtPathMgr()
{
    InitDrives(0xFFFFFFFF);
}

CNtPathMgr::~CNtPathMgr()
{
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

std::wstring CNtPathMgr::GetNtPathFromHandle(HANDLE hFile)
{
    WCHAR NameBuf[MAX_PATH];
    if (GetFinalPathNameByHandleW(hFile, NameBuf, MAX_PATH, VOLUME_NAME_NT) > 0)
        return NameBuf;
    return L"";
}

////////////////////////////////////////////////////////////////////////////////////////////
// Drives

bool CNtPathMgr::InitDrives(ULONG DriveMask)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objattrs;
    UNICODE_STRING objname;
    ULONG path_len;
    HANDLE handle;
    ULONG drive;
    //ULONG drive_count;
    std::wstring path;
    WCHAR error_str[16];
    static BOOLEAN CallInitLinks = TRUE;

    std::unique_lock lock(m_Mutex);

    //
    // create an array to hold the mappings of the 26 possible
    // drive letters to their full NT pathnames
    //

    if (m_Drives.empty())
        m_Drives.resize(MAX_DRIVES);

    InitializeObjectAttributes(
        &objattrs, &objname, OBJ_CASE_INSENSITIVE, NULL, NULL);

    //drive_count = 0;

    for (drive = 0; drive < 26; ++drive) {

        BOOLEAN HaveSymlinkTarget = FALSE;

        std::shared_ptr<SDrive> old_drive;

        //
        // process this drive if (1) it was specified in the mask,
        // or (2) if it is currently an incomplete drive in the form
        // \??\X:\ and drive X was specified in the mask
        //

        if (! (DriveMask & (1 << drive))) {

            std::shared_ptr<SDrive> file_drive = m_Drives[drive];
            if (file_drive) {
                path = file_drive->path;
                if (wmemcmp(path.c_str(), File_BQQB, 4) == 0 && path[4] && path[5] == L':' && path[6] == L'\\') {

                    WCHAR other_drive = path[4];
                    if (other_drive >= L'A' && other_drive <= L'Z')
                        other_drive = other_drive - L'A' + L'a';
                    if (other_drive >= L'a' && other_drive <= L'z') {
                        other_drive -= L'a';
                        if (DriveMask & (1 << other_drive)) {

                            DriveMask |= (1 << drive);
                        }
                    }
                }
            }

            if (! (DriveMask & (1 << drive)))
                continue;
        }

        if (m_Drives[drive]) {

            old_drive = m_Drives[drive];
            m_Drives[drive].reset();

            RemovePermLinks(old_drive->path.c_str());
        }

        //
        // translate the DosDevices symbolic link "\??\x:" into its
        // device object.  note that sometimes the DosDevices name
        // translate to symbolic link itself.
        //

        path = L"\\??\\ :";
        path[4] = (wchar_t)(L'A' + drive);

        RtlInitUnicodeString(&objname, path.c_str());

        status = NtOpenSymbolicLinkObject(&handle, SYMBOLIC_LINK_QUERY, &objattrs);

        // if (!NT_SUCCESS(status)) // todo on access denided
        
        //
        // errors indicate the drive does not exist, is no longer valid
        // or was never valid.  we can also get access denied, if the
        // drive appears as ClosedFilePath.  in any case, silently ignore
        //

        if (! NT_SUCCESS(status)) {

            if (status != STATUS_OBJECT_NAME_NOT_FOUND &&
                status != STATUS_OBJECT_PATH_NOT_FOUND &&
                status != STATUS_OBJECT_TYPE_MISMATCH &&
                status != STATUS_ACCESS_DENIED) {

                _snwprintf(error_str, 16, L"%c [%08X]", L'A' + drive, status);
                //SbieApi_Log(2307, error_str);
            }

            if (old_drive) {
                AdjustDrives(drive, old_drive->subst, old_drive->path.c_str());
            }

            continue;
        }

        //
        // get the target of the symbolic link
        //

        if (handle) {

            memset(&objname, 0, sizeof(UNICODE_STRING));
            status = NtQuerySymbolicLinkObject(handle, &objname, &path_len);

            if (status != STATUS_BUFFER_TOO_SMALL) {
                if (NT_SUCCESS(status))
                    status = STATUS_UNSUCCESSFUL;

            } else {

                path_len += 32;
                path.resize(path_len / sizeof(WCHAR));
                
                objname.MaximumLength = (USHORT)(path_len - 8);
                objname.Length        = objname.MaximumLength - sizeof(WCHAR);
                objname.Buffer        = (WCHAR*)path.c_str();
                status = NtQuerySymbolicLinkObject(handle, &objname, NULL);

                if (NT_SUCCESS(status)) {
                    path.resize(objname.Length / sizeof(WCHAR));
                    HaveSymlinkTarget = TRUE;
                }
            }

            NtClose(handle);
        }

        //
        // add a new drive entry
        //

        if (HaveSymlinkTarget) {

            bool subst = false;
            bool translated = false;

            if (wmemcmp(path.c_str(), File_BQQB, 4) == 0 && path[4] && path[5] == L':') {
                // SUBST drives translate to \??\X:...
                subst = TRUE;
            }

            path = GetName_TranslateSymlinks(path, &translated);
            if (!path.empty()) {

                // (returned path points into NameBuffer
                // so it need not be explicitly freed)

                std::shared_ptr<SDrive> file_drive = std::make_shared<SDrive>();
                file_drive->letter = (wchar_t)(L'A' + drive);
                file_drive->subst = subst;
                file_drive->path = path;
                *file_drive->sn = 0;
                if (1) { // todo
                    ULONG sn = GetVolumeSN(file_drive->path);
                    if(sn != 0)
                        _snwprintf(file_drive->sn, 10, L"%04X-%04X", HIWORD(sn), LOWORD(sn));
                }

                m_Drives[drive] = file_drive;
                //drive_count++;

                //SbieApi_MonitorPut(MONITOR_DRIVE, path);
            }
        }

        if (! NT_SUCCESS(status)) {

            _snwprintf(error_str, 16, L"%c [%08X]", L'A' + drive, status);
            //SbieApi_Log(2307, error_str);
        }
    }

    //if (drive_count == 0) {
    //    Sbie_snwprintf(error_str, 16, L"No Drives Found");
    //    SbieApi_Log(2307, error_str);
    //}

    //
    // initialize list of volumes mounted to directories rather than drives
    //

    if (CallInitLinks) {

        CallInitLinks =FALSE;

        InitLinksLocked();
    }

    return true;
}

void CNtPathMgr::AdjustDrives(ULONG path_drive_index, bool subst, const std::wstring& path)
{
    std::wstring file_drive_path;
    ULONG path_len;
    ULONG drive_index;
    BOOLEAN file_drive_subst;

    //
    // if the path of any other drives references the path being removed,
    // then convert the path of the other drive to the form \\X:\...
    //
    // if the drive being removed is a SUBST drive, then we only look at
    // other SUBST drives here, to prevent the removal of the SUBST drive
    // from having an effect on drives that are not related to the SUBST
    //

    path_len = (ULONG)path.length();

    for (drive_index = 0; drive_index < 26; ++drive_index) {

        std::shared_ptr<SDrive> file_drive = m_Drives[drive_index];
        if (! file_drive)
            continue;
        file_drive_subst = file_drive->subst;
        if (subst && (! file_drive_subst))
            continue;
        file_drive_path = file_drive->path;

        if (file_drive_path.length() >= path_len && (file_drive_path[path_len] == L'\0' || file_drive_path[path_len] == L'\\') && _wcsnicmp(file_drive_path.c_str(), path.c_str(), path_len) == 0) {

            file_drive = std::make_shared<SDrive>();
            file_drive->letter = (wchar_t)(L'A' + drive_index);
            file_drive->subst = file_drive_subst;
            file_drive->path = File_BQQB;
            file_drive->path += (WCHAR)(path_drive_index + L'A');
            file_drive->path += L':';
            file_drive->path += file_drive_path.substr(path_len);

            m_Drives[drive_index] = file_drive;

            //SbieApi_MonitorPut(MONITOR_DRIVE, new_path);
        }
    }
}

ULONG CNtPathMgr::GetVolumeSN(const std::wstring& DriveNtPath)
{
    ULONG sn = 0;
    HANDLE handle;
    IO_STATUS_BLOCK iosb;

    ULONG OldMode;
    RtlSetThreadErrorMode(0x10u, &OldMode);
    NTSTATUS status = NtCreateFile(
        &handle, GENERIC_READ | SYNCHRONIZE, SNtObject(DriveNtPath + L"\\").Get(),
        &iosb, NULL, 0, FILE_SHARE_VALID_FLAGS,
        FILE_OPEN,
        FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
        NULL, 0);
    RtlSetThreadErrorMode(OldMode, 0i64);

    if (NT_SUCCESS(status))
    {
        union {
            FILE_FS_VOLUME_INFORMATION volumeInfo;
            BYTE volumeInfoBuff[64];
        } u;
        if (NT_SUCCESS(NtQueryVolumeInformationFile(handle, &iosb, &u.volumeInfo, sizeof(u), FileFsVolumeInformation)))
            sn = u.volumeInfo.VolumeSerialNumber;

        NtClose(handle);
    }

    return sn;
}

std::wstring CNtPathMgr::GetDrivePath(ULONG DriveIndex)
{
    if (m_Drives.empty() || (DriveIndex == -1)) {
        InitDrives(0xFFFFFFFF);
        if (DriveIndex == -1)
            return L"";
    }

    if (!m_Drives.empty() && (DriveIndex < MAX_DRIVES) && m_Drives[DriveIndex])
        return m_Drives[DriveIndex]->path;

    return L"";
}

std::shared_ptr<const CNtPathMgr::SDrive> CNtPathMgr::GetDriveForPath(const std::wstring& Path)
{
    std::shared_ptr<const SDrive> drive;
    ULONG i;

    std::unique_lock lock(m_Mutex);

    for (i = 0; i < 26; ++i) {

        drive = m_Drives[i];
        if (drive) {

            if (Path.length() >= drive->path.length() && _wcsnicmp(Path.c_str(), drive->path.c_str(), drive->path.length()) == 0) {

                //
                // make sure access to \Device\HarddiskVolume10 (for M:),
                // for instance, is not matched by \Device\HarddiskVolume1
                // (for C:), by requiring a backslash or null character
                // to follow the matching drive path
                //

                const WCHAR *ptr = Path.c_str() + drive->path.length();
                if (*ptr == L'\\' || *ptr == L'\0')
                    break;
            }

            drive = NULL;
        }
    }

    return drive;
}

std::shared_ptr<const CNtPathMgr::SDrive> CNtPathMgr::GetDriveForUncPath(const std::wstring& Path, ULONG *OutPrefixLen)
{
    std::shared_ptr<CNtPathMgr::SDrive> drive;
    WCHAR *ptr1a;
    ULONG pfx_len, sfx_len;
    ULONG i;

    //
    // given an input path like
    // \Device\LanmanRedirector\server\share\file.txt
    // find the corresponding drive even if it is stored as
    // \Device\LanmanRedirector\;Q:0000000000001234\server\share
    //

    *OutPrefixLen = 0;

    if (Path.length() <= 8 || _wcsnicmp(Path.c_str(), File_Mup, 8) != 0)
        return NULL;
    ptr1a = (WCHAR*)wcschr(Path.c_str() + 8, L'\\'); // todo refactor
    if (! ptr1a)
        return NULL;
    pfx_len = (ULONG)(ptr1a - Path.c_str());
    sfx_len = (ULONG)wcslen(ptr1a);

    //
    //
    //

    std::unique_lock lock(m_Mutex);

    for (i = 0; i < 26; ++i) {

        drive = m_Drives[i];
        if (drive && drive->path.length() > 8 && // make sure it starts with \device
            _wcsnicmp(drive->path.c_str(), File_Mup, 8) == 0) {

            //
            // make sure the device component is the same
            //

            const WCHAR *ptr1b = wcschr(drive->path.c_str() + 8, L'\\');
            if (ptr1b && (ptr1b - drive->path.c_str()) == pfx_len && *(ptr1b + 1) == L';' && _wcsnicmp(Path.c_str(), drive->path.c_str(), pfx_len) == 0) {

                const WCHAR *ptr2b = wcschr(ptr1b + 1, L'\\');
                if (ptr2b) {

                    ULONG sfx_len_2 = (ULONG)wcslen(ptr2b);
                    if (sfx_len >= sfx_len_2 && _wcsnicmp(ptr1a, ptr2b, sfx_len_2) == 0) {

                        const WCHAR *ptr2a = ptr1a + sfx_len_2;
                        if (*ptr2a == L'\\' || *ptr2a == L'\0') {

                            *OutPrefixLen = (ULONG)(ptr2a - Path.c_str());
                            break;
                        }
                    }
                }
            }

            drive = NULL;
        }
    }

    return drive;
}

std::shared_ptr<const CNtPathMgr::SDrive> CNtPathMgr::GetDriveForLetter(WCHAR drive_letter)
{
    std::shared_ptr<const CNtPathMgr::SDrive> drive;

    std::unique_lock lock(m_Mutex);

    if (drive_letter >= L'A' && drive_letter <= L'Z')
        drive = m_Drives[drive_letter - L'A'];
    else if (drive_letter >= L'a' && drive_letter <= L'z')
        drive = m_Drives[drive_letter - L'a'];
    else
        drive = NULL;

    return drive;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Links

void CNtPathMgr::InitLinksLocked()
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objattrs;
    UNICODE_STRING objname;
    IO_STATUS_BLOCK IoStatusBlock;
    HANDLE hMountMgr;
    MOUNTMGR_MOUNT_POINT Input1;
    MOUNTMGR_MOUNT_POINTS *Output1;
    MOUNTDEV_NAME *Input2;
    MOUNTMGR_VOLUME_PATHS *Output2;
    ULONG index1;
    WCHAR save_char;
    std::shared_ptr<SGuid> guid;
    WCHAR text[256];

    //
    // cleanup old guid entries
    //

    m_GuidLinks.clear();

    //
    // open mount point manager device
    //

    RtlInitUnicodeString(&objname, L"\\Device\\MountPointManager");
    InitializeObjectAttributes(
        &objattrs, &objname, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtCreateFile(
        &hMountMgr, FILE_GENERIC_EXECUTE, &objattrs, &IoStatusBlock,
        NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_VALID_FLAGS,
        FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE,
        NULL, 0);

    if (! NT_SUCCESS(status))
        return;

    //
    // query all mount points
    //

    memset(&Input1, 0, sizeof(MOUNTMGR_MOUNT_POINT));
    Output1 = (MOUNTMGR_MOUNT_POINTS*)malloc(8192);

    status = NtDeviceIoControlFile(
        hMountMgr, NULL, NULL, NULL, &IoStatusBlock,
        IOCTL_MOUNTMGR_QUERY_POINTS,
        &Input1, sizeof(MOUNTMGR_MOUNT_POINT),
        Output1, 8192);

    if (! NT_SUCCESS(status)) {
        NtClose(hMountMgr);
        return;
    }

    Input2 = (MOUNTDEV_NAME*)malloc(128);
    Output2 = (MOUNTMGR_VOLUME_PATHS*)malloc(8192);

    //
    // process each primary mount point of each volume
    //

    for (index1 = 0; index1 < Output1->NumberOfMountPoints; ++index1) {

        MOUNTMGR_MOUNT_POINT *MountPoint = &Output1->MountPoints[index1];
        WCHAR *DeviceName =
            (WCHAR *)((UCHAR *)Output1 + MountPoint->DeviceNameOffset);
        ULONG DeviceNameLen =
            MountPoint->DeviceNameLength / sizeof(WCHAR);
        WCHAR *VolumeName =
            (WCHAR *)((UCHAR *)Output1 + MountPoint->SymbolicLinkNameOffset);
        ULONG VolumeNameLen =
            MountPoint->SymbolicLinkNameLength / sizeof(WCHAR);

        _snwprintf(text, 256, L"Found Mountpoint: %.*s <-> %.*s", VolumeNameLen, VolumeName, DeviceNameLen, DeviceName);
        DbgPrint("%S\r\n", text);

        if (VolumeNameLen != 48 && VolumeNameLen != 49)
            continue;
        if (_wcsnicmp(VolumeName, L"\\??\\Volume{", 11) != 0)
            continue;

        //
        // store guid to nt device association
        //

        guid = std::make_shared<SGuid>();
        wmemcpy(guid->guid, &VolumeName[10], 38);
        guid->guid[38] = 0;
        guid->path = std::wstring(DeviceName, DeviceNameLen);
        m_GuidLinks.push_back(guid);

        //
        // get all the DOS paths where the volume is mounted
        //

        Input2->NameLength = 48 * sizeof(WCHAR);
        wmemcpy(Input2->Name, VolumeName, 48);
        Input2->Name[48] = L'\0';

        status = NtDeviceIoControlFile(
            hMountMgr, NULL, NULL, NULL, &IoStatusBlock,
            IOCTL_MOUNTMGR_QUERY_DOS_VOLUME_PATHS,
            Input2, 98,
            Output2, 8192);

        if (! NT_SUCCESS(status))
            continue;

        //
        // process DOS paths
        //

        save_char = DeviceName[DeviceNameLen];
        DeviceName[DeviceNameLen] = L'\0';

        if (Output2->MultiSzLength && *Output2->MultiSz) {

            WCHAR *DosPath = Output2->MultiSz;
            ULONG DosPathLen = (ULONG)wcslen(DosPath);
            if (DosPathLen <= 3) {

                //
                // handle the case where the volume is also mounted as a
                // drive letter:
                // 1.  ignore the first mounted path (the drive letter)
                // 2.  add a reparse point from any other mounted path to
                //     the device name
                //

                DosPath += DosPathLen + 1;
                while (*DosPath) {
                    _snwprintf(text, 256, L"Mountpoint AddLink: %s <-> %s", DosPath, DeviceName);
                    DbgPrint("%S\r\n", text);
                    AddLink(TRUE, DosPath, DeviceName);
                    DosPath += wcslen(DosPath) + 1;
                }

            } else if (1) { // todo

                // handle the case where the volume is not mounted as a
                // drive letter:
                //     add reparse points for all mounted directories

                //
                // This behaviour creates \[BoxRoot]\drive\{guid} folders
                // instead of using the first mount point on a volume with a letter
                //

                WCHAR *FirstDosPath = DosPath;
                _snwprintf(text, 256, L"Mountpoint AddLink: %s <-> %s", FirstDosPath, DeviceName);
                DbgPrint("%S\r\n", text);
                AddLink(TRUE, FirstDosPath, DeviceName);
                DosPath += DosPathLen + 1;
                while (*DosPath) {
                    _snwprintf(text, 256, L"Mountpoint AddLink: %s <-> %s", DosPath, DeviceName);
                    DbgPrint("%S\r\n", text);
                    AddLink(TRUE, DosPath, DeviceName);
                    DosPath += wcslen(DosPath) + 1;
                }

            } else {

                //
                // handle the case where the volume is not mounted as a
                // drive letter:
                // 1.  add a reparse point from the device name to the first
                //     mounted directory
                // 2.  add a reparse point from any other mounted directory
                //     also to the first mounted directory
                //

                //
                // Note: this behaviour makes the first mounted directory
                // the location in the box where all files for that volume will be located
                // other mount points will be redirected to this folder
                //

                WCHAR *FirstDosPath = DosPath;
                _snwprintf(text, 256, L"Mountpoint AddLink: %s <-> %s", DeviceName, FirstDosPath);
                DbgPrint("%S\r\n", text);
                AddLink(TRUE, DeviceName, FirstDosPath);
                DosPath += DosPathLen + 1;
                while (*DosPath) {
                    _snwprintf(text, 256, L"Mountpoint AddLink: %s <-> %s", DosPath, FirstDosPath);
                    DbgPrint("%S\r\n", text);
                    AddLink(TRUE, DosPath, FirstDosPath);
                    DosPath += wcslen(DosPath) + 1;
                }
            }
        }

        DeviceName[DeviceNameLen] = save_char;
    }

    free(Output2);
    free(Input2);
    free(Output1);
    NtClose(hMountMgr);
}

std::wstring CNtPathMgr::GetName_TranslateSymlinks(const std::wstring& Path, bool *translated)
{
    const WCHAR *objname_buf = Path.c_str();
    ULONG objname_len = (ULONG)Path.length() * sizeof(WCHAR);
    NTSTATUS status;
    HANDLE handle;
    OBJECT_ATTRIBUTES objattrs;
    UNICODE_STRING objname;
    WCHAR *name;
    ULONG path_len;
    const WCHAR *suffix;
    ULONG suffix_len;
    std::wstring sname;

    // This prevents an additional \device\pipename prefix being added by File_GetBoxedPipeName (thanks Chrome).
    if (objname_len > 26 && !_wcsnicmp(objname_buf, L"\\??\\pipe", 8)) {
        if (!_wcsnicmp(objname_buf + 8, File_NamedPipe, 17)) {
            objname_buf += 8;
        }
    }

    /*if (objname_len >= 18 && File_IsNamedPipe(objname_buf, NULL)) { // todo
    handle = NULL;
    goto not_link;
    }*/

    InitializeObjectAttributes(
        &objattrs, &objname, OBJ_CASE_INSENSITIVE, NULL, NULL);

    objname.Length = (USHORT)objname_len;
    objname.Buffer = (WCHAR *)objname_buf;

    //
    // try to open the longest symbolic link we find.  for instance,
    // if the object name is \??\PIPE\MyPipe, we will open the link
    // "\??\PIPE" even though "\??\" itself is also a link
    //

    while (1) {

        objname.MaximumLength = objname.Length;

        status = NtOpenSymbolicLinkObject(
            &handle, SYMBOLIC_LINK_QUERY, &objattrs);

        if (NT_SUCCESS(status))
            break;

        // if (status == STATUS_ACCESS_DENIED // todo

        //
        // path was not a symbolic link, so chop off the
        // last path component before checking the path again
        //

        handle = NULL;

        if (objname.Length <= sizeof(WCHAR))
            break;

        do {
            objname.Length -= sizeof(WCHAR);
        } while (   objname.Length &&
            objname_buf[objname.Length / sizeof(WCHAR)] != L'\\');

        if (objname.Length <= sizeof(WCHAR))
            break;
    }

    //
    // if we couldn't locate a symbolic link then we're done
    //

    //not_link:

    if (! handle) {

        sname.resize(objname_len / sizeof(WCHAR));
        name = (WCHAR*)sname.c_str();

        memcpy(name, objname_buf, objname_len);
        *(name + objname_len / sizeof(WCHAR)) = L'\0';

        return name;
    }

    //
    // otherwise query the symbolic link into the true name buffer
    //

    suffix = objname_buf + objname.Length / sizeof(WCHAR);
    suffix_len = objname_len - objname.Length;

    memset(&objname, 0, sizeof(UNICODE_STRING));
    path_len = 0;
    status = NtQuerySymbolicLinkObject(handle, &objname, &path_len);
    if (status != STATUS_BUFFER_TOO_SMALL) {
        NtClose(handle);
        return NULL;
    }

    sname.resize((path_len + suffix_len) / sizeof(WCHAR));
    name = (WCHAR*)sname.c_str();

    objname.Length = (USHORT)(path_len - sizeof(WCHAR));
    objname.MaximumLength = (USHORT)path_len;
    objname.Buffer = name;
    status = NtQuerySymbolicLinkObject(handle, &objname, NULL);

    path_len = objname.Length;

    NtClose(handle);

    //
    // the true name buffer contains the expansion of the symbolic link,
    // copy the rest of it (the suffix) and invoke ourselves recursively
    //

    //copy_suffix:

    if (! NT_SUCCESS(status))
        name = NULL;

    else {

        WCHAR *name2 = name + path_len / sizeof(WCHAR);

        memcpy(name2, suffix, suffix_len);
        name2 += suffix_len / sizeof(WCHAR);
        *name2 = L'\0';

        //
        // copy the result to the copy name buffer, for the recursive
        // invocation of this function.  if the recursive invocation
        // doesn't find a symbolic link to translate, it will simply
        // put the result in the true name buffer, and return
        //

        std::wstring sname2;
        sname2.resize((path_len + suffix_len) / sizeof(WCHAR));
        name2 = (WCHAR*)sname.c_str();

        memcpy(name2, name, path_len + suffix_len + sizeof(WCHAR));

        sname = GetName_TranslateSymlinks(sname2, translated);
        if (!sname.empty())
            *translated = TRUE;
    }

    return sname;
}

bool CNtPathMgr::AddLink(bool PermLink, const std::wstring& Src, const std::wstring& Dst)
{
    ULONG src_len;
    ULONG dst_len;
    ULONG drive_letter;
    std::shared_ptr<SDrive>src_drive;
    std::shared_ptr<SDrive>dst_drive;
    std::shared_ptr<SLink>link;
    std::shared_ptr<SLink>old_link;

    //
    // prepare a new link structure
    //

    src_drive = NULL;
    src_len = (ULONG)Src.length();
    if (Src[1] == L':') {
        drive_letter = Src[0];
        if (drive_letter >= L'A' && drive_letter <= L'Z')
            drive_letter = drive_letter - L'A' + L'a';
        if (drive_letter >= L'a' && drive_letter <= L'z') {
            src_drive = m_Drives[drive_letter - L'a'];
            if (src_drive)
                src_len += (ULONG)src_drive->path.length();
        }
    }

    dst_drive = NULL;
    dst_len = (ULONG)Dst.length();
    if (Dst[1] == L':') {
        drive_letter = Dst[0];
        if (drive_letter >= L'A' && drive_letter <= L'Z')
            drive_letter = drive_letter - L'A' + L'a';
        if (drive_letter >= L'a' && drive_letter <= L'z') {
            dst_drive = m_Drives[drive_letter - L'a'];
            if (dst_drive)
                dst_len += (ULONG)dst_drive->path.length();
        }
    }

    link = std::make_shared<SLink>();

    //
    // prepare src path
    //

    if (src_drive) {
        link->src = src_drive->path;
        link->src += Src.substr(2);
    } else
        link->src =  Src;

    while (link->src.length() && link->src[link->src.length() - 1] == L'\\')
        link->src.erase(link->src.length() - 1);

    //
    // prepare dst path
    //

    if (dst_drive) {
        link->dst = dst_drive->path;
        link->dst += Dst.substr(2);
    } else
        link->dst = Dst;

    while (link->dst.length() && link->dst[link->dst.length() - 1] == L'\\')
        link->dst.erase(link->dst.length() - 1);

    //
    // abort if src or dst are null
    //

    if (link->src.empty() || link->dst.empty())
        return FALSE;

    //
    // abort if src and dst are the same
    //

    if (PermLink && link->src.length() == link->dst.length() && _wcsicmp(link->src.c_str(), link->dst.c_str()) == 0)
        return FALSE;

    //
    // abort if entry was already added for this src.
    //
    // in the case of a volume not mounted as a drive letter,
    // the specified src can be the destination of an old entry
    //
    // if not duplicate, add the new link
    //

    std::unique_lock lock(m_Mutex);

    if (PermLink) {

        for(auto I = m_PermLinks.begin(); I != m_PermLinks.end(); ++I)
        {
            old_link = *I;

            if ((old_link->src.length() == link->src.length() && _wcsicmp(old_link->src.c_str(), link->src.c_str()) == 0)
            ||  (old_link->dst.length() == link->src.length() && _wcsicmp(old_link->dst.c_str(), link->src.c_str()) == 0))
                return FALSE;
        }

        link->ticks = 0;
        link->same = FALSE;

        m_PermLinks.push_back(link); //List_Insert_After(File_PermLinks, NULL, link);

    } else {

        link->ticks = GetTickCount();
        if (link->src.length() == link->dst.length() && _wcsicmp(link->src.c_str(), link->dst.c_str()) == 0)
            link->same = TRUE;
        else
            link->same = FALSE;

        if(link->same)
            return FALSE;
        m_TempLinks.push_front(link);
    }

    return TRUE;
}


void CNtPathMgr::RemovePermLinks(const std::wstring& path)
{
    ULONG path_len;
    std::shared_ptr<SLink> old_link;

    //
    // if a drive is being unmounted (or remounted), scan the list of
    // reparse links and remove any references to its related device.
    //

    path_len = (ULONG)path.length();

    for(auto I = m_PermLinks.begin(); I != m_PermLinks.end();)
    {
        old_link = *I;

        const ULONG src_len = (ULONG)old_link->src.length();
        const WCHAR *src    = old_link->src.c_str();
        if (src_len >= path_len && (src[path_len] == L'\\' || src[path_len] == L'\0') && _wcsnicmp(path.c_str(), src, path_len) == 0) 
        {
            I = m_PermLinks.erase(I);
        }
        else
            ++I;
    }
}

#define FILE_IS_REDIRECTOR_OR_MUP(str,len)                              \
   (    (len >= File_RedirectorLen &&                                   \
            0 == _wcsnicmp(str, File_Redirector, File_RedirectorLen))   \
    ||  (len >= File_MupLen &&                                          \
            0 == _wcsnicmp(str, File_Mup, File_MupLen)))

ULONG CNtPathMgr::GetDrivePrefixLength(const std::wstring& work_str)
{
    ULONG prefix_len = 0;

    if (!FILE_IS_REDIRECTOR_OR_MUP(work_str.c_str(), work_str.length())) {
        std::shared_ptr<const SDrive> drive = GetDriveForPath(work_str);
        if (drive) {
            prefix_len = (ULONG)drive->path.length();
        }
        else {
            std::shared_ptr<const SGuid> guid = GetGuidForPath(work_str);
            if (guid) {
                prefix_len = (ULONG)guid->path.length();
            }
        }
    }

    if (prefix_len == work_str.length())
        prefix_len = 0;
    return prefix_len;
}

std::wstring CNtPathMgr::TranslateTempLinks(const std::wstring& _TruePath, bool bReverse, bool StripLastPathComponent)
{
    static ULONG cleanup_ticks = 0;
    std::shared_ptr<SLink> link;
    std::wstring ret;
    ULONG ret_len;
    std::wstring TruePath = _TruePath;
    std::wstring TruePath_Sufix;

    //
    // entry
    //

    ULONG ticks = GetTickCount();
    /*static ULONG TimeSpentHere = 0;
    static ULONG TimeSpentHereLastReport = 0;*/

    //
    // initialize length of input path
    //

    if (StripLastPathComponent) {
        size_t bs_pos = TruePath.find_last_of(L'\\');
        if (bs_pos == std::wstring::npos)
			return L"";
        TruePath_Sufix = TruePath.substr(bs_pos);
        TruePath.erase(bs_pos);

    } else {
        while(!TruePath.empty() && TruePath.back() == L'\\')
			TruePath.pop_back();
        if (TruePath.empty())
            return L"";
    }

    //
    // make sure the path is for a local drive
    //

    if(!GetDrivePrefixLength(TruePath))
        return ret;

    //
    // on first loop throughout this function, clean old entries
    //

    std::unique_lock lock(m_Mutex);

    if (ticks - cleanup_ticks > 1000) {

        cleanup_ticks = ticks;

        for(auto I = m_TempLinks.begin(); I != m_TempLinks.end();)
        {
            link = *I;

            if (ticks - link->ticks > 10 * 1000)
                I = m_TempLinks.erase(I);
            else 
                ++I;
        }
    }

    //
    // look for an exact match in the list of temporary links
    //

    for(auto I = m_TempLinks.begin(); I != m_TempLinks.end(); ++I)
    {
        link = *I;

        if ((bReverse ? link->dst : link->src).length() == TruePath.length() && 0 == _wcsnicmp((bReverse ? link->dst : link->src).c_str(), TruePath.c_str(), TruePath.length())) {

            if (! link->same) {

                //
                // link->dst is different from link->src, so we need to
                // append the last component to link->dst
                //

                ret = std::wstring((bReverse ? link->src : link->dst).c_str(), (bReverse ? link->src : link->dst).length());
                ret += TruePath_Sufix;
            }

            goto finish;
        }
    }

    //
    // if there was no exact match then process the path
    //

    ret = TranslateTempLinks_2(TruePath, bReverse);
    if (ret.empty())
        goto finish;

    //
    // add a link from the original true path to the final result
    //

    link = NULL;
    for(auto I = m_TempLinks.begin(); I != m_TempLinks.end(); ++I)
    {
        link = *I;

        if ((bReverse ? link->dst : link->src).length() == TruePath.length() && 0 == _wcsnicmp((bReverse ? link->dst : link->src).c_str(), TruePath.c_str(), TruePath.length()))
            break;
        link = NULL;
    }

    if (! link) {

        AddLink(FALSE, TruePath, ret);
    }

    //
    // if result is same as input, then we can return null
    //

    ret_len = (ULONG)ret.length();

    if (ret_len == TruePath.length() && 0 == _wcsnicmp(ret.c_str(), TruePath.c_str(), ret_len)) {

        ret.clear();
        goto finish;
    }

    //
    // append the last component to the return path
    //

    if (StripLastPathComponent)
        ret += TruePath_Sufix;

    //
    // finish
    //

finish:

    /*TimeSpentHere += GetTickCount() - ticks;
    if (TimeSpentHere - TimeSpentHereLastReport > 5000) {
    WCHAR txt[256];
    Sbie_snwprintf(txt, 256, L"Time Spent On Links = %d\n", TimeSpentHere);
    OutputDebugString(txt);
    TimeSpentHereLastReport = TimeSpentHere;
    }*/
    return ret;
}

std::wstring CNtPathMgr::TranslateTempLinks_2(const std::wstring& input_str, bool bReverse)
{
    std::shared_ptr<SLink> link;
    std::shared_ptr<SLink> best_link;
    std::wstring work_str;
    ULONG prefix_len;

    //
    // duplicate the input string as a work area
    //

    work_str = input_str;

    //
    // main loop
    //

    while (1) {

        //
        // find longest matching prefix from the list of temporary links
        //

        prefix_len = 0;
        best_link = NULL;

        for(auto I = m_TempLinks.begin(); I != m_TempLinks.end(); ++I)
        {
            link = *I;

			std::wstring& temp = (bReverse ? link->dst : link->src);

            if (temp.length() <= work_str.length() && temp.length() > prefix_len
                && (work_str[temp.length()] == L'\\' || work_str[temp.length()] == L'\0')
                && 0 == _wcsnicmp(temp.c_str(), work_str.c_str(), temp.length())) {

                prefix_len = (ULONG)temp.length();
                best_link = link;
            }
        }

        link = best_link;


        //
        // if we found a prefix, combine it with rest of string, then
        // restart the loop in case we have a link for the combined path
        //

        if (link) {

            if (! link->same) {

                std::wstring& temp = (bReverse ? link->src : link->dst);

                const ULONG dst_len = (ULONG)temp.length();

                work_str = temp + work_str.substr(prefix_len);

                if ((!link->stop) && (input_str.length() - prefix_len + 1 > 1))
                    continue;

                break;

            } else if (work_str.length() == prefix_len || link->stop)
                break;

        } else {

            //
            // otherwise make sure we are dealing with a local drive
            //

            prefix_len = GetDrivePrefixLength(work_str);
            if (!prefix_len)
                break;
        }

        //
        // at this point we either did not find a prefix, or we found a
        // prefix where the destination is the same as the source, i.e.
        // a normal directory path, so check the next directory on the path
        //

        if (1) {

            size_t bs_pos = work_str.find(L'\\', prefix_len + 1);
            if (bReverse)
                link = AddReverseTempLink(work_str.substr(0, bs_pos));
            else
                link = AddTempLink(work_str.substr(0, bs_pos));
            if (! link) // this can only happen due to an internal error
                break;
        }
    }

    //
    // finish
    //

    return work_str;
}

std::shared_ptr<CNtPathMgr::SLink> CNtPathMgr::AddTempLink(const std::wstring& path)
{
    NTSTATUS status;
    HANDLE handle;
    IO_STATUS_BLOCK IoStatusBlock;
    std::shared_ptr<SLink> link;
    std::wstring newpath;
    BOOLEAN stop;
    BOOLEAN bPermLinkPath = FALSE;

    //
    // try to open the path
    //

    stop = TRUE;

    status = NtCreateFile(
        &handle, FILE_READ_ATTRIBUTES | SYNCHRONIZE, SNtObject(path).Get(),
        &IoStatusBlock, NULL, 0, FILE_SHARE_VALID_FLAGS,
        FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
        NULL, 0);

    if (NT_SUCCESS(status)) 
    {
        //
        // get the reparsed absolute path
        //

        newpath = GetNtPathFromHandle(handle);
        if (!newpath.empty()) 
        {
            //
            // make sure path does not contain duplicate backslashes
            //

            for (size_t i = 0; i < newpath.size(); ++i) {
                if (newpath[i] == L'\\' && i + 1 < newpath.size() && newpath[i + 1] == L'\\') {
                    newpath.erase(i, 1);
                }
            }

            //
            // convert permanent links (i.e. drive mount points)
            //

            newpath = FixPermLinksForTempLink(newpath, false);

            //
            // verify the link points to a local drive
            //

            if (FindPermLinksForMatchPath(newpath, false))
                bPermLinkPath = TRUE;

            if (! FILE_IS_REDIRECTOR_OR_MUP(newpath.c_str(), newpath.length()) && !bPermLinkPath) {
                std::shared_ptr<const SDrive> drive = GetDriveForPath(newpath);
                if (drive)
                    stop = FALSE;
            }

        }

        NtClose(handle);
    }

    //
    // add the new link and return
    //

    if (newpath.empty())
        newpath = path;

    if (AddLink(FALSE, path, newpath)) {
        link = m_TempLinks.empty() ? NULL : m_TempLinks.front();
        if(link)
            link->stop = stop;
    } else
        link = NULL;

    return link;
}

std::shared_ptr<CNtPathMgr::SLink> CNtPathMgr::AddReverseTempLink(const std::wstring& path)
{
    std::shared_ptr<SLink> link;
    std::wstring newpath = path;
    BOOLEAN stop;
    BOOLEAN bPermLinkPath = FALSE;

    //
    // try to open the path
    //

    stop = TRUE;

    //
    // make sure path does not contain duplicate backslashes
    //

    for (size_t i = 0; i < newpath.size(); ++i) {
        if (newpath[i] == L'\\' && i + 1 < newpath.size() && newpath[i + 1] == L'\\') {
            newpath.erase(i, 1);
        }
    }

    //
    // convert permanent links (i.e. drive mount points)
    //

    newpath = FixPermLinksForTempLink(newpath, true);

    //
    // verify the link points to a local drive
    //

    if (FindPermLinksForMatchPath(newpath, true))
        bPermLinkPath = TRUE;

    if (! FILE_IS_REDIRECTOR_OR_MUP(path.c_str(), path.length()) && !bPermLinkPath) {
        std::shared_ptr<const SDrive> drive = GetDriveForPath(path);
        if (drive)
            stop = FALSE;
    }


    //
    // add the new link and return
    //

    if (newpath.empty())
        newpath = path;

    if (AddLink(FALSE, newpath, path)) {
        link = m_TempLinks.empty() ? NULL : m_TempLinks.front();
        if(link)
            link->stop = stop;
    } else
        link = NULL;

    return link;
}

std::wstring CNtPathMgr::FixPermLinksForTempLink(const std::wstring& name, bool bReverse)
{
    std::wstring result = name;

    ULONG retries = 0;

    for (auto I = m_PermLinks.begin(); I != m_PermLinks.end();) 
    {
        std::shared_ptr<SLink> link = *I;

        const ULONG src_len = (ULONG)(bReverse ? link->dst : link->src).length();
        const ULONG name_len = (ULONG)result.length();

        if (name_len >= src_len && (result[src_len] == L'\\' || result[src_len] == L'\0') && _wcsnicmp(result.c_str(), (bReverse ? link->dst : link->src).c_str(), src_len) == 0) 
        {
            const ULONG dst_len = (ULONG)(bReverse ? link->src : link->dst).length();
            if (dst_len + name_len - src_len <= result.max_size()) 
            {
                if (src_len != dst_len)
                    result.replace(0, src_len, (bReverse ? link->src : link->dst));
                else
                    result.replace(0, dst_len, (bReverse ? link->src : link->dst));

                I = m_PermLinks.begin();
                link = *I;
                ++retries;
                if (retries == 16)
                    break;
                else
                    continue;
            }
        }

        ++I;
    }

    return result;
}

void CNtPathMgr::GetDriveAndLinkForPath(const std::wstring& Path, std::shared_ptr<const SDrive>& OutDrive, std::shared_ptr<const SLink>& OutLink)
{
    OutDrive = GetDriveForPath(Path);

    std::unique_lock lock(m_Mutex);

    for(auto I = m_PermLinks.begin(); I != m_PermLinks.end(); ++I)
    {
        std::shared_ptr<const SLink> link = *I;

        const ULONG src_len = (ULONG)link->src.length();

        if (Path.length() >= src_len && _wcsnicmp(Path.c_str(), link->src.c_str(), src_len) == 0) {

            OutLink = link;
            break;
        }
    }
}

std::shared_ptr<CNtPathMgr::SLink> CNtPathMgr::FindPermLinksForMatchPath(const std::wstring& name, bool bReverse)
{
    std::unique_lock lock(m_Mutex);

    for( auto I = m_PermLinks.begin(); I != m_PermLinks.end(); ++I)
    {
        std::shared_ptr<CNtPathMgr::SLink> link = *I;

        const ULONG dst_len = (ULONG)(bReverse ? link->src : link->dst).length();

        if (name.length() >= dst_len && (name[dst_len] == L'\\' || name[dst_len] == L'\0') && _wcsnicmp(name.c_str(), (bReverse ? link->src : link->dst).c_str(), dst_len) == 0)
            return link;
    }

    return NULL;
}

std::wstring CNtPathMgr::FixPermLinksForMatchPath(const std::wstring& name)
{
    std::shared_ptr<CNtPathMgr::SLink> link = FindPermLinksForMatchPath(name, false);
    if (link)
    {
        const ULONG dst_len = (ULONG)link->dst.length();

        return link->src + name.substr(dst_len);
    }
    
    return L"";
}

////////////////////////////////////////////////////////////////////////////////////////////
// Guid Links

std::shared_ptr<const CNtPathMgr::SGuid> CNtPathMgr::GetGuidForPath(const std::wstring& Path)
{
    std::unique_lock lock(m_Mutex);

    for(auto I = m_GuidLinks.begin(); I != m_GuidLinks.end(); ++I)
    {
        std::shared_ptr<CNtPathMgr::SGuid> guid = *I;

        if (Path.length() >= guid->path.length() && _wcsnicmp(Path.c_str(), guid->path.c_str(), guid->path.length()) == 0) {

            const WCHAR *ptr = Path.c_str() + guid->path.length();
            if (*ptr == L'\\' || *ptr == L'\0')
                return guid;
        }
    }

    return NULL;
}


std::shared_ptr<const CNtPathMgr::SGuid> CNtPathMgr::GetLinkForGuid(const std::wstring& guid_str)
{
    std::unique_lock lock(m_Mutex);

    for(auto I = m_GuidLinks.begin(); I != m_GuidLinks.end(); ++I)
    {
        std::shared_ptr<CNtPathMgr::SGuid> guid = *I;

        if (_wcsnicmp(guid->guid, guid_str.c_str(), 38) == 0)
            return guid;
    }

    return NULL;
}

std::wstring CNtPathMgr::TranslateGuidToNtPath(const std::wstring& GuidPath)
{
    //
    // guid path L" \\??\\Volume{...}\..."
    //

    if (GuidPath.length() >= 48 && _wcsnicmp(GuidPath.c_str(), L"\\??\\Volume{", 11) == 0) 
    {
        std::shared_ptr<const CNtPathMgr::SGuid> guid = GetLinkForGuid(&GuidPath[10]);
        if (guid)
            return guid->path + GuidPath.substr(48);
    }

    return L"";
}

////////////////////////////////////////////////////////////////////////////////////////////
// 

std::wstring CNtPathMgr::TranslateDosToNtPath(const std::wstring& DosPath, bool bAsIsOnError)
{
    //
    // network path L"\\..."
    //

    if (DosPath.size() >= 2 && DosPath[0] == L'\\' && DosPath[1] == L'\\')
    {
        return File_Mup + DosPath.substr(2);
    } 
    
    //
    // drive-letter path L"X:\..."
    //

    if (DosPath.size() >= 3 && DosPath[1] == L':' && (DosPath[2] == L'\\' || DosPath[2] == L'\0'))
    {
        std::shared_ptr<const SDrive> drive = GetDriveForLetter(DosPath[0]);
        if (drive)
            return drive->path + DosPath.substr(2);
    }

    return bAsIsOnError ? DosPath : L"";
}

std::wstring CNtPathMgr::TranslateNtToDosPath(const std::wstring& NtPath, bool bAsIsOnError)
{
    // 
    // sometimes we get a DOS path with the \??\ prefix
    // in such cases we just quickly strip it and are done
    // 

    if (_wcsnicmp(NtPath.c_str(), L"\\??\\", 4) == 0)
        return NtPath.substr(4);
    if (_wcsnicmp(NtPath.c_str(), L"\\\\?\\", 4) == 0)
        return NtPath.substr(4);

    if (_wcsnicmp(NtPath.c_str(), File_Mup, File_MupLen) == 0)
        return L"\\\\" + NtPath.substr(File_MupLen);

    //
    // Find Dos Drive Letter
    //

    ULONG path_len = (ULONG)NtPath.length();
    ULONG prefix_len;
    std::shared_ptr<const SDrive> drive = GetDriveForPath(NtPath);
    if (drive)
        prefix_len = (ULONG)drive->path.length();
    else
        drive = GetDriveForUncPath(NtPath, &prefix_len);
    if (drive)
        return std::wstring(&drive->letter, 1) + L":" + NtPath.substr(prefix_len);

    if (!bAsIsOnError)
        return L"";

    // 
    // sometimes we have to use a path which has no drive letter
    // to make this work we use the \\.\ prefix which replaces \Device\
    // and is accepted by regular non NT Win32 APIs
    // 

    if (_wcsnicmp(NtPath.c_str(), L"\\Device\\", 8) == 0)
        return L"\\\\.\\" + NtPath.substr(8);

    return NtPath;
}
