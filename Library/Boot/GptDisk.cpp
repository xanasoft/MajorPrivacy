/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "pch.h"
#include "GptDisk.h"

#include <windows.h>
#include <objbase.h>	// CoCreateGuid, for the new partition's identifier

// see EspLocator.cpp - phnt turns INITGUID on and winioctl.h's device
// interface GUIDs are already defined elsewhere in this binary
#pragma push_macro("DEFINE_GUID")
#undef DEFINE_GUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    EXTERN_C const GUID name
#include <winioctl.h>
#pragma pop_macro("DEFINE_GUID")

static const GUID g_EfiSystemPartition =
	{ 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };

#define MAX_PARTITIONS		128

//
// A GPT reserves 34 sectors at the front for its header and entry array, and 33
// at the back for the copy of both. Anything written into either is a corrupt
// partition table.
//
#define GPT_FRONT_SECTORS	34
#define GPT_BACK_SECTORS	33

// Partitions are aligned to a megabyte. Not a requirement, but an unaligned
// partition on a disk with 4K physical sectors reads and writes every block
// twice, and every other tool does the same thing.
#define ALIGNMENT			0x100000ULL
#define ALIGN_UP(x)			(((x) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define ALIGN_DOWN(x)		((x) & ~(ALIGNMENT - 1))

/* ---------------------------------------------------------------------------
 * Disk plumbing
 * ------------------------------------------------------------------------ */

static std::wstring PhysicalDrivePath(int iDisk)
{
	wchar_t Path[64];
	swprintf_s(Path, L"\\\\.\\PhysicalDrive%d", iDisk);
	return Path;
}

static HANDLE OpenDisk(int iDisk, bool bWrite)
{
	return CreateFileW(PhysicalDrivePath(iDisk).c_str(),
		bWrite ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
}

static bool ReadLayout(HANDLE hDisk, std::vector<unsigned char>& Buffer)
{
	Buffer.resize(sizeof(DRIVE_LAYOUT_INFORMATION_EX) + sizeof(PARTITION_INFORMATION_EX) * MAX_PARTITIONS);

	DWORD uBytes = 0;
	return DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0,
		Buffer.data(), (DWORD)Buffer.size(), &uBytes, NULL) != FALSE;
}

static bool ReadGeometry(HANDLE hDisk, DISK_GEOMETRY_EX& Geometry)
{
	DWORD uBytes = 0;
	return DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0,
		&Geometry, sizeof(Geometry), &uBytes, NULL) != FALSE;
}

static int ResolveDisk(int iDisk)
{
	return iDisk >= 0 ? iDisk : CEspLocator::GetOsDisk();
}

int CGptDisk::GetOsPartition(int iDisk)
{
	iDisk = ResolveDisk(iDisk);
	if (iDisk < 0)
		return -1;

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
	int iPartition = -1;
	if (DeviceIoControl(hVolume, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &uBytes, NULL)) {
		if ((int)sdn.DeviceNumber == iDisk)
			iPartition = (int)sdn.PartitionNumber;
	}

	CloseHandle(hVolume);
	return iPartition;
}

STATUS CGptDisk::RefreshDisk(int iDisk)
{
	HANDLE hDisk = OpenDisk(iDisk, true);
	if (hDisk == INVALID_HANDLE_VALUE)
		return ERR(STATUS_ACCESS_DENIED, L"Could not open the disk to refresh its partition table.");

	DWORD uBytes = 0;
	BOOL bOk = DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &uBytes, NULL);
	CloseHandle(hDisk);

	return bOk ? OK : ERR(STATUS_UNSUCCESSFUL, L"Windows did not re-read the partition table.");
}

/* ---------------------------------------------------------------------------
 * Free space
 * ------------------------------------------------------------------------ */

