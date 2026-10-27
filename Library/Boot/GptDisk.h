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
// Making an EFI System Partition where there is none.
//
// This is the only part of the boot loader work that can destroy a disk, so it
// is worth being plain about what it does. In the easy case it writes one new
// entry into the GPT in space nothing is using. In the hard case it also
// shrinks the Windows partition to make that space - first the filesystem
// through the shrink FSCTL, then the partition itself - and a mistake in the
// arithmetic between those two steps means a partition that is smaller than the
// filesystem inside it.
//
// Two things follow from that. Shrinking is opt-in: CreateEsp will not do it
// unless asked, and reports that it has nowhere to put the partition instead.
// And every computed offset is checked against every existing partition before
// anything is written, not only against the one it was derived from.
//
// Ported from DiskCryptor's dcapi/gpt_sup.c.
//

#include "../lib_global.h"
#include "../Status.h"
#include "EspLocator.h"

#include <string>

//
// The GPT name and volume label a partition created here carries. The name is
// how it is found again - a volume label can be changed by anyone, and the ESP
// has no drive letter to remember it by.
//
#define DCS_ESP_PARTITION_NAME	L"DCS Boot Partition"
#define DCS_ESP_VOLUME_LABEL	L"DCS_BOOT"

#define DCS_ESP_DEFAULT_SIZE	(128 * 1024 * 1024)
#define DCS_ESP_MIN_SIZE		(32 * 1024 * 1024)

struct LIBRARY_EXPORT SFreeSpace
{
	uint64	uStartOffset = 0;	// byte offset, already aligned
	uint64	uSize = 0;			// how much is there
	bool	bFound = false;
};

struct LIBRARY_EXPORT SCreateEspOptions
{
	int				iDisk = -1;						// -1 for the disk Windows booted from
	uint64			uSize = DCS_ESP_DEFAULT_SIZE;

	//
	// Shrink the Windows partition when there is not enough free space.
	// Deliberately off by default: this is the step that can lose data, and a
	// caller has to have asked for it in so many words.
	//
	bool			bAllowShrink = false;

	std::wstring	Name = DCS_ESP_PARTITION_NAME;
	std::wstring	Label = DCS_ESP_VOLUME_LABEL;
};

class LIBRARY_EXPORT CGptDisk
{
public:
	// The partition Windows is installed on, or -1.
	static int					GetOsPartition(int iDisk);

	//
	// Unallocated space at the end of the disk, aligned and with room left for
	// the backup GPT. Only the tail is considered - a gap between partitions
	// is found by CreateEsp when it looks at the Windows partition's
	// neighbours, but it is not free space anyone should be offered blindly.
	//
	static RESULT(SFreeSpace)	FindFreeSpace(int iDisk, uint64 uRequired);

	// An ESP carrying our GPT partition name, or -1.
	static int					FindOurEsp(int iDisk, const std::wstring& Name = DCS_ESP_PARTITION_NAME);

	//
	// Create the partition and format it. Returns it described the same way
	// CEspLocator does, so the caller can hand it straight to CEfiInstall.
	//
	static RESULT(SEspInfo)		CreateEsp(const SCreateEspOptions& Options);

	// Use ours if it is already there, otherwise make it.
	static RESULT(SEspInfo)		GetOrCreateEsp(const SCreateEspOptions& Options);

	static STATUS				DeletePartition(int iDisk, int iPartition);
	static STATUS				FormatFat32(int iDisk, int iPartition, const std::wstring& Label);
	static STATUS				RefreshDisk(int iDisk);

	//
	// How much the Windows partition could give up, which is what the window
	// has to show before it offers to take any. 0 when it cannot be shrunk.
	//
	static uint64				GetMaxShrink(int iDisk, int iPartition);

protected:
	static STATUS				ShrinkFileSystem(int iDisk, int iPartition, uint64 uShrinkBytes);
	static STATUS				CreatePartition(int iDisk, uint64 uSize, uint64 uStartOffset,
												const std::wstring& Name, int& iNewPartition);
	static STATUS				CreateEspByShrinking(const SCreateEspOptions& Options, int iDisk, int& iNewPartition);

	// Wait for Windows to notice a new partition and mount its volume.
	static std::wstring			WaitForVolume(int iDisk, int iPartition);
};
