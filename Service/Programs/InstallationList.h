#pragma once
#include "../Programs/ProgramId.h"

class CInstallationList
{
	TRACK_OBJECT(CInstallationList)
public:
	CInstallationList();

	void Init();

	void Update();

	struct SInstallation
	{
		std::wstring RegKey;
		
		std::wstring DisplayName;
		std::wstring DisplayIcon;
		std::wstring DisplayVersion;

		std::wstring UninstallString;
		std::wstring ModifyPath;

		std::wstring InstallPath;
	};

	typedef std::shared_ptr<SInstallation> SInstallationPtr;

protected:

	struct SEnumParams
	{
		CInstallationList* pThis;
		std::map<std::wstring, SInstallationPtr> OldList;
	};

	static VOID EnumCallBack(PVOID param, const std::wstring& RegKey);

	static void EnumInstallations(const std::wstring& RegKey, VOID(*CallBack)(PVOID param, const std::wstring& RegKey), PVOID param);

	std::shared_mutex  m_Mutex;

	std::map<std::wstring, SInstallationPtr> m_List; // by RegKey
};