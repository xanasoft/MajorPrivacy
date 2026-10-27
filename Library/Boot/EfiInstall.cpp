/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "pch.h"
#include "EfiInstall.h"
#include "BootPackage.h"
#include "EfiVars.h"
#include "MokManager.h"

#include <windows.h>
#include <string.h>
#include <wchar.h>

//
// The path the firmware looks at when nothing tells it otherwise. Replacing
// this is what makes the machine boot our loader without a boot entry at all -
// and why the loader that was there first has to be kept.
//
#if defined(_M_ARM64)
 #define EFI_DEFAULT_LOADER	L"\\EFI\\Boot\\BOOTaa64.efi"
 #define EFI_LOADER_BACKUP	L"\\EFI\\Boot\\original_BOOTaa64.bin"
 #define SHIM_LOADER_NAME	L"\\EFI\\Boot\\grubaa64.efi"
#elif defined(_M_IX86)
 #define EFI_DEFAULT_LOADER	L"\\EFI\\Boot\\BOOTia32.efi"
 #define EFI_LOADER_BACKUP	L"\\EFI\\Boot\\original_BOOTia32.bin"
 #define SHIM_LOADER_NAME	L"\\EFI\\Boot\\grubia32.efi"
#else
 #define EFI_DEFAULT_LOADER	L"\\EFI\\Boot\\BOOTx64.efi"
 #define EFI_LOADER_BACKUP	L"\\EFI\\Boot\\original_BOOTx64.bin"
 #define SHIM_LOADER_NAME	L"\\EFI\\Boot\\grubx64.efi"
#endif

#define MSFT_BOOT_LOADER	L"EFI\\Microsoft\\Boot\\bootmgfw.efi"

//
// The boot entry number. DiskCryptor uses 0xDC5B and we keep it: a machine that
// has had both installed should end up with one entry that gets reused, not two
// that compete.
//
#define DCS_BOOT_ENTRY_ID	0xDC5B

// signatures the loader carries its version behind, from bootloader.h
#define LDR_CFG_SIGN1		0x1434A669
#define LDR_CFG_SIGN2		0x7269DA46

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------ */

static bool BufferHasWideString(const std::vector<unsigned char>& Data, const std::wstring& Text)
{
	if (Text.empty() || Data.size() < Text.size() * sizeof(wchar_t))
		return false;

	size_t uBytes = Text.size() * sizeof(wchar_t);
	for (size_t i = 0; i + uBytes <= Data.size(); i++) {
		if (memcmp(Data.data() + i, Text.c_str(), uBytes) == 0)
			return true;
	}
	return false;
}

static bool BufferHasBytes(const std::vector<unsigned char>& Data, const unsigned char* pNeedle, size_t uLen)
{
	if (uLen == 0 || Data.size() < uLen)
		return false;

	for (size_t i = 0; i + uLen <= Data.size(); i++) {
		if (memcmp(Data.data() + i, pNeedle, uLen) == 0)
			return true;
	}
	return false;
}

static void AppendU16(std::vector<unsigned char>& Data, unsigned short uValue)
{
	Data.push_back((unsigned char)(uValue & 0xFF));
	Data.push_back((unsigned char)(uValue >> 8));
}

static void AppendU32(std::vector<unsigned char>& Data, unsigned long uValue)
{
	for (int i = 0; i < 4; i++)
		Data.push_back((unsigned char)((uValue >> (i * 8)) & 0xFF));
}

static void AppendU64(std::vector<unsigned char>& Data, uint64 uValue)
{
	for (int i = 0; i < 8; i++)
		Data.push_back((unsigned char)((uValue >> (i * 8)) & 0xFF));
}

static void AppendWide(std::vector<unsigned char>& Data, const std::wstring& Text)
{
	for (size_t i = 0; i < Text.size(); i++)
		AppendU16(Data, (unsigned short)Text[i]);
	AppendU16(Data, 0);
}

/* ---------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------ */

