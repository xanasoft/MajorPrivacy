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
// The boot loader payload - the .efi and .dcs files that get copied onto the
// EFI partition.
//
// It ships beside MajorPrivacy.exe as DcsPkg_X64.zip and ShimPkg_X64.zip, and is
// read out of the archive in memory. Callers ask for a file by name and get its
// bytes; whoever wants it on disk writes it where it actually belongs.
//
// Nothing is ever unpacked to an intermediate folder. The loader files are
// written straight onto the EFI partition, because a copy left next to the
// executable is a second, stale copy of the thing the machine boots from - and
// it was only ever there to give the installer a file name to copy.
//
// The reading uses the small zip reader in Boot/zip, statically linked. Not
// MiscHelpers' CArchive, because that one is Qt and this has to work in a
// library that is not; and not 7z.dll either, which was the first attempt - a
// loader that has to find and talk COM to a separate DLL is a lot of moving
// parts for reading one stored-and-deflated zip, and it only ever failed in
// ways that looked like a missing file.
//
// An unpacked folder of the same name next to the executable still wins over
// the archive when there is one. That is not a cache - nothing here creates it -
// it is how a build without the zip, or one being tested against a locally
// built DcsPkg, is expected to work.
//

#include "../lib_global.h"
#include "../Status.h"

#include <string>
#include <vector>

//
// Which package. The architecture suffix is settled at compile time, because a
// 64 bit build has no use for the 32 bit boot files and the other way round.
//
enum EBootPackage
{
	eDcsPackage,		// DcsBoot, DcsOwner, DcsInfo, DcsTpm, DcsRe
	eShimPackage,		// shim, MokManager, DcsLdr, CustomSigner.der
};

//
// One file of a package, and where it belongs on the EFI partition.
//
// The list is what makes the package definite rather than "whatever was in the
// archive". efiinst.c names the files it wants and copies those; the same is done
// here. The DiskCryptor modules (DcsInt, DcsTpm) are named but flagged bDiskCryptor,
// so they are unpacked and available yet copied onto an ESP only when asked for -
// see that flag. Anything the archive happens to carry that is not named at all -
// spare DcsProp copies, PlatformInfo variants, a KeyFile - is ignored, never
// unpacked and never copied.
//
// A file can also be in the package and still not belong on a system EFI
// partition. efiinst.c makes that distinction by passing a shorter count for
// an install than for a rescue disk; bRecoveryOnly is the same distinction
// said out loud, so that the one file it applies to cannot be mistaken for
// merely optional.
//
struct LIBRARY_EXPORT SBootFile
{
	const wchar_t*	Source;		// name inside the package
	const wchar_t*	Target;		// path on the EFI partition
	bool			bRequired;	// a missing optional file is not an error
	bool			bRecoveryOnly;	// rescue media only, never copied onto a system ESP

	//
	// DiskCryptor full-disk-encryption support (DcsInt, DcsTpm). MajorPrivacy does
	// not need these, so they are installed only when the "DiskCryptor support"
	// option is set - and on an update they are refreshed only when already present,
	// so a DiskCryptor install updated through MajorPrivacy keeps its modules in step
	// with the loader instead of being left half-upgraded. DC and MP share the same
	// loader; this is how an update from one does not brick the other.
	//
	bool			bDiskCryptor = false;
};

//
// One package, open for reading - from the unpacked folder if there is one,
// otherwise from the archive, which is held in memory for as long as this
// lives. Open it once and read what you need; it owns a handle, so it does not
// copy.
//
class LIBRARY_EXPORT CBootPackageSource
{
public:
	CBootPackageSource(EBootPackage Package);
	~CBootPackageSource();

	// Folder or archive, whichever this installation has.
	STATUS	Open();

	// Is the named file in the package at all?
	bool	Contains(const std::wstring& Name) const;

	//
	// The bytes of one file. Names are matched whole and case-insensitively
	// against what the package carries, so an entry that tries to name a path -
	// "..\\DcsBoot.efi" - simply matches nothing.
	//
	STATUS	Read(const std::wstring& Name, std::vector<unsigned char>& Data) const;

protected:
	// it owns an open archive; copying it would close it twice
	CBootPackageSource(const CBootPackageSource&);
	CBootPackageSource& operator=(const CBootPackageSource&);

	struct SBootPackageImpl*	m;
};

class LIBRARY_EXPORT CBootPackage
{
public:
	//
	// One file out of a package, for callers that want a single one and have no
	// reason to hold the archive open. Both go through CBootPackageSource; a
	// caller reading several files should open one of those instead and keep it
	// for the batch.
	//
	static bool					Contains(EBootPackage Package, const std::wstring& Name);
	static STATUS				Read(EBootPackage Package, const std::wstring& Name,
									 std::vector<unsigned char>& Data);

	//
	// The files a package is expected to contain. Everything downstream works
	// from this list, so adding a file to an install is a matter of naming it
	// here rather than of remembering every place it has to be copied.
	//
	// The whole list, recovery-only entries included - they still have to be
	// unpacked from the archive, and the rescue media builder wants them. It
	// is the install that leaves them out, by reading bRecoveryOnly.
	//
	static std::vector<SBootFile>	GetFiles(EBootPackage Package);

	// The file whose presence means "this package is installed".
	static std::wstring			GetMarkerFile(EBootPackage Package);

	// Is the package available at all, as a folder or as an archive?
	static bool					IsAvailable(EBootPackage Package);

	// The bare names, without paths.
	static std::wstring			GetFolderName(EBootPackage Package);
	static std::wstring			GetArchiveName(EBootPackage Package);

	// The directory the running executable lives in, without a trailing slash.
	static std::wstring			GetAppDirectory();
};
