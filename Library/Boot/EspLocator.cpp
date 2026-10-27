/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

#include "pch.h"
#include "EspLocator.h"

#include <windows.h>

//
// phnt_windows.h turns INITGUID on, so by the time anything here is compiled
// DEFINE_GUID emits definitions rather than declarations - and the device
// interface GUIDs winioctl.h carries are already defined elsewhere in this
// binary. Borrowing the declaring form for the length of the include leaves
// them as externs, which is what this file wants anyway: it uses the IOCTLs
// and the partition structures, not the interface GUIDs.
//
#pragma push_macro("DEFINE_GUID")
#undef DEFINE_GUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)     EXTERN_C const GUID name
#include <winioctl.h>
#pragma pop_macro("DEFINE_GUID")

//
// c12a7328-f81f-11d2-ba4b-00a0c93ec93b, the GPT type GUID every EFI System
// Partition carries.
//
static const GUID g_EfiSystemPartition =
	{ 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };

// 128 partitions is more than any real disk has and keeps the layout query on
// the stack in one piece
#define MAX_PARTITIONS  128

static std::wstring PhysicalDrivePath(int iDisk)
{
	wchar_t Path[64];
	swprintf_s(Path, L"\\\\.\\PhysicalDrive%d", iDisk);
	return Path;
}

//
// The drive layout, read once and handed around. The buffer has to be sized for
// the worst case up front because the IOCTL has no way to report how much it
// wanted.
//
static bool GetDriveLayout(int iDisk, std::vector<unsigned char>& Buffer)
{
	Buffer.resize(sizeof(DRIVE_LAYOUT_INFORMATION_EX) + sizeof(PARTITION_INFORMATION_EX) * MAX_PARTITIONS);

	HANDLE hDisk = CreateFileW(PhysicalDrivePath(iDisk).c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hDisk == INVALID_HANDLE_VALUE)
		return false;

	DWORD uBytes = 0;
	BOOL bOk = DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0,
		Buffer.data(), (DWORD)Buffer.size(), &uBytes, NULL);

	CloseHandle(hDisk);
	return bOk != FALSE;
}

int CEspLocator::GetOsDisk()
{
	//
	// The volume holding the Windows directory, resolved to the physical drive
	// behind it. GetSystemDirectory rather than the boot device path, because
	// that is the disk the user means by "this machine's disk".
	//
	wchar_t SysDir[MAX_PATH] = { 0 };
	if (!GetSystemDirectoryW(SysDir, MAX_PATH))
		return -1;

	wchar_t VolumePath[MAX_PATH];
	swprintf_s(VolumePath, L"\\\\?\\%c:", SysDir[0]);

	HANDLE hVolume = CreateFileW(VolumePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, NULL);
	if (hVolume == INVALID_HANDLE_VALUE)
		return -1;

	STORAGE_DEVICE_NUMBER sdn = { 0 };
	DWORD uBytes = 0;
	int iDisk = -1;
	if (DeviceIoControl(hVolume, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &uBytes, NULL))
		iDisk = (int)sdn.DeviceNumber;

	CloseHandle(hVolume);
	return iDisk;
}

std::wstring CEspLocator::GetVolumeGuidPath(int iDisk, int iPartition)
{
	wchar_t VolumeName[MAX_PATH];
	HANDLE hFind = FindFirstVolumeW(VolumeName, MAX_PATH);
	if (hFind == INVALID_HANDLE_VALUE)
		return std::wstring();

	std::wstring Found;
	do
	{
		// CreateFile will not take the trailing backslash a volume name carries
		size_t uLen = wcslen(VolumeName);
		if (uLen > 0 && VolumeName[uLen - 1] == L'\\')
			VolumeName[uLen - 1] = L'\0';

		HANDLE hVolume = CreateFileW(VolumeName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, 0, NULL);
		if (hVolume != INVALID_HANDLE_VALUE)
		{
			STORAGE_DEVICE_NUMBER sdn = { 0 };
			DWORD uBytes = 0;
			if (DeviceIoControl(hVolume, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &uBytes, NULL)) {
				if ((int)sdn.DeviceNumber == iDisk && (int)sdn.PartitionNumber == iPartition)
					Found = VolumeName;
			}
			CloseHandle(hVolume);
		}

		if (!Found.empty())
			break;

	} while (FindNextVolumeW(hFind, VolumeName, MAX_PATH));

	FindVolumeClose(hFind);
	return Found;
}