unsigned long CEfiInstall::ReadVersion(const std::wstring& Root)
{
	//
	// The loader carries a version record behind two signature words. DiskCryptor
	// reads it out of DcsInt, which is not installed here, so it is read out of
	// the boot file instead - the record is in both.
	//
	std::vector<unsigned char> Data;
	if (CEspLocator::ReadFile(Root, CBootPackage::GetMarkerFile(eDcsPackage), Data).IsError())
		return 0;

	for (size_t i = 0; i + 12 <= Data.size(); i++)
	{
		unsigned long uSign1, uSign2, uVersion;
		memcpy(&uSign1, Data.data() + i, 4);
		if (uSign1 != LDR_CFG_SIGN1)
			continue;

		memcpy(&uSign2, Data.data() + i + 4, 4);
		if (uSign2 != LDR_CFG_SIGN2)
			continue;

		memcpy(&uVersion, Data.data() + i + 8, 4);
		return uVersion;
	}

	return 0;
}

//
// Secure Boot db trust of the shipped packages. "Directly bootable" means the
// firmware's db would run the images as they are, with no shim: every image in
// the chain is signed by a certificate enrolled in db. This is the self-rolled
// db / PK case - the owner signs the loader with a key they put in db. Ported in
// spirit from DiskCryptor's dc_efi_dcs_is_signed (dcapi/efiinst.c + sb_sup.c).
//
// Computed once: the packages beside the executable and the firmware db do not
// change while this window is open (a db change needs a reboot to take effect).
//
static bool s_bDbTrustDone = false;
static bool s_bDcsDbSigned = false;
static bool s_bShimDbSigned = false;

static bool IsSignableImage(const wchar_t* Name)
{
	const wchar_t* pExt = wcsrchr(Name, L'.');
	return pExt && (_wcsicmp(pExt, L".efi") == 0 || _wcsicmp(pExt, L".dcs") == 0);
}

static void ComputeDbTrust()
{
	if (s_bDbTrustDone)
		return;
	s_bDbTrustDone = true;

	std::vector<std::vector<unsigned char>> Db = CMokManager::ListDbCertThumbprints();
	if (Db.empty())
		return;		// no db (non-UEFI, or db empty) - nothing is directly trusted

	//
	// DCS: the whole boot chain - the firmware launches the default loader and
	// LoadImage-verifies each image it pulls in against db, so every signable image
	// must be signed by a db certificate for the chain to run without a shim.
	//
	if (CBootPackage::IsAvailable(eDcsPackage))
	{
		CBootPackageSource Source(eDcsPackage);
		if (!Source.Open().IsError())
		{
			std::vector<SBootFile> Files = CBootPackage::GetFiles(eDcsPackage);

			bool bAll = true, bAny = false;
			for (size_t i = 0; i < Files.size() && bAll; i++)
			{
				// Recovery-only and the optional DiskCryptor modules are not part of the
				// base chain the shim decision is about, so they do not gate db trust.
				if (Files[i].bRecoveryOnly || Files[i].bDiskCryptor || !IsSignableImage(Files[i].Source))
					continue;

				// the image is judged where it lies, in the package
				std::vector<unsigned char> Image;
				if (Source.Read(Files[i].Source, Image).IsError()) {
					if (Files[i].bRequired)
						bAll = false;
					continue;		// an absent optional image does not break trust
				}

				if (!CMokManager::IsDataTrustedByDb(Image, Db))
					bAll = false;	// unsigned, or not chaining to a cert the firmware trusts
				else
					bAny = true;
			}
			s_bDcsDbSigned = bAll && bAny;
		}
	}

	//
	// Shim: only the front file matters - the one the firmware launches. What the
	// shim chainloads afterwards is verified by the shim (via MOK), not by db.
	//
	if (CBootPackage::IsAvailable(eShimPackage))
	{
		std::vector<SBootFile> Files = CBootPackage::GetFiles(eShimPackage);
		if (!Files.empty())
		{
			std::vector<unsigned char> Image;
			s_bShimDbSigned = !CBootPackage::Read(eShimPackage, Files.front().Source, Image).IsError()
				&& CMokManager::IsDataTrustedByDb(Image, Db);
		}
	}
}

//
// Whether the shipped DCS package even carries the DiskCryptor-support modules, so
// the option can be offered only when there is something to install. Cached - the
// package does not change while the window is open.
//
static int s_iDcAvail = -1;

