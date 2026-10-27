/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "pch.h"
#include "RecoveryMedia.h"
#include "EspLocator.h"
#include "DcsConfig.h"
#include "GptDisk.h"

#include <windows.h>

extern "C" {
#include "fs/dcconst.h"		// ST_OK, what the two writers return
#include "fs/fs_sup.h"
}

//
// Where the loader has to be for the firmware to find it, and the name shim
// chainloads. Same constants as EfiInstall.cpp - repeated rather than shared
// because they belong to the EFI layout, not to either file.
//
#if defined(_M_ARM64)
 #define EFI_DEFAULT_LOADER	L"\\EFI\\Boot\\BOOTaa64.efi"
 #define SHIM_LOADER_NAME	L"\\EFI\\Boot\\grubaa64.efi"
#elif defined(_M_IX86)
 #define EFI_DEFAULT_LOADER	L"\\EFI\\Boot\\BOOTia32.efi"
 #define SHIM_LOADER_NAME	L"\\EFI\\Boot\\grubia32.efi"
#else
 #define EFI_DEFAULT_LOADER	L"\\EFI\\Boot\\BOOTx64.efi"
 #define SHIM_LOADER_NAME	L"\\EFI\\Boot\\grubx64.efi"
#endif

#define DCS_LOADER_NAME		L"\\EFI\\Boot\\DcsLdr.efi"
#define DCS_RECOVERY_FILE	L"\\EFI\\DCS\\DcsRe.efi"

#define ISO_VOLUME_LABEL	"DCS Boot Disk"
#define FAT_VOLUME_LABEL	"EFI_BOOT"
#define ISO_BOOT_IMAGE		L"\\EFIBOOT.IMG"

/* ---------------------------------------------------------------------------
 * Gathering the files
 * ------------------------------------------------------------------------ */

//
// One file out of a package. The medium is built in memory and written out as a
// whole image, so this hands back bytes and nothing is unpacked on the way.
//
STATUS CRecoveryMedia::ReadPackageFile(EBootPackage Package, const std::wstring& Name, std::vector<unsigned char>& Data)
{
	return CBootPackage::Read(Package, Name, Data);
}

RESULT(std::vector<CRecoveryMedia::SMediaFile>) CRecoveryMedia::BuildFileList(const SRecoveryOptions& Options)
{
	std::vector<SMediaFile> Files;

	//
	// Everything the DCS package carries that is actually present: everything an
	// install gets, plus the entries marked bRecoveryOnly - this is the medium they
	// exist for - plus the optional DiskCryptor modules when the package has them, so
	// a rescue disk can do what the installation could, encrypted systems included.
	// Absent optional files are skipped below.
	//
	std::vector<SBootFile> DcsFiles = CBootPackage::GetFiles(eDcsPackage);
	for (size_t i = 0; i < DcsFiles.size(); i++)
	{
		SMediaFile File;
		File.Path = DcsFiles[i].Target;

		STATUS Status = ReadPackageFile(eDcsPackage, DcsFiles[i].Source, File.Data);
		if (Status.IsError()) {
			if (DcsFiles[i].bRequired)
				return Status;
			continue;	// an optional file this package does not carry
		}

		Files.push_back(File);
	}

	//
	// What the firmware will actually run. Without a shim that is the recovery
	// loader itself; with one it is the shim, and the recovery loader takes the
	// name shim chainloads.
	//
	if (Options.bShim)
	{
		std::vector<SBootFile> ShimFiles = CBootPackage::GetFiles(eShimPackage);
		for (size_t i = 0; i < ShimFiles.size(); i++)
		{
			SMediaFile File;
			File.Path = ShimFiles[i].Target;

			STATUS Status = ReadPackageFile(eShimPackage, ShimFiles[i].Source, File.Data);
			if (Status.IsError()) {
				if (ShimFiles[i].bRequired)
					return Status;
				continue;
			}

			// the shim goes to the default path, DcsLdr to grub's name
			if (i == 0)
				File.Path = EFI_DEFAULT_LOADER;
			else if (File.Path == DCS_LOADER_NAME)
				File.Path = SHIM_LOADER_NAME;

			Files.push_back(File);
		}
	}
	else
	{
		//
		// DcsRe rather than DcsBoot: this is a rescue medium, and the recovery
		// module is what it exists to run.
		//
		std::vector<unsigned char> Loader;
		for (size_t i = 0; i < Files.size(); i++) {
			if (Files[i].Path == DCS_RECOVERY_FILE) { Loader = Files[i].Data; break; }
		}

		if (Loader.empty())
			return ERR(STATUS_NOT_FOUND, L"The boot loader package does not contain the recovery loader DcsRe.efi.");

		SMediaFile File;
		File.Path = EFI_DEFAULT_LOADER;
		File.Data = Loader;
		Files.push_back(File);
	}

	//
	// A configuration file, so the loader has somewhere to read its settings
	// from and somewhere to write them back to. What the caller handed over,
	// or the defaults when it handed over nothing.
	//
	{
		CDcsConfig Config = Options.Config.IsEmpty() ? CDcsConfig::MakeDefault() : Options.Config;
		std::string Xml = Config.ToXml();

		SMediaFile File;
		File.Path = L"\\EFI\\DCS\\DcsProp";
		File.Data.assign(Xml.begin(), Xml.end());
		Files.push_back(File);
	}

	return CResult<std::vector<SMediaFile>>(0, Files);
}

