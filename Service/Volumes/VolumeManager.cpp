#include "pch.h"

#include "ServiceCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "VolumeManager.h"
#include "../../Library/Common/Strings.h"
#include "../../ImBox/ImBox.h"
#include "../Access/AccessManager.h"
#include "../../Library/API/DriverAPI.h"
#include "../../Library/Helpers/NtUtil.h"

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

LONG LogEx(
    ULONG session_id,
    ULONG msgid,
    const WCHAR* format, ...)
{
    return 0;
}

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

std::shared_ptr<CVolume> FindImDisk(const std::wstring& ImageFile, ULONG session_id)
{
    std::wstring TargetNtPath;

    std::wstring ProxyName = GetProxyName(ImageFile);

    //
    // Find an already mounted RamDisk,
    // we inspect the volume label to determine if its ours
    // 

    std::vector<ULONG> DeviceList;
    DeviceList.resize(3);

retry:
    if (!ImDiskGetDeviceListEx((ULONG)DeviceList.size(), &DeviceList.front())) {
        switch (GetLastError())
        {
        case ERROR_FILE_NOT_FOUND:
            LogEx(session_id, 2232, L"");
            return NULL;

        case ERROR_MORE_DATA:
            DeviceList.resize(DeviceList[0] + 1);
            goto retry;

        default:
            LogEx(session_id, 2233, L"%d", GetLastError());
            return NULL;
        }
    }

    for (ULONG counter = 1; counter <= DeviceList[0]; counter++) {

        std::wstring proxy = ImDiskQueryDeviceProxy(IMDISK_DEVICE + std::to_wstring(DeviceList[counter]));
        if (_wcsnicmp(proxy.c_str(), L"\\BaseNamedObjects\\Global\\" IMBOX_PROXY, 25 + 11) != 0)
            continue;
        std::size_t pos = proxy.find_first_of(L'!');
        if (pos == std::wstring::npos || _wcsicmp(proxy.c_str() + (pos + 1), ProxyName.c_str()) != 0)
            continue;

        //if (GetVolumeLabel(IMDISK_DEVICE + std::to_wstring(DeviceList[counter]) + L"\\") != DISK_LABEL) 
        //  continue;

        TargetNtPath = IMDISK_DEVICE + std::to_wstring(DeviceList[counter]);
        break;
    }

    std::shared_ptr<CVolume> pMount;
    if (!TargetNtPath.empty()) {
        pMount = std::make_shared<CVolume>();
        pMount->m_DevicePath = TargetNtPath;
        pMount->m_ImageDosPath = ImageFile;
        pMount->m_VolumeSize = ImDiskQueryDeviceSize(pMount->m_DevicePath);
        pMount->m_MountPoint = ImDiskQueryDriveLetter(pMount->m_DevicePath) + std::wstring(L":\\");
    }
    return pMount;
}

