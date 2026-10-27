/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

#include "pch.h"
#include "BootPackage.h"

#include <windows.h>

//
// A zip reader, statically linked - see zip/zip.h. It is fed the archive as a
// block of memory rather than a file name, so every path in this file stays
// wide; the library itself only ever sees bytes.
//
#include "zip/zip.h"

/* ---------------------------------------------------------------------------
 * CBootPackage
 * ------------------------------------------------------------------------ */

#if defined(_M_ARM64)
 #define BOOT_PKG_ARCH  L"AA64"
#elif defined(_M_IX86)
 #define BOOT_PKG_ARCH  L"IA32"
#else
 #define BOOT_PKG_ARCH  L"X64"
#endif

//
// What a MajorPrivacy install puts on the EFI partition.
//
// DcsInt is deliberately absent. It is the module that does the disk
// encryption, which MajorPrivacy does not use - its volumes are containers, not
// encrypted partitions - so the boot loader is installed for the pinned user
// key in DcsOwner and nothing else. It is in the archive; it is simply never
// named here, and so never lands anywhere.
//
static const SBootFile g_DcsFiles[] = {
	//											required	recovery only	DiskCryptor
	// the boot file first, because everything else is only reachable through it
	{ L"DcsBoot.efi",		L"\\EFI\\DCS\\DcsBoot.efi",		true,	false,	false },
	{ L"DcsOwner.dcs",		L"\\EFI\\DCS\\DcsOwner.dcs",	true,	false,	false },
	{ L"DcsInfo.dcs",		L"\\EFI\\DCS\\DcsInfo.dcs",		true,	false,	false },
	//
	// DiskCryptor full-disk-encryption support. Always listed so an update can find
	// and refresh them, but installed only with the "DiskCryptor support" option, and
	// on update refreshed only when already present (see SBootFile::bDiskCryptor).
	//
	{ L"DcsInt.dcs",		L"\\EFI\\DCS\\DcsInt.dcs",		false,	false,	true  },
	{ L"DcsTpm.dcs",		L"\\EFI\\DCS\\DcsTpm.dcs",		false,	false,	true  },
#if !defined(_M_ARM64)
	// PC speaker beeps; there is no equivalent on ARM64 and no module either
	{ L"LegacySpeaker.dcs",	L"\\EFI\\DCS\\LegacySpeaker.dcs",	false,	false,	false },
#endif
	//
	// The rescue menu, and rescue media only.
	//
	// efiinst.c keeps it off an install by passing a count one short of its
	// table - DcsRe being the last row - and this flag is that same rule in a
	// form a row added underneath cannot break.
	//
	// It was first written down as merely optional, and an install that copied
	// it did not boot: DcsLdr took the presence of \EFI\DCS\DcsRe.efi to mean
	// it was running off a rescue disk and opened the menu instead of the
	// loader, every time. DcsLdr no longer does that, but the file still has no
	// business on a system EFI partition - and a machine that still has the
	// older loader on it is a reason of its own to keep it off.
	//
	{ L"DcsRe.efi",			L"\\EFI\\DCS\\DcsRe.efi",		false,	true,	false },
};

#if defined(_M_ARM64)
 #define SHIM_EFI		L"shimaa64.efi"
 #define SHIM_MM_EFI	L"mmaa64.efi"
#elif defined(_M_IX86)
 #define SHIM_EFI		L"shimia32.efi"
 #define SHIM_MM_EFI	L"mmia32.efi"
#else
 #define SHIM_EFI		L"shimx64.efi"
 #define SHIM_MM_EFI	L"mmx64.efi"
#endif

static const SBootFile g_ShimFiles[] = {
	//											required	recovery only
	{ SHIM_EFI,				L"\\EFI\\Boot\\" SHIM_EFI,		true,	false },
	{ SHIM_MM_EFI,			L"\\EFI\\Boot\\" SHIM_MM_EFI,	true,	false },	// MokManager
	{ L"CustomSigner.der",	L"\\EFI\\Boot\\CustomSigner.der",	true,	false },
	{ L"DcsLdr.efi",		L"\\EFI\\Boot\\DcsLdr.efi",		true,	false },
};

std::vector<SBootFile> CBootPackage::GetFiles(EBootPackage Package)
{
	const SBootFile* pFiles = Package == eShimPackage ? g_ShimFiles : g_DcsFiles;
	size_t uCount = Package == eShimPackage ? _countof(g_ShimFiles) : _countof(g_DcsFiles);

	return std::vector<SBootFile>(pFiles, pFiles + uCount);
}