RESULT(SFreeSpace) CGptDisk::FindFreeSpace(int iDisk, uint64 uRequired)
{
	iDisk = ResolveDisk(iDisk);
	if (iDisk < 0)
		return ERR(STATUS_NOT_FOUND, L"Could not work out which disk to use.");

	HANDLE hDisk = OpenDisk(iDisk, false);
	if (hDisk == INVALID_HANDLE_VALUE)
		return ERR(STATUS_ACCESS_DENIED, L"Could not open the disk.");

	DISK_GEOMETRY_EX Geometry = { 0 };
	std::vector<unsigned char> Buffer;

	if (!ReadGeometry(hDisk, Geometry) || !ReadLayout(hDisk, Buffer)) {
		CloseHandle(hDisk);
		return ERR(STATUS_UNSUCCESSFUL, L"Could not read the disk layout.");
	}
	CloseHandle(hDisk);

	PDRIVE_LAYOUT_INFORMATION_EX pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)Buffer.data();
	if (pLayout->PartitionStyle != PARTITION_STYLE_GPT)
		return ERR(STATUS_NOT_SUPPORTED, L"This disk does not use a GPT partition table.");

	uint64 uSectorSize = Geometry.Geometry.BytesPerSector;

	//
	// Past the end of everything already on the disk - which is not the same as
	// past the last entry in the table, because the entries are not required to
	// be in offset order.
	//
	uint64 uLastEnd = GPT_FRONT_SECTORS * uSectorSize;
	for (DWORD i = 0; i < pLayout->PartitionCount; i++)
	{
		PPARTITION_INFORMATION_EX pPart = &pLayout->PartitionEntry[i];
		if (pPart->PartitionLength.QuadPart <= 0)
			continue;

		uint64 uEnd = (uint64)pPart->StartingOffset.QuadPart + (uint64)pPart->PartitionLength.QuadPart;
		if (uEnd > uLastEnd)
			uLastEnd = uEnd;
	}

	uint64 uStart = ALIGN_UP(uLastEnd);
	uint64 uEnd = (uint64)Geometry.DiskSize.QuadPart - (GPT_BACK_SECTORS * uSectorSize);

	SFreeSpace Space;
	if (uStart < uEnd && uEnd - uStart >= uRequired) {
		Space.bFound = true;
		Space.uStartOffset = uStart;
		Space.uSize = uEnd - uStart;
	}

	return CResult<SFreeSpace>(0, Space);
}

int CGptDisk::FindOurEsp(int iDisk, const std::wstring& Name)
{
	iDisk = ResolveDisk(iDisk);
	if (iDisk < 0)
		return -1;

	HANDLE hDisk = OpenDisk(iDisk, false);
	if (hDisk == INVALID_HANDLE_VALUE)
		return -1;

	std::vector<unsigned char> Buffer;
	bool bOk = ReadLayout(hDisk, Buffer);
	CloseHandle(hDisk);

	if (!bOk)
		return -1;

	PDRIVE_LAYOUT_INFORMATION_EX pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)Buffer.data();
	if (pLayout->PartitionStyle != PARTITION_STYLE_GPT)
		return -1;

	for (DWORD i = 0; i < pLayout->PartitionCount; i++)
	{
		PPARTITION_INFORMATION_EX pPart = &pLayout->PartitionEntry[i];
		if (!IsEqualGUID(pPart->Gpt.PartitionType, g_EfiSystemPartition))
			continue;

		//
		// The GPT name, not the volume label. The name is written when the
		// partition is created and nothing else touches it; a label is one
		// SetVolumeLabel call away from being anything.
		//
		wchar_t PartName[37] = { 0 };
		memcpy(PartName, pPart->Gpt.Name, sizeof(pPart->Gpt.Name));
		if (_wcsicmp(PartName, Name.c_str()) == 0)
			return pPart->PartitionNumber;
	}

	return -1;
}

uint64 CGptDisk::GetMaxShrink(int iDisk, int iPartition)
{
	iDisk = ResolveDisk(iDisk);
	if (iDisk < 0 || iPartition <= 0)
		return 0;

	std::wstring VolumePath = CEspLocator::GetVolumeGuidPath(iDisk, iPartition);
	if (VolumePath.empty())
		return 0;

	HANDLE hVolume = CreateFileW(VolumePath.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hVolume == INVALID_HANDLE_VALUE)
		return 0;

	//
	// Asking the filesystem rather than guessing from free space. What can
	// actually be given up is limited by where the immovable metadata sits, and
	// on a fragmented volume that is far less than the free space suggests.
	//
	NTFS_VOLUME_DATA_BUFFER Data = { 0 };
	DWORD uBytes = 0;
	uint64 uMax = 0;

	if (DeviceIoControl(hVolume, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, &Data, sizeof(Data), &uBytes, NULL))
	{
		SHRINK_VOLUME_INFORMATION Shrink = { 0 };
		Shrink.ShrinkRequestType = ShrinkPrepare;
		Shrink.NewNumberOfSectors = 0;	// "as far as you can"

		if (DeviceIoControl(hVolume, FSCTL_SHRINK_VOLUME, &Shrink, sizeof(Shrink), NULL, 0, &uBytes, NULL))
		{
			//
			// The prepare succeeded, which reserves the space. It has to be
			// abandoned again straight away - this was a question, not a
			// request.
			//
			SHRINK_VOLUME_INFORMATION Abort = { 0 };
			Abort.ShrinkRequestType = ShrinkAbort;
			DeviceIoControl(hVolume, FSCTL_SHRINK_VOLUME, &Abort, sizeof(Abort), NULL, 0, &uBytes, NULL);

			uMax = (uint64)Data.NumberSectors.QuadPart * Data.BytesPerSector;
		}
	}

	CloseHandle(hVolume);
	return uMax;
}