static bool IsDiskCryptorInPackage()
{
	if (s_iDcAvail >= 0)
		return s_iDcAvail != 0;
	s_iDcAvail = 0;

	if (!CBootPackage::IsAvailable(eDcsPackage))
		return false;

	CBootPackageSource Source(eDcsPackage);
	if (Source.Open().IsError())
		return false;

	std::vector<SBootFile> Files = CBootPackage::GetFiles(eDcsPackage);
	for (size_t i = 0; i < Files.size(); i++)
	{
		if (!Files[i].bDiskCryptor)
			continue;
		if (Source.Contains(Files[i].Source)) {
			s_iDcAvail = 1;
			break;
		}
	}
	return s_iDcAvail != 0;
}

RESULT(SBootLoaderState) CEfiInstall::GetState(int iDisk, int iPartition)
{
	STATUS Status = CEfiVars::Init();
	if (Status.IsError())
		return Status;

	RESULT(SEspInfo) EspRes = CEspLocator::FindEsp(iDisk, iPartition);
	if (EspRes.IsError())
		return EspRes;

	SBootLoaderState State;
	State.Esp = EspRes.GetValue();

	const std::wstring& Root = State.Esp.Path;

	State.bInstalled = CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eDcsPackage));
	State.bShim = CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eShimPackage));
	State.bHasDefaultLoader = CEspLocator::FileExists(Root, EFI_DEFAULT_LOADER);
	State.bOriginalSaved = CEspLocator::FileExists(Root, EFI_LOADER_BACKUP);
	State.bHasConfig = CEspLocator::FileExists(Root, L"\\EFI\\DCS\\DcsProp");

	State.bDiskCryptorInstalled = CEspLocator::FileExists(Root, L"\\EFI\\DCS\\DcsInt.dcs");
	State.bDiskCryptorAvailable = IsDiskCryptorInPackage();

	State.bSecureBoot = CEfiVars::IsSecureBoot();
	State.bSetupMode = CEfiVars::IsSetupMode();
	State.bShimAvailable = IsShimAvailable();

	ComputeDbTrust();
	State.bDcsSignedByDb = s_bDcsDbSigned;
	State.bShimSignedByDb = s_bShimDbSigned;

	if (State.bInstalled)
		State.uVersion = ReadVersion(Root);

	//
	// Is the default loader ours? Comparing the bytes rather than trusting that
	// an install put them there: a Windows update rewrites BOOTx64.efi without
	// asking, and that is exactly the case worth reporting.
	//
	if (State.bInstalled)
	{
		std::vector<unsigned char> Default, Ours;
		if (CEspLocator::ReadFile(Root, EFI_DEFAULT_LOADER, Default).IsSuccess())
		{
			std::wstring Source = State.bShim
				? CBootPackage::GetMarkerFile(eShimPackage)
				: CBootPackage::GetMarkerFile(eDcsPackage);

			if (CEspLocator::ReadFile(Root, Source, Ours).IsSuccess())
				State.bIsDefaultLoader = Default.size() == Ours.size() && Default == Ours;
		}
	}

	//
	// The boot entry is ours if it names this partition's GUID. The GUID is what
	// the firmware matches on, so an entry that does not carry it would not boot
	// this partition whatever its description says.
	//
	std::vector<unsigned char> Entry = CEfiVars::Get(CEfiVars::BootEntryName(DCS_BOOT_ENTRY_ID));
	if (!Entry.empty() && State.Esp.bGpt)
	{
		if (BufferHasBytes(Entry, State.Esp.PartitionGuid, 16)) {
			State.bBootEntrySet = true;
			State.bBootEntryIsShim = BufferHasWideString(Entry, L"shim");
		}
	}

	return CResult<SBootLoaderState>(0, State);
}

/* ---------------------------------------------------------------------------
 * Installing
 * ------------------------------------------------------------------------ */

bool CEfiInstall::IsShimAvailable()
{
	return CBootPackage::IsAvailable(eShimPackage);
}

const wchar_t* CEfiInstall::GetDefaultLoaderPath()
{
	return EFI_DEFAULT_LOADER;
}

