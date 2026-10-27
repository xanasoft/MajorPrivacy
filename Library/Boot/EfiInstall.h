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
// Installing the DCS boot loader on an EFI System Partition.
//
// What gets installed is deliberately less than DiskCryptor installs. DcsInt -
// the module that decrypts a boot partition - is left out, because MajorPrivacy
// does not encrypt partitions; its volumes are container files that Windows
// mounts after it has already booted. What is wanted from DCS is DcsOwner,
// which holds a user key in a firmware variable the operating system cannot
// read, and the loader that runs it.
//
// The rest follows DiskCryptor exactly, because the firmware does not care what
// our reasons are: the loader has to be at \EFI\Boot\BOOTx64.efi or named by a
// boot manager entry, the original loader has to be kept somewhere so removal
// can put it back, and under Secure Boot a signed shim has to stand in front of
// the whole thing.
//
// Ported from DiskCryptor's dcapi/efiinst.c.
//

#include "../lib_global.h"
#include "../Status.h"
#include "EspLocator.h"
#include "DcsConfig.h"
#include "BootPackage.h"

#include <string>
#include <vector>

class CSecurePassword;	// Crypto/SecurePassword.h - only referenced by EnrollShimSigner

//
// What is on an EFI partition right now. Everything the bootloader window shows
// comes from one of these, read in a single pass.
//
struct LIBRARY_EXPORT SBootLoaderState
{
	SEspInfo		Esp;

	bool			bInstalled = false;			// \EFI\DCS\DcsBoot.efi is there
	bool			bShim = false;				// shim is installed in \EFI\Boot
	bool			bHasDefaultLoader = false;	// something is at \EFI\Boot\BOOTx64.efi, ours or not
	bool			bIsDefaultLoader = false;	// \EFI\Boot\BOOTx64.efi is ours
	bool			bOriginalSaved = false;		// the loader we displaced was kept
	bool			bBootEntrySet = false;		// the firmware has our boot entry
	bool			bBootEntryIsShim = false;	// and it points at the shim
	bool			bHasConfig = false;			// \EFI\DCS\DcsProp is there

	unsigned long	uVersion = 0;				// DCS build, 0 when it could not be read

	bool			bSecureBoot = false;
	bool			bSetupMode = false;
	bool			bShimAvailable = false;		// a shim package ships with this build

	//
	// Whether the firmware's db would run the images directly, i.e. they are signed
	// by a certificate enrolled in db. bDcsSignedByDb covers the whole DCS boot chain
	// (so the loader needs no shim under Secure Boot); bShimSignedByDb covers the shim
	// binary the firmware launches. These answer the Secure Boot question "will this
	// boot as it is?" and drive whether the shim is pre-selected.
	//
	bool			bDcsSignedByDb = false;
	bool			bShimSignedByDb = false;

	//
	// DiskCryptor full-disk-encryption support (DcsInt / DcsTpm). bDiskCryptorInstalled
	// is whether it is on this partition now; bDiskCryptorAvailable is whether the shipped
	// package even carries the modules to install it.
	//
	bool			bDiskCryptorInstalled = false;
	bool			bDiskCryptorAvailable = false;
};

struct LIBRARY_EXPORT SInstallOptions
{
	int				iDisk = -1;			// -1 for the disk Windows booted from
	int				iPartition = 0;		// 0 to pick the ESP automatically

	//
	// Install the shim in front of the loader. Needed when Secure Boot is on
	// and the loader is not signed by anything in the firmware's db - which is
	// the usual case, because DCS is not signed by Microsoft.
	//
	bool			bShim = false;

	//
	// Write a boot manager entry as well as replacing the default loader. It
	// always goes ahead of Windows in the boot order - an entry that boots
	// second is an entry that never boots - and it always stays, because a
	// one-shot BootNext is of no use to something that has to come up every
	// time.
	//
	bool			bBootEntry = true;

	//
	// Also install the DiskCryptor full-disk-encryption modules (DcsInt / DcsTpm).
	// Off by default - MajorPrivacy does not need them; it is for a machine that also
	// uses DiskCryptor, which shares this same loader.
	//
	bool			bDiskCryptorSupport = false;

	std::wstring	Description = L"MajorPrivacy";
};

class LIBRARY_EXPORT CEfiInstall
{
public:
	// Everything worth knowing about an EFI partition, in one read.
	static RESULT(SBootLoaderState)	GetState(int iDisk = -1, int iPartition = 0);

	// The file the firmware falls back to when no boot entry applies -
	// \EFI\Boot\BOOTx64.efi on x64, the aa64 / ia32 name elsewhere. For
	// messages: the user is told which file is meant, not "the default path".
	static const wchar_t*	GetDefaultLoaderPath();

	//
	// Put the loader on the partition. Refuses if one is already there - update
	// or remove it first, so that a second install cannot quietly overwrite a
	// backup of the original loader with our own.
	//
	static STATUS			Install(const SInstallOptions& Options);

	// Refresh the files of an existing install, keeping its configuration,
	// its shim state and the original loader backup as they are.
	static STATUS			Update(int iDisk = -1, int iPartition = 0);

	// Take it off and put the original loader back.
	static STATUS			Remove(int iDisk = -1, int iPartition = 0);

	/* --------------------------------------------------------------------- */

	static STATUS			InstallShim(int iDisk = -1, int iPartition = 0);
	static STATUS			RemoveShim(int iDisk = -1, int iPartition = 0);
	static bool				IsShimAvailable();

	//
	// Pre-queue the loader's signing certificate (the shim package's
	// CustomSigner.der) into shim's MOK allow list, so that after a shim install
	// the next boot only asks the user to confirm the enrollment in MokManager
	// instead of making them pick the certificate from disk. Password is what
	// MokManager asks for to confirm. No-op when the certificate is already
	// enrolled. Mirrors DiskCryptor's dc_mok_enroll on shim install.
	//
	static STATUS			EnrollShimSigner(const CSecurePassword& Password);

	/* --------------------------------------------------------------------- */

	//
	// The firmware's own boot menu entry. Separate from the files: a machine
	// can boot the loader because it sits at the default path, because the
	// firmware has an entry for it, or both.
	//
	static STATUS			SetBootEntry(const SInstallOptions& Options);
	static STATUS			DelBootEntry();

	/* --------------------------------------------------------------------- */

	static RESULT(CDcsConfig)	LoadConfig(int iDisk = -1, int iPartition = 0);
	static STATUS				SaveConfig(const CDcsConfig& Config, int iDisk = -1, int iPartition = 0);

protected:
	//
	// bInclDiskCryptor copies the DiskCryptor-support files too; bDcOnlyIfPresent
	// (for updates) limits those to the ones already on the partition, so an update
	// refreshes a DiskCryptor install's modules without adding them to one that never
	// had them. Both default off = the plain MajorPrivacy file set.
	//
	static STATUS			CopyPackage(const std::wstring& Root, EBootPackage Package,
										bool bInclDiskCryptor = false, bool bDcOnlyIfPresent = false);
	static STATUS			CopyCertificate(const std::wstring& Root);
	static STATUS			SetDefaultLoader(const std::wstring& Root, bool bShim);
	static unsigned long	ReadVersion(const std::wstring& Root);

	// the EFI_LOAD_OPTION blob a boot entry is made of
	static std::vector<unsigned char>	BuildLoadOption(const SEspInfo& Esp, const std::wstring& Description,
														const std::wstring& ExecPath);
};