/* ---------------------------------------------------------------------------
 * Shrinking
 * ------------------------------------------------------------------------ */

STATUS CGptDisk::ShrinkFileSystem(int iDisk, int iPartition, uint64 uShrinkBytes)
{
	std::wstring VolumePath = CEspLocator::GetVolumeGuidPath(iDisk, iPartition);
	if (VolumePath.empty())
		return ERR(STATUS_NOT_FOUND, L"Could not find the volume to shrink.");

	HANDLE hVolume = CreateFileW(VolumePath.c_str(), GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hVolume == INVALID_HANDLE_VALUE)
		return ERR(STATUS_ACCESS_DENIED, L"Could not open the Windows volume for shrinking.");

	STATUS Status;
	DWORD uBytes = 0;

	do
	{
		NTFS_VOLUME_DATA_BUFFER Data = { 0 };
		if (!DeviceIoControl(hVolume, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, &Data, sizeof(Data), &uBytes, NULL)) {
			Status = ERR(STATUS_NOT_SUPPORTED, L"Only an NTFS volume can be shrunk to make room for an EFI partition.");
			break;
		}

		uint64 uCurrentSectors = (uint64)Data.NumberSectors.QuadPart;

		// NTFS keeps a copy of its boot sector at the very end of the volume,
		// so one cluster more has to come off than the caller asked for
		uShrinkBytes += Data.BytesPerCluster;

		if (Data.BytesPerSector == 0) {
			Status = ERR(STATUS_UNSUCCESSFUL, L"The volume reported a sector size of zero.");
			break;
		}
		if (uShrinkBytes % Data.BytesPerSector)
			uShrinkBytes = ((uShrinkBytes / Data.BytesPerSector) + 1) * Data.BytesPerSector;

		uint64 uShrinkSectors = uShrinkBytes / Data.BytesPerSector;
		if (uShrinkSectors >= uCurrentSectors) {
			Status = ERR(STATUS_DISK_FULL, L"The Windows partition is too small to give up that much space.");
			break;
		}

		uint64 uNewSectors = uCurrentSectors - uShrinkSectors;

		//
		// Prepare reserves the space and can fail because immovable files sit
		// where the volume would have to end; commit is what actually moves the
		// boundary. A failed commit is aborted rather than left half done.
		//
		SHRINK_VOLUME_INFORMATION Shrink = { 0 };
		Shrink.ShrinkRequestType = ShrinkPrepare;
		Shrink.NewNumberOfSectors = (LONGLONG)uNewSectors;

		if (!DeviceIoControl(hVolume, FSCTL_SHRINK_VOLUME, &Shrink, sizeof(Shrink), NULL, 0, &uBytes, NULL)) {
			Status = ERR(STATUS_UNSUCCESSFUL,
				L"Windows cannot free that much space at the end of the volume. "
				L"Unmovable files are in the way; defragmenting may help.");
			break;
		}

		SHRINK_VOLUME_INFORMATION Commit = { 0 };
		Commit.ShrinkRequestType = ShrinkCommit;

		if (!DeviceIoControl(hVolume, FSCTL_SHRINK_VOLUME, &Commit, sizeof(Commit), NULL, 0, &uBytes, NULL))
		{
			SHRINK_VOLUME_INFORMATION Abort = { 0 };
			Abort.ShrinkRequestType = ShrinkAbort;
			DeviceIoControl(hVolume, FSCTL_SHRINK_VOLUME, &Abort, sizeof(Abort), NULL, 0, &uBytes, NULL);

			Status = ERR(STATUS_UNSUCCESSFUL, L"The volume could not be shrunk. Nothing was changed.");
			break;
		}

	} while (0);

	CloseHandle(hVolume);
	return Status;
}

/* ---------------------------------------------------------------------------
 * Creating
 * ------------------------------------------------------------------------ */

