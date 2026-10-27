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
// Bootable recovery media - a USB stick, an ISO, or a PXE directory holding the
// same loader that goes on the EFI partition.
//
// What it is for: a machine whose EFI partition has been overwritten - by a
// Windows update, a firmware reset, or a partition tool - can be booted from
// this instead, which is the difference between repairing an installation and
// reinstalling one.
//
// The layout is the same in all three cases, because the firmware's rules do
// not change with the medium: the loader has to be at \EFI\Boot\BOOTx64.efi,
// and with Secure Boot on it has to be a shim with our loader behind it.
//
// Ported from the recovery half of DiskCryptor's dcapi/efiinst.c. The FAT and
// ISO image writers underneath - fs\fat_sup.c and fs\iso_sup.c - are
// DiskCryptor's own C, compiled in unchanged rather than translated; see the
// note in fs\dcconst.h.
//

#include "../lib_global.h"
#include "../Status.h"
#include "BootPackage.h"
#include "DcsConfig.h"

#include <string>
#include <vector>

struct LIBRARY_EXPORT SRecoveryOptions
{
	//
	// Put a shim in front of the loader. Needed for the medium to boot at all
	// on a machine with Secure Boot on, and harmless on one without.
	//
	bool			bShim = false;

	// Format the stick first. Only meaningful for WriteToDrive, and only ever
	// offered for removable media.
	bool			bFormat = false;

	//
	// The DcsProp the medium is given. Left empty it gets the same defaults a
	// fresh install would; set it to carry the settings of an existing
	// installation over, so that a rescue disk behaves like the machine it was
	// made for rather than like a factory one.
	//
	CDcsConfig		Config;
};

class LIBRARY_EXPORT CRecoveryMedia
{
public:
	//
	// Write the loader onto a removable drive. Root is a drive root such as
	// L"E:\\". Refuses anything that is not removable - the mistake this
	// prevents is formatting the wrong disk.
	//
	static STATUS	WriteToDrive(const std::wstring& Root, const SRecoveryOptions& Options);

	//
	// Build a bootable ISO. The loader goes into a FAT image, which goes into
	// the ISO as its El Torito boot image - that is how a UEFI machine boots
	// optical media, and why two image writers are involved rather than one.
	//
	static STATUS	WriteIso(const std::wstring& FilePath, const SRecoveryOptions& Options);

	//
	// Lay the same files out in a directory for a PXE server to serve. Always
	// needs the shim package, because DcsLdr is what a network boot chains to
	// and that only ships there.
	//
	static STATUS	WritePxe(const std::wstring& Directory, const SRecoveryOptions& Options);

	// Is the drive one this is willing to write to?
	static bool		IsRemovableDrive(const std::wstring& Root);

protected:
	//
	// The file list a recovery medium carries, already resolved to contents.
	// Built once and then either written out as files or handed to the image
	// writer, so the two paths cannot drift apart.
	//
	struct SMediaFile
	{
		std::wstring				Path;	// where it goes, EFI style
		std::vector<unsigned char>	Data;
	};

	static RESULT(std::vector<SMediaFile>)	BuildFileList(const SRecoveryOptions& Options);
	static STATUS	ReadPackageFile(EBootPackage Package, const std::wstring& Name, std::vector<unsigned char>& Data);
};