STATUS CEfiInstall::CopyPackage(const std::wstring& Root, EBootPackage Package,
								bool bInclDiskCryptor, bool bDcOnlyIfPresent)
{
	//
	// Opened once for the whole set, and read straight onto the partition. The
	// package is never unpacked anywhere on the way: a loader file exists in the
	// archive and then on the ESP, and nowhere in between, so there is no third
	// copy to go stale or to be mistaken for the one the machine boots.
	//
	CBootPackageSource Source(Package);

	STATUS Status = Source.Open();
	if (Status.IsError())
		return Status;

	std::vector<SBootFile> Files = CBootPackage::GetFiles(Package);

	for (size_t i = 0; i < Files.size(); i++)
	{
		//
		// Rescue media files are not part of an install. DcsRe is the only one
		// of them, and copying it here is what made installs boot into the
		// recovery menu instead of the loader - see the table in BootPackage.cpp.
		//
		if (Files[i].bRecoveryOnly)
			continue;

		//
		// DiskCryptor-support modules: only with the option, and on an update only
		// when the target is already there - so an update refreshes a DiskCryptor
		// install's modules in step with the loader, but never adds them to a plain
		// MajorPrivacy install.
		//
		if (Files[i].bDiskCryptor)
		{
			if (!bInclDiskCryptor)
				continue;
			if (bDcOnlyIfPresent && !CEspLocator::FileExists(Root, Files[i].Target))
				continue;
		}

		//
		// An optional file the package does not have is skipped rather than
		// failing the install - LegacySpeaker has no ARM64 build, and a package
		// trimmed for size may drop the recovery loader.
		//
		if (!Source.Contains(Files[i].Source)) {
			if (Files[i].bRequired)
				return ERR(STATUS_NOT_FOUND, std::wstring(L"The boot loader package does not contain ") + Files[i].Source + L".");
			continue;
		}

		std::vector<unsigned char> Data;
		Status = Source.Read(Files[i].Source, Data);
		if (Status.IsError())
			return Status;

		Status = CEspLocator::WriteFile(Root, Files[i].Target, Data.data(), Data.size());
		if (Status.IsError())
			return Status;
	}

	return OK;
}

//
// The DCS pre-boot loader needs a copy of the MajorPrivacy license certificate next to its
// binaries (\EFI\DCS\Certificate.dat) to validate at boot. It is the user's per-machine license
// from the app directory, not part of the shipped package. Copy it when present; a machine with no
// certificate (free mode) simply has none to copy, which is not an install failure.
//
STATUS CEfiInstall::CopyCertificate(const std::wstring& Root)
{
	std::wstring AppDir = CBootPackage::GetAppDirectory();
	std::wstring Source = AppDir + L"\\Certificate.dat";

	if (GetFileAttributesW(Source.c_str()) == INVALID_FILE_ATTRIBUTES)
		return OK;		// no license certificate installed - nothing to copy

	return CEspLocator::CopyFromFolder(AppDir, L"Certificate.dat", Root, L"\\EFI\\DCS\\Certificate.dat");
}

STATUS CEfiInstall::SetDefaultLoader(const std::wstring& Root, bool bShim)
{
	if (!bShim)
	{
		// the loader itself goes to the default path
		return CEspLocator::CopyFileTo(Root, CBootPackage::GetMarkerFile(eDcsPackage), EFI_DEFAULT_LOADER);
	}

	//
	// With a shim the firmware runs the shim, and the shim runs whatever sits
	// next to it under the name it was built to chainload - grubx64.efi, because
	// shim comes from the Linux world and that name is compiled into it. So the
	// shim takes the default path and our loader takes grub's name.
	//
	STATUS Status = CEspLocator::CopyFileTo(Root, CBootPackage::GetMarkerFile(eShimPackage), EFI_DEFAULT_LOADER);
	if (Status.IsError())
		return Status;

	return CEspLocator::RenameFile(Root, L"\\EFI\\Boot\\DcsLdr.efi", SHIM_LOADER_NAME);
}