std::wstring CBootPackage::GetMarkerFile(EBootPackage Package)
{
	//
	// DiskCryptor tests for DcsInt to decide whether DCS is installed. That one
	// is never copied here, so the boot file stands in - it is the one thing
	// every install has and no other product writes.
	//
	return GetFiles(Package).front().Target;
}

std::wstring CBootPackage::GetAppDirectory()
{
	wchar_t Path[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, Path, MAX_PATH);

	std::wstring Dir = Path;
	size_t uSlash = Dir.find_last_of(L'\\');
	if (uSlash != std::wstring::npos)
		Dir.resize(uSlash);
	return Dir;
}

std::wstring CBootPackage::GetFolderName(EBootPackage Package)
{
	return (Package == eShimPackage ? std::wstring(L"ShimPkg_") : std::wstring(L"DcsPkg_")) + BOOT_PKG_ARCH;
}

std::wstring CBootPackage::GetArchiveName(EBootPackage Package)
{
	return GetFolderName(Package) + L".zip";
}

bool CBootPackage::IsAvailable(EBootPackage Package)
{
	std::wstring Dir = GetAppDirectory();

	if (GetFileAttributesW((Dir + L"\\" + GetFolderName(Package)).c_str()) != INVALID_FILE_ATTRIBUTES)
		return true;
	return GetFileAttributesW((Dir + L"\\" + GetArchiveName(Package)).c_str()) != INVALID_FILE_ATTRIBUTES;
}

/* ---------------------------------------------------------------------------
 * Reading a package
 * ------------------------------------------------------------------------ */

//
// Entry names in a zip are bytes. Modern writers mark them UTF-8 with a flag in
// the header; older ones leave them in CP437. A boot package only ever holds
// plain ASCII names, where the two agree, so UTF-8 is assumed and anything that
// is not valid UTF-8 simply fails to convert - which means it matches nothing.
//
static bool Utf8ToWide(const char* pStr, std::wstring& Out)
{
	if (!pStr)
		return false;

	int iLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pStr, -1, NULL, 0);
	if (iLen <= 0)
		return false;

	Out.resize((size_t)iLen - 1);
	if (iLen > 1 && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pStr, -1, &Out[0], iLen) <= 0)
		return false;

	return true;
}

// as much as is asked of ReadFile at a time
static const size_t CHUNK = 1 << 20;

