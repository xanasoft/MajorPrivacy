#include "pch.h"
#include "InstallationList.h"
#include "ServiceCore.h"
#include "../Programs/ProgramManager.h"
#include "../../Framework/Common/Buffer.h"
#include "../../Library/Helpers/RegUtil.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../../Library/Helpers/NtObj.h"
#include "../../Library/Helpers/Scoped.h"
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

VOID CInstallationList::EnumCallBack(PVOID param, const std::wstring& RegKey)
{
    SEnumParams* pParams = (SEnumParams*)param;

    CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
    if (!NT_SUCCESS(NtOpenKey((PHANDLE)&hKey, KEY_READ, SNtObject(RegKey.c_str(), NULL, NULL).Get())))
        return;

    std::wstring InstallLocation = RegQueryWString(hKey, L"InstallLocation");
    std::wstring UninstallString = RegQueryWString(hKey, L"UninstallString");

    if (InstallLocation.empty()/* && UninstallString.empty()*/)
        return;

    if(InstallLocation.at(0) == L'\"')
        InstallLocation = InstallLocation.substr(1, InstallLocation.length() - 2);

    bool bAdd = false;
    auto F = pParams->OldList.find(RegKey);
    SInstallationPtr pInstalledApp;
    if (F != pParams->OldList.end()) {
        pInstalledApp = F->second;
        pParams->OldList.erase(F);
    }
    else
    {
        pInstalledApp = SInstallationPtr(new SInstallation());

        pInstalledApp->RegKey = RegKey;

        pInstalledApp->UninstallString = UninstallString;
        pInstalledApp->ModifyPath = RegQueryWString(hKey, L"ModifyPath");

        pInstalledApp->InstallPath = InstallLocation; // DOS Path

        if(!theCore->ProgramManager()->IsPathReserved(InstallLocation))
            bAdd = true;
    }

    pInstalledApp->DisplayName = RegQueryWString(hKey, L"DisplayName");
    pInstalledApp->DisplayIcon = RegQueryWString(hKey, L"DisplayIcon");
    pInstalledApp->DisplayVersion = RegQueryWString(hKey, L"DisplayVersion");

    if (bAdd) {
        pParams->pThis->m_List.insert(std::make_pair(RegKey, pInstalledApp));
        theCore->ProgramManager()->AddInstallation(pInstalledApp);
    }
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
}