STATUS CGptDisk::CreatePartition(int iDisk, uint64 uSize, uint64 uStartOffset,
								 const std::wstring& Name, int& iNewPartition)
{
	iNewPartition = -1;

	HANDLE hDisk = OpenDisk(iDisk, true);
	if (hDisk == INVALID_HANDLE_VALUE)
		return ERR(STATUS_ACCESS_DENIED, L"Could not open the disk for writing.");

	STATUS Status;
	DWORD uBytes = 0;
	std::vector<unsigned char> Buffer;

	do
	{
		DISK_GEOMETRY_EX Geometry = { 0 };
		if (!ReadGeometry(hDisk, Geometry) || !ReadLayout(hDisk, Buffer)) {
			Status = ERR(STATUS_UNSUCCESSFUL, L"Could not read the disk layout.");
			break;
		}

		PDRIVE_LAYOUT_INFORMATION_EX pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)Buffer.data();
		if (pLayout->PartitionStyle != PARTITION_STYLE_GPT) {
			Status = ERR(STATUS_NOT_SUPPORTED, L"This disk does not use a GPT partition table.");
			break;
		}
		if (pLayout->PartitionCount >= MAX_PARTITIONS) {
			Status = ERR(STATUS_DISK_FULL, L"The partition table is full.");
			break;
		}

		uint64 uSectorSize = Geometry.Geometry.BytesPerSector;
		uint64 uUsableEnd = (uint64)Geometry.DiskSize.QuadPart - (GPT_BACK_SECTORS * uSectorSize);

		if (uStartOffset + uSize > uUsableEnd) {
			Status = ERR(STATUS_DISK_FULL, L"There is not enough room at the end of the disk.");
			break;
		}

		//
		// Against every partition, not only the one the offset was worked out
		// from. This is the last check before the table is written and the only
		// one that would catch an arithmetic slip further up.
		//
		for (DWORD i = 0; i < pLayout->PartitionCount; i++)
		{
			PPARTITION_INFORMATION_EX pPart = &pLayout->PartitionEntry[i];
			if (pPart->PartitionLength.QuadPart <= 0)
				continue;

			uint64 uPartStart = (uint64)pPart->StartingOffset.QuadPart;
			uint64 uPartEnd = uPartStart + (uint64)pPart->PartitionLength.QuadPart;

			if (uStartOffset < uPartEnd && uStartOffset + uSize > uPartStart) {
				Status = ERR(STATUS_UNSUCCESSFUL, L"The new partition would overlap an existing one. Nothing was changed.");
				break;
			}
		}
		if (Status.IsError())
			break;

		GUID NewGuid;
		if (CoCreateGuid(&NewGuid) != S_OK) {
			Status = ERR(STATUS_UNSUCCESSFUL, L"Could not generate a partition identifier.");
			break;
		}

		//
		// Inserted in offset order. Windows does not require it, but a table
		// whose entries run backwards confuses other partitioning tools and
		// there is no reason to write one.
		//
		DWORD uInsertAt = pLayout->PartitionCount;
		for (DWORD i = 0; i < pLayout->PartitionCount; i++) {
			if ((uint64)pLayout->PartitionEntry[i].StartingOffset.QuadPart > uStartOffset) {
				uInsertAt = i;
				break;
			}
		}

		if (uInsertAt < pLayout->PartitionCount) {
			memmove(&pLayout->PartitionEntry[uInsertAt + 1], &pLayout->PartitionEntry[uInsertAt],
				(pLayout->PartitionCount - uInsertAt) * sizeof(PARTITION_INFORMATION_EX));
		}

		// only the new entry is rewritten; the rest stay exactly as they are
		for (DWORD i = 0; i < pLayout->PartitionCount; i++) {
			if (i != uInsertAt)
				pLayout->PartitionEntry[i].RewritePartition = FALSE;
		}

		PPARTITION_INFORMATION_EX pNew = &pLayout->PartitionEntry[uInsertAt];
		memset(pNew, 0, sizeof(PARTITION_INFORMATION_EX));
		pNew->PartitionStyle = PARTITION_STYLE_GPT;
		pNew->StartingOffset.QuadPart = (LONGLONG)uStartOffset;
		pNew->PartitionLength.QuadPart = (LONGLONG)uSize;
		pNew->PartitionNumber = 0;			// Windows assigns it
		pNew->RewritePartition = TRUE;
		pNew->Gpt.PartitionType = g_EfiSystemPartition;
		pNew->Gpt.PartitionId = NewGuid;
		pNew->Gpt.Attributes = 0;
		wcsncpy_s(pNew->Gpt.Name, Name.c_str(), _TRUNCATE);

		pLayout->PartitionCount++;

		if (!DeviceIoControl(hDisk, IOCTL_DISK_SET_DRIVE_LAYOUT_EX,
			Buffer.data(), (DWORD)Buffer.size(), NULL, 0, &uBytes, NULL)) {
			Status = ERR(STATUS_UNSUCCESSFUL, L"Windows refused to write the new partition table.");
			break;
		}

		DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &uBytes, NULL);

		//
		// Which number it ended up with is Windows' decision, so it is read
		// back rather than guessed - matched by start offset, the one field
		// that cannot have changed.
		//
		Sleep(500);
		if (ReadLayout(hDisk, Buffer))
		{
			pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)Buffer.data();
			for (DWORD i = 0; i < pLayout->PartitionCount; i++) {
				if ((uint64)pLayout->PartitionEntry[i].StartingOffset.QuadPart == uStartOffset) {
					iNewPartition = pLayout->PartitionEntry[i].PartitionNumber;
					break;
				}
			}
		}

		if (iNewPartition <= 0) {
			Status = ERR(STATUS_UNSUCCESSFUL, L"The partition was written but Windows did not report it back.");
			break;
		}

	} while (0);

	CloseHandle(hDisk);
	return Status;
}

