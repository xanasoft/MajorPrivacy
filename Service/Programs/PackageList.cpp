#include "pch.h"
#include "PackageList.h"
#include "ServiceCore.h"
#include "../Programs/ProgramManager.h"
#include "../../Library/Common/Buffer.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../Library/Common/DbgHelp.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Common/Exception.h"

CPackageList::CPackageList()
{
}

void CPackageList::Init()
{
    //bool bLoaded = 
    LoadFromCache();

    Update();

    //if (!bLoaded)
    StoreToCache();
}

//DWORD CALLBACK CPackageList__ThreadProc(LPVOID lpThreadParameter)
//{
//    CPackageList* This = (CPackageList*)lpThreadParameter;
//
//    This->UpdateAsync();
//
//    CloseHandle(This->m_hUpdater);
//    This->m_hUpdater = NULL;
//
//    return 0;
//}

BOOLEAN CPackageList::EnumCallBack(PVOID param, void* AppPackage, void* AppPackage2)
{
    SEnumParams* pParams = (SEnumParams*)param;

    std::wstring PackageName;       // windows.immersivecontrolpanel
    std::wstring PackageFamilyName; // windows.immersivecontrolpanel_cw5n1h2txyewy
    std::wstring PackageVersion;    // 10.0.1000.2
    std::wstring PackageFullName;   // windows.immersivecontrolpanel_10.0.2.1000_neutral_neutral_cw5n1h2txyewy
    GetAppPackageInfos(AppPackage, AppPackage2, 
        &PackageName, &PackageFullName, &PackageFamilyName, &PackageVersion, 
        NULL, NULL, NULL);

    if (PackageFullName.empty() || PackageFamilyName.empty())
        return TRUE;

    auto F = pParams->OldList.find(PackageFullName);
    SPackagePtr pAppPackage;
    if (F != pParams->OldList.end()) {
        pAppPackage = F->second;
        pParams->OldList.erase(F);
    }
    else
    {
        pAppPackage = SPackagePtr(new SPackage());

        pAppPackage->PackageFullName = PackageFullName; 
        pAppPackage->PackageSid = GetAppContainerSidFromName(PackageFamilyName);

        pAppPackage->PackageName = PackageName;
        pAppPackage->PackageFamilyName = PackageFamilyName;
        pAppPackage->PackageVersion = PackageVersion;

        std::wstring PackageInstallPath;
        GetAppPackageInfos(AppPackage, AppPackage2, 
            NULL, NULL, NULL, NULL,
            &pAppPackage->PackageDisplayName, &pAppPackage->SmallLogoPath, &PackageInstallPath);

        pAppPackage->PackageInstallPath = PackageInstallPath; // DOS Path

        pParams->pThis->m_List.insert(std::make_pair(PackageFullName, pAppPackage));
        theCore->ProgramManager()->AddPackage(pAppPackage);
    }

    return TRUE;
}

void CPackageList::Update()
{
    //std::unique_lock Lock(m_Mutex);
    //
    //if (m_hUpdater)
    //    return;
    //m_hUpdater = CreateThread(NULL, 0, CPackageList__ThreadProc, this, 0, NULL);

    SEnumParams Params;
    Params.pThis = this;
    Params.OldList = m_List;

#ifdef _DEBUG
    uint64 start = GetUSTickCount();
#endif

    EnumAllAppPackages(EnumCallBack, &Params);

#ifdef _DEBUG
    DbgPrint("EnumAllAppPackages took %d ms cycles\r\n", (GetUSTickCount() - start) / 1000);
#endif

    for(auto E: Params.OldList) {
        m_List.erase(E.first);
        SPackagePtr pAppPackage = E.second;
        if (pAppPackage) theCore->ProgramManager()->RemovePackage(pAppPackage);
    }
}

