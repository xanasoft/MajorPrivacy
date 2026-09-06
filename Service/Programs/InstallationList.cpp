#include "pch.h"
#include "InstallationList.h"
#include "ServiceCore.h"
#include "../Programs/ProgramManager.h"
#include "../../Framework/Common/Buffer.h"
#include "../../Library/Helpers/RegUtil.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../../Library/Helpers/NtObj.h"
#include "../../Library/Helpers/Scoped.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../Library/Common/DbgHelp.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Common/Exception.h"

CInstallationList::CInstallationList()
{
}

void CInstallationList::Init()
{
	Update();
}

static std::wstring ExtractInstallPath(const std::wstring& Command)
{
    if (Command.empty())
        return L"";

    // Skip if contains environment variables (not expanded)
    if (Command.find(L'%') != std::wstring::npos)
        return L"";

    std::wstring FilePath = Command;
    if (FilePath.at(0) == L'\"')
        FilePath = GetFileFromCommand(FilePath);

    // Extract directory from file path
    size_t pos = FilePath.find_last_of(L'\\');
    if (pos == std::wstring::npos || pos < 3)
        return L"";

    std::wstring DirPath = FilePath.substr(0, pos + 1);

    // Convert to lowercase for comparison
    std::wstring LowerPath = DirPath;
    std::transform(LowerPath.begin(), LowerPath.end(), LowerPath.begin(), ::towlower);

    // Reject Package Cache paths (cached installers, not actual install locations)
    if (LowerPath.find(L"\\package cache\\") != std::wstring::npos)
        return L"";

    // Reject temp paths
    if (LowerPath.find(L"\\temp\\") != std::wstring::npos)
        return L"";
    if (LowerPath.find(L"\\tmp\\") != std::wstring::npos)
        return L"";

    return DirPath;
}

VOID CInstallationList::EnumCallBack(PVOID param, const std::wstring& RegKey)
{
    SEnumParams* pParams = (SEnumParams*)param;

    CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
    if (!NT_SUCCESS(NtOpenKey((PHANDLE)&hKey, KEY_READ, SNtObject(RegKey.c_str(), NULL, NULL).Get())))
        return;

    std::wstring InstallLocation = RegQueryWString(hKey, L"InstallLocation");
    std::wstring UninstallString = RegQueryWString(hKey, L"UninstallString");

    // Try to extract install path from UninstallString or DisplayIcon if InstallLocation is empty
    if (InstallLocation.empty())
        InstallLocation = ExtractInstallPath(UninstallString);
    else if(InstallLocation.at(0) == L'\"')
        InstallLocation = InstallLocation.substr(1, InstallLocation.length() - 2);

    if (InstallLocation.empty())
        return;

    // ignore entries pointing to default windows locations
    if(theCore->ProgramManager()->IsPathReserved(InstallLocation))
        return;

    SInstallationPtr pInstalledApp;
    auto F = pParams->OldList.find(RegKey);
    if (F != pParams->OldList.end()) {
        pInstalledApp = F->second;
        if(pInstalledApp->InstallPath == InstallLocation)
            pParams->OldList.erase(F);
        else
            pInstalledApp.reset();
    }
    
    if(!pInstalledApp)
    {
        pInstalledApp = SInstallationPtr(new SInstallation());

        pInstalledApp->RegKey = RegKey;

        pInstalledApp->UninstallString = UninstallString;
        pInstalledApp->ModifyPath = RegQueryWString(hKey, L"ModifyPath");

        pInstalledApp->InstallPath = InstallLocation; // DOS Path

        pParams->NewList.insert(std::make_pair(RegKey, pInstalledApp));
    }

    pInstalledApp->DisplayName = RegQueryWString(hKey, L"DisplayName");
    pInstalledApp->DisplayVersion = RegQueryWString(hKey, L"DisplayVersion");
	pInstalledApp->DisplayIcon = RegQueryWString(hKey, L"DisplayIcon");
}

void CInstallationList::EnumInstallations(const std::wstring& RegKey, VOID(*CallBack)(PVOID param, const std::wstring& RegKey), PVOID param)
{
    CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
    if (!NT_SUCCESS(NtOpenKey((PHANDLE)&hKey, KEY_READ, SNtObject(RegKey.c_str(), NULL, NULL).Get())))
        return;

    std::wstring subKeyName;
    for (ULONG Index = 0; RegEnumKeys(hKey, Index, subKeyName); Index++)
        CallBack(param, RegKey + L"\\" + subKeyName);
}

void CInstallationList::Update()
{
    if(!theCore->Config()->GetBool("Service", "EnumInstallations", true))
        return;

    SEnumParams Params;
    Params.pThis = this;
    Params.OldList = m_List;

#ifdef _DEBUG
    uint64 start = GetUSTickCount();
#endif

    EnumInstallations(L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", EnumCallBack, &Params);
    EnumInstallations(L"\\REGISTRY\\MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", EnumCallBack, &Params);

    // TODO: enum also per user locations

#ifdef _DEBUG
    DbgPrint("EnumAllAppPackages took %llu ms cycles\r\n", (GetUSTickCount() - start) / 1000);
#endif

    for(auto E: Params.OldList) {
        m_List.erase(E.first);
        SInstallationPtr pInstalledApp = E.second;
        if (pInstalledApp) theCore->ProgramManager()->RemoveInstallation(pInstalledApp);
    }

	for (auto E : Params.NewList) {
        SInstallationPtr pInstalledApp = E.second;
        m_List.insert(std::make_pair(pInstalledApp->RegKey, pInstalledApp));
        //theCore->ProgramManager()->AddInstallation(pInstalledApp);
	}

    for (auto E : m_List) {
        SInstallationPtr pInstalledApp = E.second;
        theCore->ProgramManager()->AddInstallation(pInstalledApp);
    }
}