/* ---------------------------------------------------------------------------
 * Removable drives
 * ------------------------------------------------------------------------ */

bool CRecoveryMedia::IsRemovableDrive(const std::wstring& Root)
{
	if (Root.size() < 2 || Root[1] != L':')
		return false;

	wchar_t DrivePath[8];
	swprintf_s(DrivePath, L"%c:\\", Root[0]);

	//
	// GetDriveType rather than the geometry IOCTL: it needs no handle on the
	// raw device, so asking the question does not itself require the rights
	// that writing would.
	//
	return GetDriveTypeW(DrivePath) == DRIVE_REMOVABLE;
}

STATUS CRecoveryMedia::WriteToDrive(const std::wstring& Root, const SRecoveryOptions& Options)
{
	if (Options.bShim && !CBootPackage::IsAvailable(eShimPackage))
		return ERR(STATUS_NOT_FOUND, L"This installation does not include a shim package.");

	//
	// Removable only. The one mistake this stops is formatting the wrong disk,
	// and it is not a mistake anything downstream could undo.
	//
	if (!IsRemovableDrive(Root))
		return ERR(STATUS_INVALID_PARAMETER,
			L"The external loader can only be written to a removable drive. "
			L"The drive given is a fixed disk, a network share or does not exist.");

	RESULT(std::vector<SMediaFile>) FilesRes = BuildFileList(Options);
	if (FilesRes.IsError())
		return FilesRes;

	//
	// The file list is complete before anything is formatted. A package problem
	// then costs nothing; discovering it after the stick has been wiped costs
	// whatever was on it.
	//
	if (Options.bFormat)
	{
		wchar_t DrivePath[8];
		swprintf_s(DrivePath, L"%c:", Root[0]);

		//
		// The partition number behind the drive letter, because the format
		// helper works on disk and partition rather than on a letter.
		//
		std::wstring VolumePath = std::wstring(L"\\\\?\\") + DrivePath;
		HANDLE hVolume = CreateFileW(VolumePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, 0, NULL);
		if (hVolume == INVALID_HANDLE_VALUE)
			return ERR(STATUS_ACCESS_DENIED, L"Could not open the drive.");

		STORAGE_DEVICE_NUMBER sdn = { 0 };
		DWORD uBytes = 0;
		BOOL bOk = DeviceIoControl(hVolume, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &uBytes, NULL);
		CloseHandle(hVolume);

		if (!bOk)
			return ERR(STATUS_UNSUCCESSFUL, L"Could not identify the drive.");

		STATUS Status = CGptDisk::FormatFat32((int)sdn.DeviceNumber, (int)sdn.PartitionNumber, L"EFI_BOOT");
		if (Status.IsError())
			return Status;
	}

	//
	// A drive letter root, with the trailing backslash removed so that the EFI
	// style paths - which all begin with one - join onto it cleanly.
	//
	std::wstring Base = Root;
	while (!Base.empty() && (Base.back() == L'\\' || Base.back() == L'/'))
		Base.pop_back();

	const std::vector<SMediaFile>& Files = FilesRes.GetValue();
	for (size_t i = 0; i < Files.size(); i++)
	{
		STATUS Status = CEspLocator::WriteFile(Base, Files[i].Path, Files[i].Data.data(), Files[i].Data.size());
		if (Status.IsError())
			return Status;
	}

	return OK;
}

/* ---------------------------------------------------------------------------
 * ISO
 * ------------------------------------------------------------------------ */