/*void CPackageList::UpdateAsync()
{
#ifdef _DEBUG
    uint64 start = GetUSTickCount();
#endif

    auto List = EnumAllAppPackages(); // !!! this is very slow
    if (List.IsError())
        return;

#ifdef _DEBUG
    DbgPrint("EnumAllAppPackages took %d ms cycles\r\n", (GetUSTickCount() - start) / 1000);
#endif

    std::unique_lock Lock(m_Mutex);

    auto OldList = m_ListBySID;

    for (auto pPackage: *List.GetValue())
    {
        std::wstring Id = pPackage->PackageSID;

        auto F = OldList.find(Id);
        SPackagePtr pAppPackage;
        if (F != OldList.end()) {
            pAppPackage = F->second;
            OldList.erase(F);
        }
        else
        {
            pAppPackage = SPackagePtr(new SPackage());
            pAppPackage->PackageSid = Id;
            pAppPackage->AppUserModelId = pPackage->AppUserModelId;
		    pAppPackage->PackageName = pPackage->PackageName;
		    pAppPackage->PackageDisplayName = pPackage->PackageDisplayName;
		    pAppPackage->PackageFamilyName = pPackage->PackageFamilyName;
		    pAppPackage->PackageInstallPath = DosPathToNtPath(pPackage->PackageInstallPath);
            pAppPackage->PackageFullName = pPackage->PackageFullName;
		    pAppPackage->PackageVersion = pPackage->PackageVersion;
		    pAppPackage->SmallLogoPath = pPackage->SmallLogoPath;

            m_ListBySID.insert(std::make_pair(Id, pAppPackage));

            theCore->ProgramManager()->AddPackage(pAppPackage);
        }
    }

    for(auto E: OldList) {
        m_ListBySID.erase(E.first);
        SPackagePtr pAppPackage = E.second;
        if (pAppPackage) theCore->ProgramManager()->RemovePackage(pAppPackage);
    }
}*/

bool CPackageList::LoadFromCache()
{
    CBuffer Buffer;
    if (!ReadFile(theCore->GetDataFolder() + L"\\AppPackages.dat", 0, Buffer))
        return false;

    CVariant List;
    //try {
    if(List.FromPacket(&Buffer, true) != CVariant::eErrNone)
		return false;
    //} catch (const CException&) {
    //    return false;
    //}

    for (uint32 i = 0; i < List.Count(); i++)
    {
        CVariant Package = List[i];
        SPackagePtr pAppPackage = SPackagePtr(new SPackage());

        pAppPackage->PackageFullName = Package["FullName"];
        pAppPackage->PackageSid = Package["Sid"];

        pAppPackage->PackageName = Package["Name"];
        pAppPackage->PackageFamilyName = Package["FamilyName"];
        pAppPackage->PackageVersion = Package["Version"];

        pAppPackage->PackageInstallPath = Package["InstallPath"];
        pAppPackage->PackageDisplayName = Package["DisplayName"];
        pAppPackage->SmallLogoPath = Package["LogoPath"];

        m_List.insert(std::make_pair(pAppPackage->PackageFullName, pAppPackage));
        theCore->ProgramManager()->AddPackage(pAppPackage);
    }

    return true;
}

void CPackageList::StoreToCache()
{
    CVariant List;

    for (auto I : m_List)
    {
        SPackagePtr pAppPackage = I.second;
        CVariant Package;

        Package["FullName"] = pAppPackage->PackageFullName;
		Package["Sid"] = pAppPackage->PackageSid;

		Package["Name"] = pAppPackage->PackageName;
		Package["FamilyName"] = pAppPackage->PackageFamilyName;
		Package["Version"] = pAppPackage->PackageVersion;

		Package["InstallPath"] = pAppPackage->PackageInstallPath;
		Package["DisplayName"] = pAppPackage->PackageDisplayName;
		Package["LogoPath"] = pAppPackage->SmallLogoPath;

        List.Append(Package);
    }

    CBuffer Buffer;
    List.ToPacket(&Buffer);
    WriteFile(theCore->GetDataFolder() + L"\\AppPackages.dat", 0, Buffer); 
}