#pragma once
#include "../Programs/ProgramId.h"

class CPackageList
{
	TRACK_OBJECT(CPackageList)
public:
	CPackageList();

	void Init();

	void Update();

	struct SPackage
	{
		TRACK_OBJECT(SPackage)

		std::wstring PackageSid;
		
		std::wstring PackageFullName;

		std::wstring PackageName;
		std::wstring PackageFamilyName;
		std::wstring PackageVersion;

		std::wstring PackageInstallPath;
		std::wstring PackageDisplayName;
		std::wstring SmallLogoPath;
	
		//std::wstring AppUserModelId;
	};

	typedef std::shared_ptr<SPackage> SPackagePtr;

	bool LoadFromCache();
	void StoreToCache();

protected:
	//friend DWORD CALLBACK CPackageList__ThreadProc(LPVOID lpThreadParameter);
	//
	//void UpdateAsync();
	//
	//HANDLE m_hUpdater = NULL;

	struct SEnumParams
	{
		CPackageList* pThis;
		std::map<std::wstring, SPackagePtr> OldList;
	};

	static BOOLEAN EnumCallBack(PVOID param, void* AppPackage, void* AppPackage2);

	std::shared_mutex  m_Mutex;

	std::map<std::wstring, SPackagePtr> m_List; // by PackageFullName
	//std::multimap<std::wstring, SPackagePtr> m_ListBySID;
};