STATUS CEfiInstall::Install(const SInstallOptions& Options)
{
	STATUS Status = CEfiVars::Init();
	if (Status.IsError())
		return Status;

	RESULT(SEspInfo) EspRes = CEspLocator::FindEsp(Options.iDisk, Options.iPartition);
	if (EspRes.IsError())
		return EspRes;

	SEspInfo Esp = EspRes.GetValue();
	const std::wstring& Root = Esp.Path;

	//
	// Refuse rather than overwrite. A second install would copy our own loader
	// over the backup of the one it displaced the first time, and that backup is
	// the only way the machine gets its original loader back.
	//
	if (CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eDcsPackage)))
		return ERR(STATUS_OBJECT_NAME_COLLISION, L"A boot loader is already installed on this EFI partition. Update or remove it first.");

	if (Options.bShim && !IsShimAvailable())
		return ERR(STATUS_NOT_FOUND, L"This installation does not include a shim package, which Secure Boot needs.");

	Status = CEspLocator::MakeDir(Root, L"\\EFI\\DCS");
	if (Status.IsError())
		return Status;

	Status = CopyPackage(Root, eDcsPackage, Options.bDiskCryptorSupport, false);
	if (Status.IsError())
		return Status;

	//
	// The license certificate must sit next to the DCS binaries for the loader to validate at boot.
	//
	Status = CopyCertificate(Root);
	if (Status.IsError())
		return Status;

	//
	// Keep whatever was booting this machine before. If there was nothing at the
	// default path there is nothing to keep - a machine that boots only through
	// firmware entries - and removal will simply delete what we put there.
	//
	Status = CEspLocator::MakeDir(Root, L"\\EFI\\Boot");
	if (Status.IsError())
		return Status;

	if (CEspLocator::FileExists(Root, EFI_DEFAULT_LOADER) && !CEspLocator::FileExists(Root, EFI_LOADER_BACKUP))
	{
		Status = CEspLocator::CopyFileTo(Root, EFI_DEFAULT_LOADER, EFI_LOADER_BACKUP);
		if (Status.IsError())
			return Status;
	}

	if (Options.bShim)
	{
		Status = CopyPackage(Root, eShimPackage);
		if (Status.IsError())
			return Status;
	}

	Status = SetDefaultLoader(Root, Options.bShim);
	if (Status.IsError())
		return Status;

	//
	// A configuration file, unless one survived a previous install. DCS reads
	// its defaults for anything absent, so this only has to exist.
	//
	if (!CEspLocator::FileExists(Root, L"\\EFI\\DCS\\DcsProp"))
	{
		CDcsConfig Config = CDcsConfig::MakeDefault();
		Status = Config.Save(Root);
		if (Status.IsError())
			return Status;
	}

	if (Options.bBootEntry)
	{
		SInstallOptions Entry = Options;
		Entry.iDisk = Esp.iDisk;
		Entry.iPartition = Esp.iPartition;

		Status = SetBootEntry(Entry);
		if (Status.IsError())
			return Status;
	}

	return OK;
}

STATUS CEfiInstall::Update(int iDisk, int iPartition)
{
	RESULT(SEspInfo) EspRes = CEspLocator::FindEsp(iDisk, iPartition);
	if (EspRes.IsError())
		return EspRes;

	const std::wstring& Root = EspRes.GetValue().Path;

	if (!CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eDcsPackage)))
		return ERR(STATUS_NOT_FOUND, L"No boot loader is installed on this EFI partition.");

	//
	// Refresh the DiskCryptor-support modules too, but only the ones already on the
	// partition (bDcOnlyIfPresent) - so a DiskCryptor install updated through MajorPrivacy
	// keeps its modules in step with the loader, while a plain install gains nothing new.
	//
	STATUS Status = CopyPackage(Root, eDcsPackage, true, true);
	if (Status.IsError())
		return Status;

	//
	// Refresh the license certificate alongside the binaries (it may have been renewed).
	//
	Status = CopyCertificate(Root);
	if (Status.IsError())
		return Status;

	//
	// Whether the shim stays is decided by what is already there, not by an
	// option: an update must not quietly change how the machine boots.
	//
	bool bShim = CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eShimPackage));
	if (bShim)
	{
		Status = CopyPackage(Root, eShimPackage);
		if (Status.IsError())
			return Status;
	}

	//
	// The default loader is rewritten because that is the file a Windows update
	// is most likely to have replaced; the configuration and the original loader
	// backup are left exactly as they are.
	//
	return SetDefaultLoader(Root, bShim);
}