STATUS CGptDisk::CreateEspByShrinking(const SCreateEspOptions& Options, int iDisk, int& iNewPartition)
{
	int iOsPartition = GetOsPartition(iDisk);
	if (iOsPartition <= 0)
		return ERR(STATUS_NOT_FOUND, L"Could not find the Windows partition on this disk.");

	//
	// Work out where the new partition would go before anything is changed, so
	// that a layout with no room for it is refused while the disk is still
	// untouched.
	//
	uint64 uOsStart = 0, uOsEnd = 0, uNextStart = 0, uSectorSize = 512;
	{
		HANDLE hDisk = OpenDisk(iDisk, false);
		if (hDisk == INVALID_HANDLE_VALUE)
			return ERR(STATUS_ACCESS_DENIED, L"Could not open the disk.");

		DISK_GEOMETRY_EX Geometry = { 0 };
		std::vector<unsigned char> Buffer;
		bool bOk = ReadGeometry(hDisk, Geometry) && ReadLayout(hDisk, Buffer);
		CloseHandle(hDisk);

		if (!bOk)
			return ERR(STATUS_UNSUCCESSFUL, L"Could not read the disk layout.");

		PDRIVE_LAYOUT_INFORMATION_EX pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)Buffer.data();
		if (pLayout->PartitionStyle != PARTITION_STYLE_GPT)
			return ERR(STATUS_NOT_SUPPORTED, L"This disk does not use a GPT partition table.");

		uSectorSize = Geometry.Geometry.BytesPerSector;

		int iOsIndex = -1;
		for (DWORD i = 0; i < pLayout->PartitionCount; i++) {
			if (pLayout->PartitionEntry[i].PartitionNumber == iOsPartition) { iOsIndex = (int)i; break; }
		}
		if (iOsIndex < 0)
			return ERR(STATUS_NOT_FOUND, L"Could not find the Windows partition in the partition table.");

		uOsStart = (uint64)pLayout->PartitionEntry[iOsIndex].StartingOffset.QuadPart;
		uOsEnd = uOsStart + (uint64)pLayout->PartitionEntry[iOsIndex].PartitionLength.QuadPart;

		// whatever comes next after the Windows partition, or the start of the
		// backup GPT if nothing does
		uNextStart = (uint64)Geometry.DiskSize.QuadPart - (GPT_BACK_SECTORS * uSectorSize);
		for (DWORD i = 0; i < pLayout->PartitionCount; i++)
		{
			if ((int)i == iOsIndex)
				continue;
			uint64 uStart = (uint64)pLayout->PartitionEntry[i].StartingOffset.QuadPart;
			if (uStart >= uOsEnd && uStart < uNextStart)
				uNextStart = uStart;
		}
	}

	//
	// If the gap after Windows is already big enough there is nothing to shrink,
	// which is worth checking before asking the filesystem to move anything.
	//
	uint64 uGap = uNextStart > uOsEnd ? uNextStart - uOsEnd : 0;
	if (uGap >= Options.uSize)
	{
		uint64 uStart = ALIGN_UP(uOsEnd);
		if (uStart + Options.uSize <= uNextStart)
			return CreatePartition(iDisk, Options.uSize, uStart, Options.Name, iNewPartition);
	}

	if (!Options.bAllowShrink)
		return ERR(STATUS_DISK_FULL,
			L"There is no unallocated space for an EFI partition. "
			L"Shrinking the Windows partition would make room, but that was not permitted.");

	//
	// Two megabytes over what is needed. Both the partition start and its end
	// get aligned, and each alignment can eat up to a megabyte.
	//
	uint64 uNeeded = Options.uSize - uGap + 2 * ALIGNMENT;

	STATUS Status = ShrinkFileSystem(iDisk, iOsPartition, uNeeded);
	if (Status.IsError())
		return Status;

	//
	// The filesystem is now smaller than its partition. From here the partition
	// has to be brought down to match, and until that is done the disk is in a
	// state where nothing is lost but the space is not usable either.
	//
	HANDLE hDisk = OpenDisk(iDisk, true);
	if (hDisk == INVALID_HANDLE_VALUE)
		return ERR(STATUS_ACCESS_DENIED, L"The volume was shrunk, but the disk could not be opened to resize the partition.");

	DWORD uBytes = 0;
	std::vector<unsigned char> Buffer;

	do
	{
		if (!ReadLayout(hDisk, Buffer)) {
			Status = ERR(STATUS_UNSUCCESSFUL, L"Could not re-read the disk layout after shrinking.");
			break;
		}

		PDRIVE_LAYOUT_INFORMATION_EX pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)Buffer.data();

		int iOsIndex = -1;
		for (DWORD i = 0; i < pLayout->PartitionCount; i++) {
			if (pLayout->PartitionEntry[i].PartitionNumber == iOsPartition) { iOsIndex = (int)i; break; }
		}
		if (iOsIndex < 0) {
			Status = ERR(STATUS_NOT_FOUND, L"The Windows partition disappeared from the table.");
			break;
		}

		uint64 uOldLength = (uint64)pLayout->PartitionEntry[iOsIndex].PartitionLength.QuadPart;

		//
		// Positions worked out backwards from the next partition, so the new
		// one lands against it rather than against a number derived from the
		// shrink - which is approximate.
		//
		uint64 uEspEnd = ALIGN_DOWN(uNextStart);
		uint64 uEspStart = ALIGN_DOWN(uEspEnd - Options.uSize);
		uint64 uNewOsLength = uEspStart > uOsStart ? uEspStart - uOsStart : 0;

		if (uNewOsLength == 0 || uNewOsLength >= uOldLength) {
			Status = ERR(STATUS_UNSUCCESSFUL, L"The computed layout makes no sense; nothing further was changed.");
			break;
		}

		//
		// The partition must still be at least as large as the filesystem now
		// inside it. Getting this wrong is the one mistake here that loses data,
		// so it is checked against what was actually asked of the filesystem.
		//
		uint64 uShrunkFsSize = uOldLength > uNeeded ? uOldLength - uNeeded : 0;
		if (uNewOsLength < uShrunkFsSize) {
			Status = ERR(STATUS_UNSUCCESSFUL,
				L"The Windows partition would end up smaller than the filesystem inside it. Nothing further was changed.");
			break;
		}

		DISK_GROW_PARTITION Grow = { 0 };
		Grow.PartitionNumber = iOsPartition;
		Grow.BytesToGrow.QuadPart = -(LONGLONG)(uOldLength - uNewOsLength);	// negative shrinks

		if (!DeviceIoControl(hDisk, IOCTL_DISK_GROW_PARTITION, &Grow, sizeof(Grow), NULL, 0, &uBytes, NULL)) {
			Status = ERR(STATUS_UNSUCCESSFUL,
				L"Windows refused to resize the partition. The filesystem was shrunk but the partition was not; "
				L"no data was lost and the space can be recovered by extending the volume again.");
			break;
		}

		DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &uBytes, NULL);

		//
		// Where the partition actually ended up, rather than where it was meant
		// to: Windows rounds the resize to its own idea of a boundary.
		//
		if (!ReadLayout(hDisk, Buffer)) {
			Status = ERR(STATUS_UNSUCCESSFUL, L"Could not re-read the disk layout after resizing.");
			break;
		}
		pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)Buffer.data();

		iOsIndex = -1;
		for (DWORD i = 0; i < pLayout->PartitionCount; i++) {
			if (pLayout->PartitionEntry[i].PartitionNumber == iOsPartition) { iOsIndex = (int)i; break; }
		}
		if (iOsIndex < 0) {
			Status = ERR(STATUS_NOT_FOUND, L"The Windows partition disappeared from the table.");
			break;
		}

		uint64 uActualEnd = (uint64)pLayout->PartitionEntry[iOsIndex].StartingOffset.QuadPart
			+ (uint64)pLayout->PartitionEntry[iOsIndex].PartitionLength.QuadPart;

		uEspStart = ALIGN_UP(uActualEnd);
		if (uEspStart + Options.uSize > uNextStart) {
			Status = ERR(STATUS_DISK_FULL, L"After resizing there is still not enough room for the EFI partition.");
			break;
		}

		CloseHandle(hDisk);
		hDisk = INVALID_HANDLE_VALUE;

		Status = CreatePartition(iDisk, Options.uSize, uEspStart, Options.Name, iNewPartition);

	} while (0);

	if (hDisk != INVALID_HANDLE_VALUE)
		CloseHandle(hDisk);
	return Status;
}