std::wstring CEspLocator::GetGlobalRootPath(int iDisk, int iPartition)
{
	wchar_t Path[MAX_PATH];
	swprintf_s(Path, L"\\\\?\\GLOBALROOT\\Device\\Harddisk%d\\Partition%d", iDisk, iPartition);
	return Path;
}

bool CEspLocator::IsGptDisk(int iDisk)
{
	std::vector<unsigned char> Buffer;
	if (!GetDriveLayout(iDisk, Buffer))
		return false;

	PDRIVE_LAYOUT_INFORMATION_EX pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)Buffer.data();
	return pLayout->PartitionStyle == PARTITION_STYLE_GPT;
}

std::vector<SDiskInfo> CEspLocator::EnumDisks()
{
	std::vector<SDiskInfo> Disks;
	int iOsDisk = GetOsDisk();

	//
	// Probing by number rather than walking the device interface: a machine
	// with a gap in its drive numbers is possible but rare, so the scan runs a
	// little past the last disk found instead of stopping at the first miss.
	//
	int iMisses = 0;
	for (int i = 0; i < 64 && iMisses < 8; i++)
	{
		HANDLE hDisk = CreateFileW(PhysicalDrivePath(i).c_str(), 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hDisk == INVALID_HANDLE_VALUE) {
			iMisses++;
			continue;
		}
		iMisses = 0;

		SDiskInfo Info;
		Info.iDisk = i;
		Info.bIsOsDisk = (i == iOsDisk);

		DWORD uBytes = 0;
		GET_LENGTH_INFORMATION Length = { 0 };
		if (DeviceIoControl(hDisk, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &Length, sizeof(Length), &uBytes, NULL))
			Info.uSize = (uint64)Length.Length.QuadPart;

		//
		// The model string, out of the storage descriptor. Worth the extra
		// query: a disk picker that says "Disk 0" and "Disk 1" is no help when
		// the wrong choice rewrites a partition table.
		//
		STORAGE_PROPERTY_QUERY Query = { StorageDeviceProperty, PropertyStandardQuery };
		std::vector<unsigned char> Desc(1024);
		if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &Query, sizeof(Query),
			Desc.data(), (DWORD)Desc.size(), &uBytes, NULL))
		{
			PSTORAGE_DEVICE_DESCRIPTOR pDesc = (PSTORAGE_DEVICE_DESCRIPTOR)Desc.data();
			std::string Name;
			if (pDesc->VendorIdOffset && pDesc->VendorIdOffset < uBytes)
				Name += (const char*)(Desc.data() + pDesc->VendorIdOffset);
			if (pDesc->ProductIdOffset && pDesc->ProductIdOffset < uBytes) {
				if (!Name.empty() && Name.back() != ' ') Name += " ";
				Name += (const char*)(Desc.data() + pDesc->ProductIdOffset);
			}
			// trim, the fields are space padded
			while (!Name.empty() && Name.back() == ' ') Name.pop_back();
			size_t uStart = Name.find_first_not_of(' ');
			if (uStart != std::string::npos) Name = Name.substr(uStart);

			Info.Name.assign(Name.begin(), Name.end());
		}

		CloseHandle(hDisk);

		Info.bGpt = IsGptDisk(i);
		Info.iEspCount = (int)EnumEsps(i).size();

		Disks.push_back(Info);
	}

	return Disks;
}

