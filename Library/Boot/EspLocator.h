#pragma once
/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

//
// Finding the EFI System Partition, and reaching its files.
//
// An ESP normally has no drive letter, so it is opened through a path that does
// not need one: either the volume's own GUID path, or
// \\?\GLOBALROOT\Device\HarddiskN\PartitionM. Both work with the ordinary file
// APIs, which is why nothing here has to mount anything.
//
// Ported from DiskCryptor's dcapi/efiinst.c.
//

#include "../lib_global.h"
#include "../Status.h"

#include <string>
#include <vector>

struct LIBRARY_EXPORT SEspInfo
{
	int				iDisk = -1;			// physical drive number
	int				iPartition = -1;	// 1 based partition number
	std::wstring	Path;				// root path, no trailing backslash
	std::wstring	Label;				// volume label, when one could be read
	uint64			uSize = 0;			// partition size in bytes
	uint64			uFree = 0;			// free space in bytes, 0 when unknown
	uint64			uStartOffset = 0;	// byte offset of the partition on the disk

	//
	// The GPT partition GUID, as bytes rather than a GUID so that this header
	// does not have to drag in windows.h. A UEFI boot entry identifies its
	// partition by this, which is why it is carried here and not looked up
	// again when the entry is written.
	//
	unsigned char	PartitionGuid[16] = { 0 };
	//
	// The drive letter it is reachable under, "X:" style, or empty - which is
	// the normal state for an ESP. Only set so that it can be shown and taken
	// away again; nothing in this library works through it.
	//
	std::wstring	MountPoint;

	bool			bGpt = false;		// false for a FAT partition on an MBR disk
	bool			bHasDcs = false;	// \EFI\DCS is present
	bool			bHasMicrosoft = false;	// \EFI\Microsoft\Boot\bootmgfw.efi is present

	bool			IsValid() const { return iDisk >= 0 && iPartition > 0 && !Path.empty(); }
};

struct LIBRARY_EXPORT SDiskInfo
{
	int				iDisk = -1;
	std::wstring	Name;			// model string, when one could be read
	uint64			uSize = 0;
	bool			bGpt = false;
	bool			bIsOsDisk = false;
	int				iEspCount = 0;
};

class LIBRARY_EXPORT CEspLocator
{
public:
	// The physical drive Windows booted from, or -1.
	static int					GetOsDisk();

	// Every fixed disk the machine has, for the disk picker.
	static std::vector<SDiskInfo>	EnumDisks();

	//
	// Every EFI System Partition on a disk. On an MBR disk - which UEFI does
	// not boot from, but which turns up on removable media - a FAT partition
	// holding an \EFI directory is reported instead.
	//
	static std::vector<SEspInfo>	EnumEsps(int iDisk);

	//
	// The ESP to work on. Prefers one that already carries \EFI\DCS, so that a
	// second run finds what the first one installed rather than a different
	// partition; falls back to the first ESP on the disk.
	//
	// iDisk -1 means the disk Windows booted from.
	//
	static RESULT(SEspInfo)		FindEsp(int iDisk = -1, int iPartition = 0);

	// The volume GUID path for a partition, without a trailing backslash.
	static std::wstring			GetVolumeGuidPath(int iDisk, int iPartition);

	// \\?\GLOBALROOT\Device\HarddiskN\PartitionM - always available, but slower
	// for file work than the volume path, so it is only the fallback.
	static std::wstring			GetGlobalRootPath(int iDisk, int iPartition);

	static bool					IsGptDisk(int iDisk);

	/* ---------------------------------------------------------------------
	 * Drive letters
	 *
	 * An EFI System Partition normally has none, which is why Windows leaves
	 * it alone. Giving it one is useful for looking at what is actually on it
	 * with ordinary tools - and is worth taking away again afterwards, because
	 * while the letter is there the partition is exposed to everything running
	 * on the machine.
	 * ------------------------------------------------------------------- */

	// The drive letter a partition currently has, "X:" style, or empty.
	static std::wstring			GetMountPoint(int iDisk, int iPartition);

	// Give it one. DriveLetter is "X:" or "X:\".
	static STATUS				Mount(int iDisk, int iPartition, const std::wstring& DriveLetter);

	// Take it away again. The partition and its contents are untouched.
	static STATUS				Unmount(const std::wstring& DriveLetter);

	//
	// Drive letters nothing is using, from D: up - A and B mean floppies to
	// Windows and C is where it lives, so none of the three is ever offered.
	//
	static std::vector<std::wstring>	GetFreeDriveLetters();

	/* ---------------------------------------------------------------------
	 * Files on the ESP
	 *
	 * All paths are relative to the ESP root and start with a backslash, the
	 * way they are spelled in the EFI world: L"\\EFI\\DCS\\DcsBoot.efi".
	 * ------------------------------------------------------------------- */

	static bool					FileExists(const std::wstring& Root, const std::wstring& Name);
	static STATUS				ReadFile(const std::wstring& Root, const std::wstring& Name, std::vector<unsigned char>& Data);
	static STATUS				WriteFile(const std::wstring& Root, const std::wstring& Name, const void* pData, size_t uSize);
	static STATUS				RemoveFile(const std::wstring& Root, const std::wstring& Name);
	static STATUS				RenameFile(const std::wstring& Root, const std::wstring& From, const std::wstring& To);
	static STATUS				CopyFileTo(const std::wstring& Root, const std::wstring& From, const std::wstring& To);
	static STATUS				MakeDir(const std::wstring& Root, const std::wstring& Name);
	static STATUS				DeleteDir(const std::wstring& Root, const std::wstring& Name, bool bRecursive = true);

	// Copy from an ordinary folder on disk into the ESP.
	static STATUS				CopyFromFolder(const std::wstring& Folder, const std::wstring& Source,
											   const std::wstring& Root, const std::wstring& Target);
};