std::wstring CGptDisk::WaitForVolume(int iDisk, int iPartition)
{
	//
	// Windows mounts a new partition on its own schedule, and formatting one
	// that is not mounted yet fails in ways that look like a bad disk. Ten
	// seconds is what DiskCryptor waits and it is enough in practice.
	//
	for (int i = 0; i < 10; i++)
	{
		Sleep(1000);

		std::wstring Path = CEspLocator::GetVolumeGuidPath(iDisk, iPartition);
		if (!Path.empty()) {
			if (GetFileAttributesW((Path + L"\\").c_str()) != INVALID_FILE_ATTRIBUTES)
				return Path;
		}
	}

	// it exists whether or not Windows mounted it; the raw path still works
	return CEspLocator::GetGlobalRootPath(iDisk, iPartition);
}

/* ---------------------------------------------------------------------------
 * Formatting
 * ------------------------------------------------------------------------ */

//
// fmifs.dll's Format, which is what format.com drives. The callback has no user
// context, so the outcome comes back through a thread local - the alternative
// is a global, and two disks being formatted at once would then report each
// other's result.
//
typedef enum {
	FmIfsPercentCompleted = 0,
	FmIfsFormatReport,
	FmIfsInsertDisk,
	FmIfsIncompatibleFileSystem,
	FmIfsFormattingDestination,
	FmIfsIncompatibleMedia,
	FmIfsAccessDenied,
	FmIfsMediaWriteProtected,
	FmIfsCantLock,
	FmIfsCantQuickFormat,
	FmIfsIoError,
	FmIfsFinished,
} FMIFS_PACKET_TYPE;