std::vector<SEspInfo> CEspLocator::EnumEsps(int iDisk)
{
	std::vector<SEspInfo> Esps;

	std::vector<unsigned char> Buffer;
	if (!GetDriveLayout(iDisk, Buffer))
		return Esps;

	PDRIVE_LAYOUT_INFORMATION_EX pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)Buffer.data();

	for (DWORD i = 0; i < pLayout->PartitionCount; i++)
	{
		PPARTITION_INFORMATION_EX pPart = &pLayout->PartitionEntry[i];

		bool bGpt = pLayout->PartitionStyle == PARTITION_STYLE_GPT;
		if (bGpt) {
			if (!IsEqualGUID(pPart->Gpt.PartitionType, g_EfiSystemPartition))
				continue;
		}
		else if (pLayout->PartitionStyle == PARTITION_STYLE_MBR) {
			//
			// UEFI does not boot an MBR disk, but removable media formatted
			// this way turns up, so a FAT partition carrying an \EFI directory
			// is accepted as one.
			//
			if (pPart->Mbr.PartitionType != PARTITION_FAT32
				&& pPart->Mbr.PartitionType != PARTITION_FAT32_XINT13
				&& pPart->Mbr.PartitionType != PARTITION_XINT13)
				continue;
		}
		else
			continue;

		SEspInfo Info;
		Info.iDisk = iDisk;
		Info.iPartition = pPart->PartitionNumber;
		Info.uSize = (uint64)pPart->PartitionLength.QuadPart;
		Info.uStartOffset = (uint64)pPart->StartingOffset.QuadPart;
		Info.bGpt = bGpt;

		if (bGpt)
			memcpy(Info.PartitionGuid, &pPart->Gpt.PartitionId, 16);

		Info.Path = GetVolumeGuidPath(iDisk, pPart->PartitionNumber);
		if (Info.Path.empty())
			Info.Path = GetGlobalRootPath(iDisk, pPart->PartitionNumber);

		if (!bGpt && !FileExists(Info.Path, L"\\EFI"))
			continue;	// a FAT partition that is not a boot partition

		Info.bHasDcs = FileExists(Info.Path, L"\\EFI\\DCS");
		Info.bHasMicrosoft = FileExists(Info.Path, L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi");

		//
		// Free space and label, best effort. An ESP has no drive letter, so
		// these only answer through the volume GUID path - the GLOBALROOT
		// fallback gives nothing, which is why it is only the fallback.
		//
		std::wstring VolumeRoot = Info.Path + L"\\";
		ULARGE_INTEGER FreeBytes = { 0 }, TotalBytes = { 0 };
		if (GetDiskFreeSpaceExW(VolumeRoot.c_str(), &FreeBytes, &TotalBytes, NULL))
			Info.uFree = FreeBytes.QuadPart;

		wchar_t Label[MAX_PATH + 1] = { 0 };
		if (GetVolumeInformationW(VolumeRoot.c_str(), Label, MAX_PATH, NULL, NULL, NULL, NULL, 0))
			Info.Label = Label;

		Info.MountPoint = GetMountPoint(iDisk, pPart->PartitionNumber);

		Esps.push_back(Info);
	}

	return Esps;
}

/* ---------------------------------------------------------------------------
 * Drive letters
 * ------------------------------------------------------------------------ */

std::wstring CEspLocator::GetMountPoint(int iDisk, int iPartition)
{
	std::wstring Volume = GetVolumeGuidPath(iDisk, iPartition);
	if (Volume.empty())
		return std::wstring();
	if (Volume.back() != L'\\')
		Volume += L'\\';

	//
	// A volume can have several paths - drive letters and mounted folders
	// alike - so the list is walked for one that is a drive letter. An ESP
	// usually has none at all and this comes back empty.
	//
	std::vector<wchar_t> Names(512);
	DWORD uLen = 0;
	if (!GetVolumePathNamesForVolumeNameW(Volume.c_str(), Names.data(), (DWORD)Names.size(), &uLen))
	{
		if (GetLastError() != ERROR_MORE_DATA)
			return std::wstring();
		Names.resize(uLen);
		if (!GetVolumePathNamesForVolumeNameW(Volume.c_str(), Names.data(), (DWORD)Names.size(), &uLen))
			return std::wstring();
	}

	for (const wchar_t* p = Names.data(); *p; p += wcslen(p) + 1) {
		if (wcslen(p) >= 2 && p[1] == L':')
			return std::wstring(p, 2);
	}

	return std::wstring();
}

STATUS CEspLocator::Mount(int iDisk, int iPartition, const std::wstring& DriveLetter)
{
	if (DriveLetter.size() < 2 || DriveLetter[1] != L':')
		return ERR(STATUS_INVALID_PARAMETER, L"\"" + DriveLetter + L"\" is not a drive letter.");

	std::wstring Volume = GetVolumeGuidPath(iDisk, iPartition);
	if (Volume.empty()) {
		return ERR(STATUS_NOT_FOUND,
			L"The partition has no volume name, so Windows cannot give it a drive letter.");
	}
	if (Volume.back() != L'\\')
		Volume += L'\\';

	// SetVolumeMountPoint wants both paths to end in a backslash
	std::wstring Root = DriveLetter.substr(0, 2) + L"\\";

	if (!SetVolumeMountPointW(Root.c_str(), Volume.c_str())) {
		DWORD uError = GetLastError();
		return ERR(STATUS_UNSUCCESSFUL, L"Could not assign " + DriveLetter
			+ L" to the partition (error " + std::to_wstring(uError) + L").");
	}

	return OK;
}