STATUS CEfiInstall::Remove(int iDisk, int iPartition)
{
	RESULT(SEspInfo) EspRes = CEspLocator::FindEsp(iDisk, iPartition);
	if (EspRes.IsError())
		return EspRes;

	const std::wstring& Root = EspRes.GetValue().Path;

	if (!CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eDcsPackage)))
		return ERR(STATUS_NOT_FOUND, L"No boot loader is installed on this EFI partition.");

	// the firmware entry first: an entry pointing at files that are gone is
	// worse than no entry at all
	DelBootEntry();

	CEspLocator::DeleteDir(Root, L"\\EFI\\DCS", true);

	if (CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eShimPackage)))
	{
		std::vector<SBootFile> Files = CBootPackage::GetFiles(eShimPackage);
		for (size_t i = 0; i < Files.size(); i++)
			CEspLocator::RemoveFile(Root, Files[i].Target);

		CEspLocator::RemoveFile(Root, SHIM_LOADER_NAME);
	}

	CEspLocator::RemoveFile(Root, EFI_DEFAULT_LOADER);

	//
	// Put back what was there before. A rename rather than a copy, so that the
	// backup cannot be left behind to be mistaken for a current loader.
	//
	if (CEspLocator::FileExists(Root, EFI_LOADER_BACKUP))
		CEspLocator::RenameFile(Root, EFI_LOADER_BACKUP, EFI_DEFAULT_LOADER);

	return OK;
}

/* ---------------------------------------------------------------------------
 * Shim
 * ------------------------------------------------------------------------ */

STATUS CEfiInstall::InstallShim(int iDisk, int iPartition)
{
	if (!IsShimAvailable())
		return ERR(STATUS_NOT_FOUND, L"This installation does not include a shim package.");

	RESULT(SEspInfo) EspRes = CEspLocator::FindEsp(iDisk, iPartition);
	if (EspRes.IsError())
		return EspRes;

	const std::wstring& Root = EspRes.GetValue().Path;

	if (!CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eDcsPackage)))
		return ERR(STATUS_NOT_FOUND, L"No boot loader is installed on this EFI partition.");

	if (CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eShimPackage)))
		return OK;	// already there

	STATUS Status = CopyPackage(Root, eShimPackage);
	if (Status.IsError())
		return Status;

	return SetDefaultLoader(Root, true);
}

STATUS CEfiInstall::RemoveShim(int iDisk, int iPartition)
{
	RESULT(SEspInfo) EspRes = CEspLocator::FindEsp(iDisk, iPartition);
	if (EspRes.IsError())
		return EspRes;

	const std::wstring& Root = EspRes.GetValue().Path;

	if (!CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eShimPackage)))
		return OK;

	std::vector<SBootFile> Files = CBootPackage::GetFiles(eShimPackage);
	for (size_t i = 0; i < Files.size(); i++)
		CEspLocator::RemoveFile(Root, Files[i].Target);

	CEspLocator::RemoveFile(Root, SHIM_LOADER_NAME);

	//
	// Back to the loader running directly. Leaving the shim's default-path copy
	// in place would leave the machine chainloading a file that is no longer
	// there.
	//
	if (CEspLocator::FileExists(Root, CBootPackage::GetMarkerFile(eDcsPackage)))
		return SetDefaultLoader(Root, false);

	return OK;
}

//
// Pre-provision shim's MOK allow list with the loader's signing certificate, so
// that after a shim install the next boot only asks the user to *confirm* the
// enrollment in MokManager rather than making them find the certificate on disk
// themselves. The certificate ships in the shim package as CustomSigner.der;
// Password is what MokManager asks for at the boot screen to confirm. A no-op
// (OK) when the certificate is already enrolled. Mirrors DiskCryptor's
// dc_mok_enroll_files on shim install (dcapi/efiinst.c).
//
STATUS CEfiInstall::EnrollShimSigner(const CSecurePassword& Password)
{
	if (!IsShimAvailable())
		return ERR(STATUS_NOT_FOUND, L"This installation does not include a shim package.");

	std::vector<unsigned char> Cert;
	if (CBootPackage::Read(eShimPackage, L"CustomSigner.der", Cert).IsError() || Cert.empty())
		return ERR(STATUS_NOT_FOUND, L"The shim package does not contain the signer certificate (CustomSigner.der).");

	//
	// Already trusted? Don't queue a request MokManager would make the user
	// confirm for a key the machine already has in its list.
	//
	if (CMokManager::IsCertEnrolled(Cert))
		return OK;

	STATUS Status = CMokManager::EnrollCerts(std::vector<std::vector<unsigned char>>{ Cert }, Password, eMokAllow);
	if (Status.IsError())
		return Status;

	//
	// Give the person at the keyboard time to reach the MokManager prompt. Shim
	// resets this to its short default once the request is handled, so it only
	// stretches the one boot that matters. (DiskCryptor sets 10 minutes here.)
	//
	CMokManager::SetTimeout(600);
	return OK;
}