typedef enum { FmIfsRemovable = 0, FmIfsFixed, FmIfsFloppy } FMIFS_MEDIA_FLAG;

typedef BOOLEAN (WINAPI *TFmIfsCallback)(FMIFS_PACKET_TYPE PacketType, DWORD PacketLength, PVOID PacketData);
typedef VOID (WINAPI *TFormatEx)(PWSTR DriveRoot, FMIFS_MEDIA_FLAG MediaFlag, PWSTR Format,
								 PWSTR Label, BOOL QuickFormat, TFmIfsCallback Callback);

static thread_local bool t_bFormatOk = false;
static thread_local bool t_bFormatDone = false;

static BOOLEAN WINAPI FormatCallback(FMIFS_PACKET_TYPE PacketType, DWORD PacketLength, PVOID PacketData)
{
	if (PacketType == FmIfsFinished && PacketData && PacketLength >= sizeof(BOOLEAN)) {
		t_bFormatDone = true;
		t_bFormatOk = *(BOOLEAN*)PacketData != FALSE;
	}
	return TRUE;	// keep going
}

STATUS CGptDisk::FormatFat32(int iDisk, int iPartition, const std::wstring& Label)
{
	std::wstring VolumePath = CEspLocator::GetVolumeGuidPath(iDisk, iPartition);
	if (VolumePath.empty())
		VolumePath = CEspLocator::GetGlobalRootPath(iDisk, iPartition);

	HMODULE hLib = LoadLibraryW(L"fmifs.dll");
	if (!hLib)
		return ERR(STATUS_DLL_NOT_FOUND, L"fmifs.dll could not be loaded; the partition cannot be formatted.");

	TFormatEx pFormat = (TFormatEx)GetProcAddress(hLib, "FormatEx");
	if (!pFormat)
		pFormat = (TFormatEx)GetProcAddress(hLib, "Format");

	if (!pFormat) {
		FreeLibrary(hLib);
		return ERR(STATUS_PROCEDURE_NOT_FOUND, L"fmifs.dll does not export a format entry point.");
	}

	t_bFormatOk = false;
	t_bFormatDone = false;

	//
	// The trailing backslash matters: fmifs takes a volume root, and without it
	// the call fails with nothing useful reported.
	//
	std::wstring Root = VolumePath + L"\\";
	std::wstring FileSystem = L"FAT32";
	std::wstring VolumeLabel = Label;

	pFormat((PWSTR)Root.c_str(), FmIfsFixed, (PWSTR)FileSystem.c_str(),
		(PWSTR)VolumeLabel.c_str(), TRUE, FormatCallback);

	FreeLibrary(hLib);

	//
	// A callback that never reported finishing is treated as a failure. An
	// unformatted EFI partition is not something to carry on from.
	//
	if (!t_bFormatDone || !t_bFormatOk)
		return ERR(STATUS_UNSUCCESSFUL, L"The EFI partition could not be formatted as FAT32.");

	return OK;
}

/* ---------------------------------------------------------------------------
 * Top level
 * ------------------------------------------------------------------------ */