STATUS CEspLocator::Unmount(const std::wstring& DriveLetter)
{
	if (DriveLetter.size() < 2 || DriveLetter[1] != L':')
		return ERR(STATUS_INVALID_PARAMETER, L"\"" + DriveLetter + L"\" is not a drive letter.");

	std::wstring Root = DriveLetter.substr(0, 2) + L"\\";

	if (!DeleteVolumeMountPointW(Root.c_str())) {
		DWORD uError = GetLastError();
		return ERR(STATUS_UNSUCCESSFUL, L"Could not remove the drive letter " + DriveLetter
			+ L" (error " + std::to_wstring(uError) + L").");
	}

	return OK;
}

std::vector<std::wstring> CEspLocator::GetFreeDriveLetters()
{
	std::vector<std::wstring> Letters;

	DWORD uMask = GetLogicalDrives();
	for (wchar_t c = L'D'; c <= L'Z'; c++) {
		if (!(uMask & (1u << (c - L'A'))))
			Letters.push_back(std::wstring(1, c) + L":");
	}

	return Letters;
}

RESULT(SEspInfo) CEspLocator::FindEsp(int iDisk, int iPartition)
{
	if (iDisk < 0) {
		iDisk = GetOsDisk();
		if (iDisk < 0)
			return ERR(STATUS_NOT_FOUND, L"Could not work out which disk Windows booted from.");
	}

	std::vector<SEspInfo> Esps = EnumEsps(iDisk);
	if (Esps.empty())
		return ERR(STATUS_NOT_FOUND, L"No EFI System Partition was found on this disk.");

	if (iPartition > 0) {
		for (size_t i = 0; i < Esps.size(); i++) {
			if (Esps[i].iPartition == iPartition)
				return CResult<SEspInfo>(0, Esps[i]);
		}
		return ERR(STATUS_NOT_FOUND, L"The requested partition is not an EFI System Partition.");
	}

	//
	// One that already has DCS on it wins, so that a second run works on what
	// the first one installed rather than on whichever ESP happens to be first.
	//
	for (size_t i = 0; i < Esps.size(); i++) {
		if (Esps[i].bHasDcs)
			return CResult<SEspInfo>(0, Esps[i]);
	}

	return CResult<SEspInfo>(0, Esps[0]);
}

/* ---------------------------------------------------------------------------
 * Files
 * ------------------------------------------------------------------------ */

bool CEspLocator::FileExists(const std::wstring& Root, const std::wstring& Name)
{
	return GetFileAttributesW((Root + Name).c_str()) != INVALID_FILE_ATTRIBUTES;
}