/* ---------------------------------------------------------------------------
 * The firmware boot entry
 * ------------------------------------------------------------------------ */

std::vector<unsigned char> CEfiInstall::BuildLoadOption(const SEspInfo& Esp, const std::wstring& Description,
														const std::wstring& ExecPath)
{
	//
	// An EFI_LOAD_OPTION: attributes, the length of the device path that
	// follows the description, the description itself, and then a device path
	// of exactly three nodes - which partition, which file on it, and the end
	// marker. The firmware matches the partition by its GPT GUID, so the entry
	// keeps working if the partition is renumbered.
	//
	std::vector<unsigned char> Data;

	const unsigned short uFilePathLength =
		(unsigned short)(42							// the hard drive node
			+ 4										// the file path node's own header
			+ ExecPath.size() * 2 + 2				// the path and its terminator
			+ 4);									// the end of device path node

	AppendU32(Data, 1);								// LOAD_OPTION_ACTIVE
	AppendU16(Data, uFilePathLength);
	AppendWide(Data, Description);

	// HARDDRIVE_DEVICE_PATH
	Data.push_back(0x04);							// type: media
	Data.push_back(0x01);							// subtype: hard drive
	AppendU16(Data, 0x2A);							// 42 bytes, this node
	AppendU32(Data, (unsigned long)Esp.iPartition);
	AppendU64(Data, Esp.uStartOffset >> 9);			// in 512 byte blocks
	AppendU64(Data, Esp.uSize >> 9);
	Data.insert(Data.end(), Esp.PartitionGuid, Esp.PartitionGuid + 16);
	Data.push_back(0x02);							// MBR type: GPT
	Data.push_back(0x02);							// signature type: GUID

	// FILE_PATH
	Data.push_back(0x04);							// type: media
	Data.push_back(0x04);							// subtype: file path
	AppendU16(Data, (unsigned short)(ExecPath.size() * 2 + 2 + 4));
	AppendWide(Data, ExecPath);

	// END_DEVICE_PATH
	Data.push_back(0x7F);
	Data.push_back(0xFF);
	AppendU16(Data, 4);

	return Data;
}