RESULT(SEspInfo) CGptDisk::CreateEsp(const SCreateEspOptions& Options)
{
	int iDisk = ResolveDisk(Options.iDisk);
	if (iDisk < 0)
		return ERR(STATUS_NOT_FOUND, L"Could not work out which disk to use.");

	if (Options.uSize < DCS_ESP_MIN_SIZE)
		return ERR(STATUS_INVALID_PARAMETER, L"An EFI partition smaller than 32 MB is not useful.");

	if (!CEspLocator::IsGptDisk(iDisk))
		return ERR(STATUS_NOT_SUPPORTED, L"This disk does not use a GPT partition table, so it cannot hold an EFI System Partition.");

	int iNewPartition = -1;

	//
	// Free space at the end of the disk is the case that costs nothing. Only
	// when there is none does anything have to move.
	//
	RESULT(SFreeSpace) SpaceRes = FindFreeSpace(iDisk, Options.uSize);
	if (SpaceRes.IsError())
		return SpaceRes;

	SFreeSpace Space = SpaceRes.GetValue();

	STATUS Status;
	if (Space.bFound)
		Status = CreatePartition(iDisk, Options.uSize, Space.uStartOffset, Options.Name, iNewPartition);
	else
		Status = CreateEspByShrinking(Options, iDisk, iNewPartition);

	if (Status.IsError())
		return Status;

	std::wstring Path = WaitForVolume(iDisk, iNewPartition);

	Status = FormatFat32(iDisk, iNewPartition, Options.Label);
	if (Status.IsError()) {
		//
		// The partition exists but has no filesystem. It is left in place
		// rather than deleted: deleting it would be a second write to the
		// partition table on the strength of a failure nobody has looked at.
		//
		return Status;
	}

	RefreshDisk(iDisk);

	RESULT(SEspInfo) EspRes = CEspLocator::FindEsp(iDisk, iNewPartition);
	if (EspRes.IsError())
		return EspRes;

	return EspRes;
}

RESULT(SEspInfo) CGptDisk::GetOrCreateEsp(const SCreateEspOptions& Options)
{
	int iDisk = ResolveDisk(Options.iDisk);
	if (iDisk < 0)
		return ERR(STATUS_NOT_FOUND, L"Could not work out which disk to use.");

	int iExisting = FindOurEsp(iDisk, Options.Name);
	if (iExisting > 0)
		return CEspLocator::FindEsp(iDisk, iExisting);

	return CreateEsp(Options);
}

STATUS CGptDisk::DeletePartition(int iDisk, int iPartition)
{
	iDisk = ResolveDisk(iDisk);
	if (iDisk < 0 || iPartition <= 0)
		return ERR(STATUS_INVALID_PARAMETER, L"No partition to delete.");

	//
	// Refuse to touch the partition Windows is running from. The caller should
	// never ask, but a partition number is one typo away from being the wrong
	// one and this is not recoverable.
	//
	if (iPartition == GetOsPartition(iDisk))
		return ERR(STATUS_ACCESS_DENIED, L"That is the Windows partition.");

	HANDLE hDisk = OpenDisk(iDisk, true);
	if (hDisk == INVALID_HANDLE_VALUE)
		return ERR(STATUS_ACCESS_DENIED, L"Could not open the disk for writing.");

	DWORD uBytes = 0;
	STATUS Status;

	//
	// Read the table, drop the one entry, write it back. Deliberately not
	// IOCTL_DISK_DELETE_DRIVE_LAYOUT, which removes every partition on the disk.
	//
	std::vector<unsigned char> Buffer;
	if (!ReadLayout(hDisk, Buffer)) {
		CloseHandle(hDisk);
		return ERR(STATUS_UNSUCCESSFUL, L"Could not read the disk layout.");
	}

	PDRIVE_LAYOUT_INFORMATION_EX pLayout = (PDRIVE_LAYOUT_INFORMATION_EX)Buffer.data();

	bool bFound = false;
	for (DWORD i = 0; i < pLayout->PartitionCount; i++)
	{
		if (pLayout->PartitionEntry[i].PartitionNumber != iPartition)
			continue;

		memmove(&pLayout->PartitionEntry[i], &pLayout->PartitionEntry[i + 1],
			(pLayout->PartitionCount - i - 1) * sizeof(PARTITION_INFORMATION_EX));
		pLayout->PartitionCount--;
		bFound = true;
		break;
	}

	if (!bFound) {
		CloseHandle(hDisk);
		return ERR(STATUS_NOT_FOUND, L"No such partition.");
	}

	if (!DeviceIoControl(hDisk, IOCTL_DISK_SET_DRIVE_LAYOUT_EX,
		Buffer.data(), (DWORD)Buffer.size(), NULL, 0, &uBytes, NULL))
		Status = ERR(STATUS_UNSUCCESSFUL, L"Windows refused to write the new partition table.");
	else
		DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &uBytes, NULL);

	CloseHandle(hDisk);
	return Status;
}