STATUS CEspLocator::ReadFile(const std::wstring& Root, const std::wstring& Name, std::vector<unsigned char>& Data)
{
	std::wstring Path = Root + Name;

	HANDLE hFile = CreateFileW(Path.c_str(), GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return ERR(STATUS_NOT_FOUND, L"Could not open " + Name + L" on the EFI partition.");

	LARGE_INTEGER Size = { 0 };
	if (!GetFileSizeEx(hFile, &Size) || Size.QuadPart > 64 * 1024 * 1024) {
		CloseHandle(hFile);
		return ERR(STATUS_FILE_TOO_LARGE, L"Unexpected size for " + Name + L".");
	}

	Data.resize((size_t)Size.QuadPart);

	DWORD uRead = 0;
	BOOL bOk = Data.empty() ? TRUE : ::ReadFile(hFile, Data.data(), (DWORD)Data.size(), &uRead, NULL);
	CloseHandle(hFile);

	if (!bOk || uRead != Data.size())
		return ERR(STATUS_UNSUCCESSFUL, L"Could not read " + Name + L" from the EFI partition.");
	return OK;
}

STATUS CEspLocator::WriteFile(const std::wstring& Root, const std::wstring& Name, const void* pData, size_t uSize)
{
	std::wstring Path = Root + Name;

	//
	// Everything above the file has to exist first. The EFI world nests
	// \EFI\DCS and \EFI\Boot, and neither is there on a fresh ESP.
	//
	size_t uSlash = Name.find_last_of(L'\\');
	if (uSlash != std::wstring::npos && uSlash > 0) {
		STATUS Status = MakeDir(Root, Name.substr(0, uSlash));
		if (Status.IsError())
			return Status;
	}

	HANDLE hFile = CreateFileW(Path.c_str(), GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return ERR(STATUS_ACCESS_DENIED, L"Could not create " + Name + L" on the EFI partition.");

	DWORD uWritten = 0;
	BOOL bOk = uSize == 0 ? TRUE : ::WriteFile(hFile, pData, (DWORD)uSize, &uWritten, NULL);

	// an EFI partition is written once and read at boot; a lost write here is
	// a machine that does not start, so it is flushed rather than trusted
	if (bOk) FlushFileBuffers(hFile);
	CloseHandle(hFile);

	if (!bOk || uWritten != uSize)
		return ERR(STATUS_UNSUCCESSFUL, L"Could not write " + Name + L" to the EFI partition.");
	return OK;
}

STATUS CEspLocator::RemoveFile(const std::wstring& Root, const std::wstring& Name)
{
	std::wstring Path = Root + Name;

	DWORD uAttr = GetFileAttributesW(Path.c_str());
	if (uAttr == INVALID_FILE_ATTRIBUTES)
		return OK;	// already gone

	// a boot file is commonly read-only or hidden
	if (uAttr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
		SetFileAttributesW(Path.c_str(), FILE_ATTRIBUTE_NORMAL);

	if (!::DeleteFileW(Path.c_str()))
		return ERR(STATUS_ACCESS_DENIED, L"Could not delete " + Name + L" from the EFI partition.");
	return OK;
}

STATUS CEspLocator::RenameFile(const std::wstring& Root, const std::wstring& From, const std::wstring& To)
{
	RemoveFile(Root, To);

	if (!MoveFileW((Root + From).c_str(), (Root + To).c_str()))
		return ERR(STATUS_UNSUCCESSFUL, L"Could not rename " + From + L" on the EFI partition.");
	return OK;
}

STATUS CEspLocator::CopyFileTo(const std::wstring& Root, const std::wstring& From, const std::wstring& To)
{
	std::vector<unsigned char> Data;
	STATUS Status = ReadFile(Root, From, Data);
	if (Status.IsError())
		return Status;
	return WriteFile(Root, To, Data.data(), Data.size());
}

STATUS CEspLocator::MakeDir(const std::wstring& Root, const std::wstring& Name)
{
	//
	// Created a level at a time: CreateDirectory will not make \EFI\DCS when
	// \EFI is not there yet, and on a blank ESP neither is.
	//
	std::wstring Path = Root;
	size_t uPos = 0;

	while (uPos < Name.size())
	{
		size_t uNext = Name.find(L'\\', uPos + 1);
		if (uNext == std::wstring::npos)
			uNext = Name.size();

		Path += Name.substr(uPos, uNext - uPos);
		uPos = uNext;

		if (!CreateDirectoryW(Path.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
			return ERR(STATUS_ACCESS_DENIED, L"Could not create the directory " + Name + L" on the EFI partition.");
	}

	return OK;
}

STATUS CEspLocator::DeleteDir(const std::wstring& Root, const std::wstring& Name, bool bRecursive)
{
	std::wstring Dir = Root + Name;

	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW((Dir + L"\\*").c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do {
			if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				continue;

			std::wstring Child = Name + L"\\" + fd.cFileName;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (bRecursive)
					DeleteDir(Root, Child, true);
			}
			else
						RemoveFile(Root, Child);

		} while (FindNextFileW(hFind, &fd));

		FindClose(hFind);
	}

	if (!RemoveDirectoryW(Dir.c_str()) && GetFileAttributesW(Dir.c_str()) != INVALID_FILE_ATTRIBUTES)
		return ERR(STATUS_ACCESS_DENIED, L"Could not remove the directory " + Name + L" from the EFI partition.");
	return OK;
}

STATUS CEspLocator::CopyFromFolder(const std::wstring& Folder, const std::wstring& Source,
								   const std::wstring& Root, const std::wstring& Target)
{
	std::wstring From = Folder;
	if (!From.empty() && From.back() != L'\\')
		From += L"\\";
	From += Source;

	HANDLE hFile = CreateFileW(From.c_str(), GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return ERR(STATUS_NOT_FOUND, L"The boot loader package is missing " + Source + L".");

	LARGE_INTEGER Size = { 0 };
	if (!GetFileSizeEx(hFile, &Size) || Size.QuadPart > 64 * 1024 * 1024) {
		CloseHandle(hFile);
		return ERR(STATUS_FILE_TOO_LARGE, L"Unexpected size for " + Source + L".");
	}

	std::vector<unsigned char> Data((size_t)Size.QuadPart);
	DWORD uRead = 0;
	BOOL bOk = Data.empty() ? TRUE : ::ReadFile(hFile, Data.data(), (DWORD)Data.size(), &uRead, NULL);
	CloseHandle(hFile);

	if (!bOk || uRead != Data.size())
		return ERR(STATUS_UNSUCCESSFUL, L"Could not read " + Source + L" from the boot loader package.");

	return WriteFile(Root, Target, Data.data(), Data.size());
}