std::shared_ptr<CVolume> MountImDisk(const std::wstring& ImageFile, const wchar_t* pPassword, ULONG64 sizeKb, ULONG session_id, const std::wstring& MountPoint = L"")
{
    bool ok = false;

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
    if (Drive[0] == 0) {
        LogEx(session_id, 2234, L"");
        return NULL;
    }

    std::wstring cmd;
    if (ImageFile.empty()) cmd = L"ImBox type=ram";
    else cmd = L"ImBox type=img image=\"" + ImageFile + L"\"";
    if (pPassword && *pPassword) cmd += L" cipher=AES";
    //cmd += L" size=" + std::to_wstring(sizeKb) + L" mount=" + std::wstring(Drive) + L" format=ntfs:" DISK_LABEL;
    cmd += L" size=" + std::to_wstring(sizeKb) + L" mount=" + std::wstring(Drive) + L" format=ntfs";

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

    SSection* pSection = NULL;
    HANDLE hMapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x1000, (IMBOX_SECTION + std::wstring(sName)).c_str());
    if (hMapping) {
        pSection = (SSection*)MapViewOfFile(hMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0x1000);
        memset(pSection, 0, 0x1000);
    }
    cmd += L" section=" IMBOX_SECTION + std::wstring(sName);

    if (pPassword && *pPassword)
        wmemcpy(pSection->in.pass, pPassword, wcslen(pPassword) + 1);


    std::wstring app = theCore->GetAppDir() + L"\\ImBox.exe";
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi = { 0 };
    if (CreateProcess(app.c_str(), (WCHAR*)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {

        //
        // Wait for ImDisk to be mounted
        //

        HANDLE hEvents[] = { hEvent, pi.hProcess };
        DWORD dwEvent = WaitForMultipleObjects(ARRAYSIZE(hEvents), hEvents, FALSE, 40 * 1000);
        if (dwEvent != WAIT_OBJECT_0) {
            DWORD ret;
            GetExitCodeProcess(pi.hProcess, &ret);
            if (ret == STILL_ACTIVE) {
                LogEx(session_id, 2240, L"%S", Drive);
                //TerminateProcess(pi.hProcess, -1);
            }
            else {
                switch (ret) {
                case ERR_FILE_NOT_OPENED:   LogEx(session_id, 2241, L"%S", ImageFile.c_str()); break;
                case ERR_UNKNOWN_CIPHER:    LogEx(session_id, 2242, L""); break;
                case ERR_WRONG_PASSWORD:    LogEx(session_id, 2243, L""); break;
                case ERR_KEY_REQUIRED:      LogEx(session_id, 2244, L""); break;
                case ERR_IMDISK_FAILED:     LogEx(session_id, 2236, L"ImDisk"); break;
                case ERR_IMDISK_TIMEOUT:    LogEx(session_id, 2240, L"%S", Drive); break;
                default:                    LogEx(session_id, 2246, L"%d", ret); break;
                }
            }
        }
        else {

            if(_wcsnicmp(pSection->out.mount, IMDISK_DEVICE, IMDISK_DEVICE_LEN) == 0)
                pMount->m_DevicePath = std::wstring(pSection->out.mount);
            else { // fallback
                HANDLE handle = ImDiskOpenDeviceByMountPoint(Drive, 0);
                if (handle != INVALID_HANDLE_VALUE) {
                    BYTE buffer[MAX_PATH];
                    DWORD length = sizeof(buffer);
                    if (NT_SUCCESS(NtQueryObject(handle, ObjectNameInformation, buffer, length, &length))) {
                        UNICODE_STRING* uni = &((OBJECT_NAME_INFORMATION*)buffer)->Name;
                        length = uni->Length / sizeof(WCHAR);
                        if (uni->Buffer) {
                            uni->Buffer[length] = 0;
                            pMount->m_DevicePath = uni->Buffer;
                        }
                    }
                    CloseHandle(handle);
                }
            }

            if (!pMount->m_DevicePath.empty()) {

                ok = true;

                if (!drvLetter) {
                    if (!DefineDosDevice(DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE | DDD_RAW_TARGET_PATH, Drive, pMount->m_DevicePath.c_str())) {
                        LogEx(session_id, 2235, L"%S", Drive);
                    }
                }
            }
        }

        if(ok)
            pMount->m_ProcessHandle = pi.hProcess;
        else
            CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else
        LogEx(session_id, 2236, L"ImBox");

    if (pSection) {
        memset(pSection, 0, 0x1000);
        UnmapViewOfFile(pSection);
    }
    if(hMapping) 
        CloseHandle(hMapping);

    if(hEvent) 
        CloseHandle(hEvent);

    if (!ok)
        return NULL;
    pMount->m_ImageDosPath = ImageFile;
    pMount->m_VolumeSize = ImDiskQueryDeviceSize(pMount->m_DevicePath);
    pMount->m_MountPoint = MountPoint;
    return pMount;
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

    //HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, (IMBOX_EVENT + std::wstring(sName)).c_str());
    //cmd += L" event=" IMBOX_EVENT + std::wstring(sName);

    SSection* pSection = NULL;
    HANDLE hMapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x1000, (IMBOX_SECTION + std::wstring(sName)).c_str());
    if (hMapping) {
        pSection = (SSection*)MapViewOfFile(hMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0x1000);
        memset(pSection, 0, 0x1000);
    }
    cmd += L" section=" IMBOX_SECTION + std::wstring(sName);

    //if (pPassword && *pPassword)
    wmemcpy(pSection->in.pass, pPassword, wcslen(pPassword) + 1);

    if (pBuffer && bWrite)
    {
        pSection->magic = SECTION_MAGIC;
        pSection->id = uId;
        pSection->size = (USHORT)pBuffer->GetSize();
        memcpy(pSection->data, pBuffer->GetBuffer(), pSection->size);
    }

    std::wstring app = theCore->GetAppDir() + L"\\ImBox.exe";
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi = { 0 };
    if (CreateProcess(app.c_str(), (WCHAR*)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {

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
                if (pSection->magic == SECTION_MAGIC && pSection->id == uId) {
                    pBuffer->SetSize(pSection->size, true);
                    memcpy(pBuffer->GetBuffer(), pSection->data, pSection->size);
                }
            }
            Status = OK;
        }
		
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    if (pSection) {
        memset(pSection, 0, 0x1000);
        UnmapViewOfFile(pSection);
    }
    if(hMapping) 
        CloseHandle(hMapping);

    //if(hEvent) 
    //    CloseHandle(hEvent);

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
            LogEx(-1, 2232, L"");
            return STATUS_DEVICE_NOT_READY;

        case ERROR_MORE_DATA:
            DeviceList.resize(DeviceList[0] + 1);
            goto retry;

        default:
            LogEx(-1, 2233, L"%d", GetLastError());
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
        if (_wcsnicmp(proxy.c_str(), L"\\BaseNamedObjects\\Global\\" IMBOX_PROXY, 25 + 11) != 0)
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

    return STATUS_SUCCESS;
}

STATUS CVolumeManager::CreateImage(const std::wstring& Path, const std::wstring& Password, uint64 uSize, const std::wstring& Cipher)
{
    // todo: Cipher
    std::shared_ptr<CVolume> pMount = MountImDisk(Path, Password.c_str(), uSize, -1);
    if(!pMount)
        return STATUS_UNSUCCESSFUL;

    UnmountImDisk(pMount->m_DevicePath, pMount->m_ProcessHandle);
    return STATUS_SUCCESS;
}

STATUS CVolumeManager::ChangeImagePassword(const std::wstring& Path, const std::wstring& OldPassword, const std::wstring& NewPassword)
{
    CBuffer Data(1024);
    Data.WriteData(NewPassword.c_str(), NewPassword.length() * sizeof(wchar_t));
    ExecImDisk(Path, OldPassword.c_str(), L"new_key", true, &Data, SECTION_PARAM_ID_KEY);
	return STATUS_NOT_IMPLEMENTED;
}

//STATUS CVolumeManager::DeleteImage(const std::wstring& Path)
//{
//	return STATUS_NOT_IMPLEMENTED;
//}

STATUS CVolumeManager::MountImage(const std::wstring& Path, const std::wstring& MountPoint, const std::wstring& Password)
{
    std::unique_lock Lock(m_Mutex);

    auto F = m_VolumesByPath.find(MkLower(Path));
    if(F != m_VolumesByPath.end())
		return STATUS_ALREADY_COMPLETE;

    auto pMount = MountImDisk(Path, Password.c_str(), 0, -1, MountPoint.c_str());
    if(!pMount)
        return STATUS_ERR_WRONG_PASSWORD;

    STATUS Status = ProtectVolume(Path, pMount->m_DevicePath);
    //STATUS Status = theCore->Driver()->SetupRuleAlias(DosPathToNtPath(Path), pMount->m_DevicePath); 
    if (Status.IsError()) { // if we fail to set up protection, unmount the volume, TODO: fix-me: change to first seting up protection, then mounting to a pre defined device path!!!
        if(UnmountImDisk(pMount->m_DevicePath, pMount->m_ProcessHandle))
            return Status; 
        // else // if we should fail to unmount it leave it in place and report the error
    }

    m_Volumes[MkLower(pMount->m_DevicePath)] = pMount;
    m_VolumesByPath[MkLower(pMount->m_ImageDosPath)] = pMount;
    return Status;
}

STATUS CVolumeManager::DismountVolume(const std::wstring& DevicePath)
{
    std::unique_lock Lock(m_Mutex);

	auto F = m_Volumes.find(MkLower(DevicePath));
	if(F == m_Volumes.end())
		return STATUS_NOT_FOUND;

	if(!UnmountImDisk(F->second->m_DevicePath, F->second->m_ProcessHandle))
		return STATUS_UNSUCCESSFUL;

    UnProtectVolume(F->second->m_DevicePath);
    //theCore->Driver()->ClearRuleAlias(F->second->m_DevicePath);

	m_VolumesByPath.erase(MkLower(F->second->m_ImageDosPath));
	m_Volumes.erase(F);
	return STATUS_SUCCESS;
}

STATUS CVolumeManager::DismountAll()
{
    std::unique_lock Lock(m_Mutex);

    for(auto I = m_Volumes.begin(); I != m_Volumes.end();)
	{
        if (!UnmountImDisk(I->second->m_DevicePath, I->second->m_ProcessHandle))
            I++;
        else 
        {
            UnProtectVolume(I->second->m_DevicePath);
            //theCore->Driver()->ClearRuleAlias(I->second->m_DevicePath);

            m_VolumesByPath.erase(MkLower(I->second->m_ImageDosPath));
            I = m_Volumes.erase(I);
        }
	}

    if (!m_Volumes.empty())
        return STATUS_UNSUCCESSFUL;
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
        if(pRule->IsTemporary() && RulePath.length() >= DevicePath.length() && _wcsnicmp(RulePath.c_str(), DevicePath.c_str(), DevicePath.length()) == 0)
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
        pClone->SetData(API_V_RULE_DATA_REF_GUID, CVariant(pRule->GetGuid()));
        pClone->SetAccessPath(AccessPath);
        return theCore->AccessManager()->AddRule(pClone);
    }

    return OK;
}

STATUS CVolumeManager::RemoveRule(const std::wstring& OriginGuid)
{
    auto Rules = theCore->AccessManager()->GetAllRules();
    for(auto I: Rules) 
    {
        auto pRule = I.second;
        std::wstring RulePath = pRule->GetAccessPath();
        if(pRule->IsTemporary() && pRule->GetData(API_V_RULE_DATA_REF_GUID).AsStr() == OriginGuid)
            theCore->AccessManager()->RemoveRule(pRule->GetGuid());
    }
    return OK;
}

void CVolumeManager::OnRuleChanged(const std::wstring& Guid, enum class ERuleEvent Event, enum class ERuleType Type)
{
    RemoveRule(Guid);

    if(Event != ERuleEvent::eRemoved)
	{
        auto pRule = theCore->AccessManager()->GetRule(Guid);
        if (pRule && pRule->IsEnabled() && pRule->GetAccessPath().find(L"/") != std::wstring::npos)
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
}