STATUS CRecoveryMedia::WriteIso(const std::wstring& FilePath, const SRecoveryOptions& Options)
{
	if (Options.bShim && !CBootPackage::IsAvailable(eShimPackage))
		return ERR(STATUS_NOT_FOUND, L"This installation does not include a shim package.");

	RESULT(std::vector<SMediaFile>) FilesRes = BuildFileList(Options);
	if (FilesRes.IsError())
		return FilesRes;

	const std::vector<SMediaFile>& Files = FilesRes.GetValue();

	//
	// The writers take a flat array of path/size/data. Filled from the vector
	// rather than built into it directly, so the C structures live for exactly
	// as long as the two calls below and the data stays owned by the vector.
	//
	std::vector<file_entry_t> Entries(Files.size());
	memset(Entries.data(), 0, Entries.size() * sizeof(file_entry_t));

	for (size_t i = 0; i < Files.size(); i++)
	{
		if (Files[i].Path.size() >= MAX_PATH)
			return ERR(STATUS_NAME_TOO_LONG, L"A file name in the boot loader package is too long.");

		wcscpy_s(Entries[i].path, Files[i].Path.c_str());
		Entries[i].size = Files[i].Data.size();
		Entries[i].data = (void*)Files[i].Data.data();
	}

	STATUS Status;
	void* pFatData = NULL;
	size_t uFatSize = 0;
	void* pIsoData = NULL;
	size_t uIsoSize = 0;

	do
	{
		//
		// A UEFI machine boots optical media through El Torito, and what El
		// Torito points at is a FAT image - so the loader goes into a FAT
		// filesystem first, and that whole filesystem becomes one file in the
		// ISO.
		//
		if (create_fat_image(FAT_VOLUME_LABEL, Entries.data(), Entries.size(), &pFatData, &uFatSize) != ST_OK) {
			Status = ERR(STATUS_UNSUCCESSFUL, L"Could not build the FAT boot image.");
			break;
		}

		file_entry_t BootImage;
		memset(&BootImage, 0, sizeof(BootImage));
		wcscpy_s(BootImage.path, ISO_BOOT_IMAGE);
		BootImage.data = pFatData;
		BootImage.size = uFatSize;

		if (create_iso_image(ISO_VOLUME_LABEL, &BootImage, 1, &BootImage, &pIsoData, &uIsoSize) != ST_OK) {
			Status = ERR(STATUS_UNSUCCESSFUL, L"Could not build the ISO image.");
			break;
		}

		HANDLE hFile = CreateFileW(FilePath.c_str(), GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			Status = ERR(STATUS_ACCESS_DENIED, L"Could not create " + FilePath + L".");
			break;
		}

		DWORD uWritten = 0;
		BOOL bOk = WriteFile(hFile, pIsoData, (DWORD)uIsoSize, &uWritten, NULL);
		CloseHandle(hFile);

		if (!bOk || uWritten != uIsoSize) {
			DeleteFileW(FilePath.c_str());	// a half written ISO is worse than none
			Status = ERR(STATUS_UNSUCCESSFUL, L"Could not write " + FilePath + L".");
			break;
		}

	} while (0);

	if (pFatData) free(pFatData);
	if (pIsoData) free(pIsoData);

	return Status;
}

/* ---------------------------------------------------------------------------
 * PXE
 * ------------------------------------------------------------------------ */

STATUS CRecoveryMedia::WritePxe(const std::wstring& Directory, const SRecoveryOptions& Options)
{
	//
	// A network boot always chains through DcsLdr, which only ships in the shim
	// package - so that package is needed whether or not the shim itself is
	// being used.
	//
	if (!CBootPackage::IsAvailable(eShimPackage))
		return ERR(STATUS_NOT_FOUND,
			L"Network boot files need the shim package, which this installation does not include.");

	SRecoveryOptions PxeOptions = Options;

	RESULT(std::vector<SMediaFile>) FilesRes = BuildFileList(PxeOptions);
	if (FilesRes.IsError())
		return FilesRes;

	std::vector<SMediaFile> Files = FilesRes.GetValue();

	//
	// Without a shim the network loader stands in for it: DcsLdr takes the
	// default path directly, which is what a PXE server hands out.
	//
	if (!Options.bShim)
	{
		std::vector<unsigned char> Loader;
		STATUS Status = ReadPackageFile(eShimPackage, L"DcsLdr.efi", Loader);
		if (Status.IsError())
			return Status;

		bool bReplaced = false;
		for (size_t i = 0; i < Files.size(); i++) {
			if (Files[i].Path == EFI_DEFAULT_LOADER) { Files[i].Data = Loader; bReplaced = true; break; }
		}

		if (!bReplaced) {
			SMediaFile File;
			File.Path = EFI_DEFAULT_LOADER;
			File.Data = Loader;
			Files.push_back(File);
		}
	}

	std::wstring Base = Directory;
	while (!Base.empty() && (Base.back() == L'\\' || Base.back() == L'/'))
		Base.pop_back();

	if (!CreateDirectoryW(Base.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		return ERR(STATUS_ACCESS_DENIED, L"Could not create " + Directory + L".");

	for (size_t i = 0; i < Files.size(); i++)
	{
		STATUS Status = CEspLocator::WriteFile(Base, Files[i].Path, Files[i].Data.data(), Files[i].Data.size());
		if (Status.IsError())
			return Status;
	}

	return OK;
}