STATUS CEfiInstall::SetBootEntry(const SInstallOptions& Options)
{
	STATUS Status = CEfiVars::Init();
	if (Status.IsError())
		return Status;

	RESULT(SEspInfo) EspRes = CEspLocator::FindEsp(Options.iDisk, Options.iPartition);
	if (EspRes.IsError())
		return EspRes;

	SEspInfo Esp = EspRes.GetValue();

	if (!Esp.bGpt)
		return ERR(STATUS_NOT_SUPPORTED, L"A firmware boot entry needs a GPT partition to point at.");

	if (!CEspLocator::FileExists(Esp.Path, CBootPackage::GetMarkerFile(eDcsPackage)))
		return ERR(STATUS_NOT_FOUND, L"No boot loader is installed on this EFI partition.");

	//
	// With a shim installed the entry points at the shim rather than at the
	// default loader path: Windows updates overwrite BOOTx64.efi, and an entry
	// naming the shim directly survives that.
	//
	bool bShim = CEspLocator::FileExists(Esp.Path, CBootPackage::GetMarkerFile(eShimPackage));

	std::wstring ExecPath = bShim
		? CBootPackage::GetMarkerFile(eShimPackage)
		: CBootPackage::GetMarkerFile(eDcsPackage);

	std::wstring Description = Options.Description;
	if (bShim)
		Description += L" (SHIM)";

	std::vector<unsigned char> Entry = BuildLoadOption(Esp, Description, ExecPath);

	//
	// Only written when it differs. Firmware variable storage is flash with a
	// finite write budget, and re-running an install should not spend any of it.
	//
	std::wstring Name = CEfiVars::BootEntryName(DCS_BOOT_ENTRY_ID);
	std::vector<unsigned char> Existing = CEfiVars::Get(Name);
	if (Existing != Entry)
	{
		Status = CEfiVars::Set(Name, EFI_GLOBAL_VAR_GUID, Entry);
		if (Status.IsError())
			return Status;
	}

	/* --- BootOrder --- */

	std::vector<unsigned short> Order = CEfiVars::GetBootOrder();

	size_t uAt = (size_t)-1;
	for (size_t i = 0; i < Order.size(); i++) {
		if (Order[i] == DCS_BOOT_ENTRY_ID) { uAt = i; break; }
	}

	//
	// Whether to push ahead of the first entry depends on what that entry is.
	// Displacing Windows is the point of the entry; displacing another
	// operating system's loader, or a firmware utility, is not something to do
	// uninvited, so that case is left where it is.
	//
	bool bFirstIsWindows = false;
	if (!Order.empty() && uAt != 0)
	{
		std::vector<unsigned char> First = CEfiVars::Get(CEfiVars::BootEntryName(Order[0]));
		bFirstIsWindows = BufferHasWideString(First, MSFT_BOOT_LOADER);
	}

	bool bChanged = false;
	if (uAt == (size_t)-1)
	{
		if (bFirstIsWindows)
			Order.insert(Order.begin(), (unsigned short)DCS_BOOT_ENTRY_ID);
		else
			Order.push_back((unsigned short)DCS_BOOT_ENTRY_ID);
		bChanged = true;
	}
	else if (uAt > 0 && bFirstIsWindows)
	{
		Order.erase(Order.begin() + uAt);
		Order.insert(Order.begin(), (unsigned short)DCS_BOOT_ENTRY_ID);
		bChanged = true;
	}

	if (bChanged)
	{
		Status = CEfiVars::SetBootOrder(Order);
		if (Status.IsError())
			return Status;
	}

	return OK;
}

STATUS CEfiInstall::DelBootEntry()
{
	STATUS Status = CEfiVars::Init();
	if (Status.IsError())
		return Status;

	CEfiVars::Delete(CEfiVars::BootEntryName(DCS_BOOT_ENTRY_ID));

	std::vector<unsigned short> Order = CEfiVars::GetBootOrder();

	bool bChanged = false;
	for (size_t i = 0; i < Order.size(); ) {
		if (Order[i] == DCS_BOOT_ENTRY_ID) {
			Order.erase(Order.begin() + i);
			bChanged = true;
		}
		else
			i++;
	}

	if (bChanged)
		CEfiVars::SetBootOrder(Order);

	//
	// And take ourselves out of BootNext, which would otherwise point the next
	// boot at an entry that no longer exists.
	//
	std::vector<unsigned char> Next = CEfiVars::Get(L"BootNext");
	if (Next.size() == sizeof(unsigned short))
	{
		unsigned short uNext = (unsigned short)(Next[0] | (Next[1] << 8));
		if (uNext == DCS_BOOT_ENTRY_ID)
			CEfiVars::Delete(L"BootNext");
	}

	return OK;
}

/* ---------------------------------------------------------------------------
 * Configuration
 * ------------------------------------------------------------------------ */

RESULT(CDcsConfig) CEfiInstall::LoadConfig(int iDisk, int iPartition)
{
	RESULT(SEspInfo) EspRes = CEspLocator::FindEsp(iDisk, iPartition);
	if (EspRes.IsError())
		return EspRes;

	CDcsConfig Config;
	STATUS Status = Config.Load(EspRes.GetValue().Path);
	if (Status.IsError())
		return Status;

	return CResult<CDcsConfig>(0, Config);
}

STATUS CEfiInstall::SaveConfig(const CDcsConfig& Config, int iDisk, int iPartition)
{
	RESULT(SEspInfo) EspRes = CEspLocator::FindEsp(iDisk, iPartition);
	if (EspRes.IsError())
		return EspRes;

	return Config.Save(EspRes.GetValue().Path);
}