static bool ReadWholeFile(const std::wstring& Path, std::vector<unsigned char>& Data)
{
	HANDLE hFile = CreateFileW(Path.c_str(), GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	LARGE_INTEGER Size = { 0 };
	if (!GetFileSizeEx(hFile, &Size) || Size.QuadPart < 0 || Size.QuadPart > (64 << 20)) {
		CloseHandle(hFile);
		return false;
	}

	Data.resize((size_t)Size.QuadPart);

	size_t uAt = 0;
	while (uAt < Data.size())
	{
		DWORD uRead = 0;
		size_t uLeft = Data.size() - uAt;
		if (!::ReadFile(hFile, &Data[uAt], (DWORD)(uLeft < CHUNK ? uLeft : CHUNK), &uRead, NULL) || uRead == 0)
			break;
		uAt += uRead;
	}
	CloseHandle(hFile);

	return uAt == Data.size();
}

//
// What a source is holding: either a folder, or the archive bytes with the zip
// reader parked on top of them. The bytes have to outlive the handle, which is
// why they live here and not on the stack of whoever opened it.
//
struct SBootPackageImpl
{
	EBootPackage				Package;
	bool						bOpen = false;

	std::wstring				Folder;		// set when serving from an unpacked folder
	std::vector<unsigned char>	Blob;		// the archive, when serving from one
	struct zip_t*				pZip = NULL;

	SBootPackageImpl(EBootPackage P) : Package(P) {}
};

CBootPackageSource::CBootPackageSource(EBootPackage Package)
{
	m = new SBootPackageImpl(Package);
}

CBootPackageSource::~CBootPackageSource()
{
	if (m) {
		if (m->pZip) zip_stream_close(m->pZip);
		delete m;
	}
}

STATUS CBootPackageSource::Open()
{
	if (m->bOpen)
		return OK;

	std::wstring Dir = CBootPackage::GetAppDirectory();

	//
	// An unpacked folder wins over the archive. Nothing here ever creates one;
	// it is there because somebody put it there, which is the whole point.
	//
	std::wstring Folder = Dir + L"\\" + CBootPackage::GetFolderName(m->Package);
	DWORD uAttr = GetFileAttributesW(Folder.c_str());
	if (uAttr != INVALID_FILE_ATTRIBUTES && (uAttr & FILE_ATTRIBUTE_DIRECTORY)) {
		m->Folder = Folder;
		m->bOpen = true;
		return OK;
	}

	std::wstring Archive = Dir + L"\\" + CBootPackage::GetArchiveName(m->Package);
	if (GetFileAttributesW(Archive.c_str()) == INVALID_FILE_ATTRIBUTES) {
		return ERR(STATUS_NOT_FOUND, L"Neither " + CBootPackage::GetFolderName(m->Package) + L" nor "
			+ CBootPackage::GetArchiveName(m->Package) + L" was found next to MajorPrivacy.exe.");
	}

	//
	// The archive is read whole and handed to the zip reader as memory. A boot
	// package is a few hundred kilobytes, so this costs nothing, and it keeps the
	// reader away from file names entirely - it has no wide-character entry
	// point, and the install path is the user's to choose.
	//
	if (!ReadWholeFile(Archive, m->Blob))
		return ERR(STATUS_UNSUCCESSFUL, L"Could not read " + Archive + L".");

	m->pZip = zip_stream_open((const char*)m->Blob.data(), m->Blob.size(), 0, 'r');
	if (!m->pZip)
		return ERR(STATUS_UNSUCCESSFUL, Archive + L" is not a readable zip archive.");

	m->bOpen = true;
	return OK;
}

//
// Walk the archive for an entry of exactly this name. Returns its index, or -1.
//
// Matching is whole-name and case-insensitive, never a path operation: an entry
// calling itself "..\\DcsBoot.efi" is not the same string as "DcsBoot.efi" and so
// is never found. Since nothing downstream builds a path out of an archive name
// either - targets come from the table in this file - an archive cannot steer
// where anything lands.
//
static int FindEntry(struct zip_t* pZip, const std::wstring& Name)
{
	int iTotal = zip_total_entries(pZip);
	for (int i = 0; i < iTotal; i++)
	{
		if (zip_entry_openbyindex(pZip, i) != 0)
			return -1;

		std::wstring Entry;
		bool bNamed = Utf8ToWide(zip_entry_name(pZip), Entry);
		bool bDir = zip_entry_isdir(pZip) == 1;
		zip_entry_close(pZip);

		if (bNamed && !bDir && _wcsicmp(Entry.c_str(), Name.c_str()) == 0)
			return i;
	}
	return -1;
}

bool CBootPackageSource::Contains(const std::wstring& Name) const
{
	if (!m->bOpen)
		return false;

	if (!m->Folder.empty())
		return GetFileAttributesW((m->Folder + L"\\" + Name).c_str()) != INVALID_FILE_ATTRIBUTES;

	return FindEntry(m->pZip, Name) >= 0;
}

STATUS CBootPackageSource::Read(const std::wstring& Name, std::vector<unsigned char>& Data) const
{
	Data.clear();

	if (!m->bOpen)
		return ERR(STATUS_INVALID_DEVICE_STATE, L"The boot loader package has not been opened.");

	if (!m->Folder.empty())
	{
		if (!ReadWholeFile(m->Folder + L"\\" + Name, Data))
			return ERR(STATUS_NOT_FOUND, L"The boot loader package does not contain " + Name + L".");
		return OK;
	}

	int iIndex = FindEntry(m->pZip, Name);
	if (iIndex < 0)
		return ERR(STATUS_NOT_FOUND, L"The boot loader package does not contain " + Name + L".");

	if (zip_entry_openbyindex(m->pZip, iIndex) != 0)
		return ERR(STATUS_UNSUCCESSFUL, L"Could not read " + Name + L" from the boot loader package.");

	unsigned long long uSize = zip_entry_size(m->pZip);
	if (uSize > (64ull << 20)) {
		zip_entry_close(m->pZip);
		return ERR(STATUS_FILE_TOO_LARGE, L"Unexpected size for " + Name + L".");
	}

	Data.resize((size_t)uSize);

	bool bOk = uSize == 0 || zip_entry_noallocread(m->pZip, Data.data(), Data.size()) >= 0;
	zip_entry_close(m->pZip);

	if (!bOk) {
		Data.clear();
		return ERR(STATUS_UNSUCCESSFUL, L"Could not unpack " + Name + L" from the boot loader package.");
	}

	return OK;
}

/* ------------------------------------------------------------------------- */

bool CBootPackage::Contains(EBootPackage Package, const std::wstring& Name)
{
	CBootPackageSource Source(Package);
	if (Source.Open().IsError())
		return false;
	return Source.Contains(Name);
}

STATUS CBootPackage::Read(EBootPackage Package, const std::wstring& Name, std::vector<unsigned char>& Data)
{
	CBootPackageSource Source(Package);

	STATUS Status = Source.Open();
	if (Status.IsError())
		return Status;

	return Source.Read(Name, Data);
}
