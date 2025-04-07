#pragma once
#include "../lib_global.h"
#include "../Status.h"

class LIBRARY_EXPORT CPathReparse
{
public:
	CPathReparse();
	~CPathReparse();

	static CPathReparse* Instance();

	static void Dispose();

	static std::wstring GetNtPathFromHandle(HANDLE hFile);


	////////////////////////////////////////////////////////////////////////////////////////////
	// Drives

	struct SDrive 
	{
		WCHAR letter = 0;
		WCHAR sn[10] = {0};
		bool subst = false;
		std::wstring path;
	};

	bool InitDrives(ULONG DriveMask = -1);
	std::wstring GetDrivePath(ULONG DriveIndex);

	std::shared_ptr<const SDrive> GetDriveForPath(const std::wstring& Path);
	std::shared_ptr<const SDrive> GetDriveForUncPath(const std::wstring& Path, ULONG *OutPrefixLen);
	std::shared_ptr<const SDrive> GetDriveForLetter(WCHAR drive_letter);

	static ULONG GetVolumeSN(const std::wstring& DriveNtPath);

	////////////////////////////////////////////////////////////////////////////////////////////
	// File Links

	struct SLink 
	{
		ULONG ticks = 0;
		bool same = false;
		bool stop = false;
		std::wstring dst;
		std::wstring src;
	};

	std::wstring TranslateTempLinks(const std::wstring& TruePath, bool bReverse, bool StripLastPathComponent = false);
	std::wstring TranslateDosToNtPath(const std::wstring& DosPath, bool bAsIsOnError = true);
	std::wstring TranslateNtToDosPath(const std::wstring& path, bool bAsIsOnError = true);

	////////////////////////////////////////////////////////////////////////////////////////////
	// Guid Links

	struct SGuid 
	{
		WCHAR guid[38 + 1] = {0};
		std::wstring path;
	};

	std::shared_ptr<const SGuid> GetGuidForPath(const std::wstring& Path);
	std::shared_ptr<const SGuid> GetLinkForGuid(const std::wstring& guid_str);


	std::wstring TranslateGuidToNtPath(const std::wstring& GuidPath);

protected:
	static CPathReparse* m_pInstance;

	std::recursive_mutex m_Mutex;

	////////////////////////////////////////////////////////////////////////////////////////////
	// Drives

	void AdjustDrives(ULONG path_drive_index, bool subst, const std::wstring& path);

	std::vector<std::shared_ptr<SDrive>> m_Drives;

	////////////////////////////////////////////////////////////////////////////////////////////
	// File Links

	void InitLinksLocked();
	std::wstring GetName_TranslateSymlinks(const std::wstring& path, bool *translated);
	bool AddLink(bool PermLink, const std::wstring& Src, const std::wstring& Dst);
	void RemovePermLinks(const std::wstring& path);
	ULONG GetDrivePrefixLength(const std::wstring& work_str);
	std::wstring TranslateTempLinks_2(const std::wstring& input_str, bool bReverse);
	std::shared_ptr<SLink> AddTempLink(const std::wstring&path);
	std::shared_ptr<SLink> AddReverseTempLink(const std::wstring& path);
	std::wstring FixPermLinksForTempLink(const std::wstring& name, bool bReverse);
	void GetDriveAndLinkForPath(const std::wstring& Path, std::shared_ptr<const SDrive>& OutDrive, std::shared_ptr<const SLink>& OutLink);
	std::shared_ptr<SLink> FindPermLinksForMatchPath(const std::wstring& name, bool bReverse);
	std::wstring FixPermLinksForMatchPath(const std::wstring& name);

	std::list<std::shared_ptr<SLink>> m_PermLinks;
	std::list<std::shared_ptr<SLink>> m_TempLinks;

	////////////////////////////////////////////////////////////////////////////////////////////
	// Guid Links

	std::vector<std::shared_ptr<SGuid>> m_GuidLinks;
};
