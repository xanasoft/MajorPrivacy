#include "pch.h"
#include "Tweak.h"
#include "..\ServiceCore.h"
#include "..\ServiceAPI.h"
#include "..\Library\Helpers\RegUtil.h"
#include "..\Library\Helpers\GPOUtil.h"
#include "TweakManager.h"
#include "..\Library\Helpers\Scoped.h"
#include "..\Library\Helpers\Service.h"
#include "..\Library\Helpers\SchdUtil.h"

bool SWinVer::Test() const
{
    //ULONG majorVersion = g_OsVersion.dwMajorVersion;
    //ULONG minorVersion = g_OsVersion.dwMinorVersion;
    ULONG buildVersion = g_OsVersion.dwBuildNumber;

    // Windows 10 home and or pro ignore some policys
    if(buildVersion >= 10000 && Win10Ed > m_OsEdition)
        return false;

    if (MinBuild && buildVersion < MinBuild)
        return false;
    if (MaxBuild && buildVersion > MaxBuild)
        return false;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
// CAbstractTweak

CVariant CAbstractTweak::ToVariant() const
{
	std::unique_lock lock(m_Mutex);

	CVariant Data;
	Data.BeginMap();
	
	WriteVariant(Data);

	Data.Finish();
	return Data;
}

void CAbstractTweak::WriteVariant(CVariant& Data) const
{
	Data.Write(SVC_API_TWEAK_NAME, m_Name);

    switch (m_Hint)
    {
    case ETweakHint::eNone:         Data.Write(SVC_API_TWEAK_HINT, SVC_API_TWEAK_HINT_NONE); break;
    case ETweakHint::eRecommended:  Data.Write(SVC_API_TWEAK_HINT, SVC_API_TWEAK_HINT_RECOMMENDED); break;
    }

    ETweakStatus Status = GetStatus();
    switch (Status)
    {
    case ETweakStatus::eNotSet:     Data.Write(SVC_API_TWEAK_STATUS, SVC_API_TWEAK_NOT_SET); break;
    case ETweakStatus::eApplied:    Data.Write(SVC_API_TWEAK_STATUS, SVC_API_TWEAK_APPLIED); break;
    case ETweakStatus::eSet:        Data.Write(SVC_API_TWEAK_STATUS, SVC_API_TWEAK_SET); break;
    case ETweakStatus::eMissing:    Data.Write(SVC_API_TWEAK_STATUS, SVC_API_TWEAK_MISSING); break;
    default:                        Data.Write(SVC_API_TWEAK_STATUS, SVC_API_TWEAK_TYPE_GROUP); 
    }
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweakList

bool CTweakList::IsAvailable() const
{
    for (auto pTweak : m_List) {
        if (pTweak->IsAvailable())
            return true;
    }
    return false;
}

STATUS CTweakList::Apply(ETweakMode Mode)
{
    STATUS Status = OK;
    for (auto pTweak : m_List) {
        if (!pTweak->IsAvailable())
            continue;
        if (pTweak->GetHint() != ETweakHint::eRecommended && !((int)Mode & (int)ETweakMode::eAll))
            continue;
        Status = pTweak->Apply(Mode);
        if (!Status) break;
    }
    return Status;
}

STATUS CTweakList::Undo(ETweakMode Mode)
{
    for (auto pTweak : m_List) {
        if (!pTweak->IsAvailable())
            continue;
        pTweak->Undo(Mode);
    }
    return OK;
}

void CTweakList::WriteVariant(CVariant& Data) const
{
	CAbstractTweak::WriteVariant(Data);

    CVariant List;
	List.BeginList();
	for (auto I : m_List)
		List.WriteVariant(I->ToVariant());
	List.Finish();
	Data.WriteVariant(SVC_API_TWEAK_LIST, List);
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweakGroup

void CTweakGroup::WriteVariant(CVariant& Data) const
{
	CTweakList::WriteVariant(Data);

	Data.Write(SVC_API_TWEAK_TYPE, SVC_API_TWEAK_TYPE_GROUP);
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweakSet

ETweakStatus CTweakSet::GetStatus() const
{
    int State = (int)ETweakStatus::eNotSet;
    for (auto pTweak : m_List) {
        ETweakStatus eState = pTweak->GetStatus();
        if ((int)eState > State)
            State = (int)eState;
    }
    return (ETweakStatus)State;
}

void CTweakSet::WriteVariant(CVariant& Data) const
{
	CTweakList::WriteVariant(Data);

	Data.Write(SVC_API_TWEAK_TYPE, SVC_API_TWEAK_TYPE_SET);
}


///////////////////////////////////////////////////////////////////////////////////////
// CTweak

STATUS CTweak::Apply(ETweakMode Mode)
{
    if (!((int)Mode & (int)ETweakMode::eDontSet))
        m_Set = true;
    return OK;
}

STATUS CTweak::Undo(ETweakMode Mode)
{
    if (!((int)Mode & (int)ETweakMode::eDontSet))
        m_Set = false;
    return OK;
}

void CTweak::WriteVariant(CVariant& Data) const
{
	CAbstractTweak::WriteVariant(Data);

	Data.Write(SVC_API_TWEAK_SET, m_Set);
}


//
// ************************************************************************************
// Tweaks
//

////////////////////////////////////////////
// CRegTweak

ETweakStatus CRegTweak::GetStatus() const
{
    std::unique_lock Lock(m_Mutex);

    std::pair<HKEY, const wchar_t*> RegPath = SplitRegKeyPath(m_Key);

    CVariant Value = RegQuery(RegPath.first, RegPath.second, m_Value.c_str());
    if (Value == m_Data)
		return m_Set ? ETweakStatus::eSet : ETweakStatus::eApplied;
	return m_Set ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CRegTweak::Apply(ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Apply(Mode);

    std::pair<HKEY, const wchar_t*> RegPath = SplitRegKeyPath(m_Key);

    if (RegSet(RegPath.first, RegPath.second, m_Value.c_str(), m_Data) != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);
    return OK;
}

STATUS CRegTweak::Undo(ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Undo(Mode);

    std::pair<HKEY, const wchar_t*> RegPath = SplitRegKeyPath(m_Key);

    CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
    if(RegOpenKeyExW(RegPath.first, RegPath.second, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return OK;
    if(RegDeleteValueW(hKey, m_Value.c_str()) != ERROR_SUCCESS)
		return ERR(STATUS_UNSUCCESSFUL);
	return OK;    
}

void CRegTweak::WriteVariant(CVariant& Data) const
{
	CTweak::WriteVariant(Data);

    Data.Write(SVC_API_TWEAK_TYPE, SVC_API_TWEAK_TYPE_REG);

	Data.Write(SVC_API_TWEAK_KEY, m_Key);
    Data.Write(SVC_API_TWEAK_VALUE, m_Value);
    Data.WriteVariant(SVC_API_TWEAK_DATA, m_Data);
}

////////////////////////////////////////////
// CGpoTweak

ETweakStatus CGpoTweak::GetStatus() const
{
    CGPO* pGPO = svcCore->TweakManager()->GetGPOLocked();

    std::unique_lock Lock(m_Mutex);

    std::pair<HKEY, const wchar_t*> RegPath = SplitRegKeyPath(m_Key);

    CScopedHandle hRoot = CScopedHandle((HKEY)0, RegCloseKey);
    if (RegPath.first == HKEY_LOCAL_MACHINE)
        *&hRoot = pGPO->GetMachineRoot();
    else if (RegPath.first == HKEY_CURRENT_USER)
        *&hRoot = pGPO->GetUserRoot();
    if(!hRoot)
		return m_Set ? ETweakStatus::eMissing : ETweakStatus::eNotSet;

    CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
    if(RegOpenKeyExW(hRoot, RegPath.second, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return m_Set ? ETweakStatus::eMissing : ETweakStatus::eNotSet;

    CVariant Value = RegQuery(hKey, m_Value.c_str());
    if (Value == m_Data)
        return m_Set ? ETweakStatus::eSet : ETweakStatus::eApplied;
    return m_Set ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CGpoTweak::Apply(ETweakMode Mode)
{
    CGPO* pGPO = svcCore->TweakManager()->GetGPOLocked();

    std::unique_lock Lock(m_Mutex);

    std::pair<HKEY, const wchar_t*> RegPath = SplitRegKeyPath(m_Key);

    CScopedHandle hRoot = CScopedHandle((HKEY)0, RegCloseKey);
    if (RegPath.first == HKEY_LOCAL_MACHINE)
        *&hRoot = pGPO->GetMachineRoot();
    else if (RegPath.first == HKEY_CURRENT_USER)
        *&hRoot = pGPO->GetUserRoot();
    if(!hRoot)
        return ERR(STATUS_UNSUCCESSFUL);

    CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
    if(RegCreateKeyExW(hRoot, RegPath.second, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);

    if(RegSet(hKey, m_Value.c_str(), m_Data) != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);
    if (!pGPO->Save())
        return ERR(STATUS_UNSUCCESSFUL);
    return OK;
}

STATUS CGpoTweak::Undo(ETweakMode Mode)
{
    CGPO* pGPO = svcCore->TweakManager()->GetGPOLocked();

    std::unique_lock Lock(m_Mutex);

    std::pair<HKEY, const wchar_t*> RegPath = SplitRegKeyPath(m_Key);

    CScopedHandle hRoot = CScopedHandle((HKEY)0, RegCloseKey);
    if (RegPath.first == HKEY_LOCAL_MACHINE)
        *&hRoot = pGPO->GetMachineRoot();
    else if (RegPath.first == HKEY_CURRENT_USER)
        *&hRoot = pGPO->GetUserRoot();
    if(!hRoot)
        return ERR(STATUS_UNSUCCESSFUL);

    CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
    if(RegCreateKeyExW(hRoot, RegPath.second, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return OK;

    if(RegDeleteValueW(hKey, m_Value.c_str()) != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);
    if (!pGPO->Save())
        return ERR(STATUS_UNSUCCESSFUL);
    return OK;    
}

void CGpoTweak::WriteVariant(CVariant& Data) const
{
	CTweak::WriteVariant(Data);
    
    Data.Write(SVC_API_TWEAK_TYPE, SVC_API_TWEAK_TYPE_GPO);

	Data.Write(SVC_API_TWEAK_KEY, m_Key);
    Data.Write(SVC_API_TWEAK_VALUE, m_Value);
    Data.WriteVariant(SVC_API_TWEAK_DATA, m_Data);
}

////////////////////////////////////////////
// CSvcTweak

const WCHAR* g_RegKeySCM = L"SYSTEM\\CurrentControlSet\\Services\\";

bool CSvcTweak::IsAvailable() const
{
    if(!CTweak::IsAvailable())
		return false;

    std::unique_lock Lock(m_Mutex);

	DWORD SvcStart = RegQueryDWord(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"Start", -1);
	if(SvcStart == -1)
		return false;
	return true;
}

ETweakStatus CSvcTweak::GetStatus() const
{
    std::unique_lock Lock(m_Mutex);

    // faster then querygin the SCM
    DWORD SvcStart = RegQueryDWord(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"Start", -1);

    if(SvcStart == SERVICE_DISABLED)
        return m_Set ? ETweakStatus::eSet : ETweakStatus::eApplied;
    return m_Set ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CSvcTweak::Apply(ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Apply(Mode);

    DWORD SvcStart = RegQueryDWord(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"Start", -1);
    RegSetDWord(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"OldStart", SvcStart);

    return SetServiceStart(m_SvcTag.c_str(), SERVICE_DISABLED);
}

STATUS CSvcTweak::Undo(ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Undo(Mode);

    DWORD SvcStart = RegQueryDWord(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"OldStart", -1);
    if (SvcStart != -1){
        STATUS Status = SetServiceStart(m_SvcTag.c_str(), SvcStart);
        if (!Status)
			return Status;
        RegDelValue(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"OldStart");
	}
    return ERR(STATUS_NOT_FOUND);
}

void CSvcTweak::WriteVariant(CVariant& Data) const
{
	CTweak::WriteVariant(Data);
    
    Data.Write(SVC_API_TWEAK_TYPE, SVC_API_TWEAK_TYPE_SVC);

	Data.Write(SVC_API_TWEAK_SVC_TAG, m_SvcTag);
}

////////////////////////////////////////////
// CTaskTweak

bool IsTaskEnabledEx(const std::wstring& path, const std::wstring& taskName)
{
    if (taskName == L"*")
    {
        std::vector<std::wstring> tasks = EnumTasks(path);
		for (auto task : tasks) {
			if (!IsTaskEnabled(path, task))
				return false;
		}
		return true;
	}
    
    return IsTaskEnabled(path, taskName);
}

bool SetTaskEnabledEx(const std::wstring& path, const std::wstring& taskName, bool setValue)
{
    if (taskName == L"*")
    {
        std::vector<std::wstring> tasks = EnumTasks(path);
        for (auto task : tasks) {
            SetTaskEnabledEx(path, task, setValue);
        }
        return true;
    }

    return SetTaskEnabledEx(path, taskName, setValue);
}

bool CheckTaskExistEx(const std::wstring& path, const std::wstring& taskName)
{
    if (taskName == L"*")
    {
        std::vector<std::wstring> tasks = EnumTasks(path);
        for (auto task : tasks) {
            if (GetTaskStateKnown(path, task) != 0)
                return true;
        }
        return false;
    }

    return GetTaskStateKnown(path, taskName) != 0;
}

bool CTaskTweak::IsAvailable() const
{
    if(!CTweak::IsAvailable())
        return false;

    std::unique_lock Lock(m_Mutex);

    return CheckTaskExistEx(m_Folder.c_str(), m_Entry.c_str());
}

ETweakStatus CTaskTweak::GetStatus() const
{
    std::unique_lock Lock(m_Mutex);

    if(IsTaskEnabledEx(m_Folder.c_str(), m_Entry.c_str()))
        return m_Set ? ETweakStatus::eSet : ETweakStatus::eApplied;
    return m_Set ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CTaskTweak::Apply(ETweakMode Mode)
{
    CTweak::Apply(Mode);

    if(!SetTaskEnabledEx(m_Folder.c_str(), m_Entry.c_str(), false))
        return ERR(STATUS_UNSUCCESSFUL);
    return OK;
}

STATUS CTaskTweak::Undo(ETweakMode Mode)
{
    CTweak::Undo(Mode);

    if(!SetTaskEnabledEx(m_Folder.c_str(), m_Entry.c_str(), true))
        return ERR(STATUS_UNSUCCESSFUL);
    return OK;
}

void CTaskTweak::WriteVariant(CVariant& Data) const
{
	CTweak::WriteVariant(Data);
    
    Data.Write(SVC_API_TWEAK_TYPE, SVC_API_TWEAK_TYPE_TASK);

	Data.Write(SVC_API_TWEAK_FOLDER, m_Folder);
	Data.Write(SVC_API_TWEAK_ENTRY, m_Entry);
}

////////////////////////////////////////////
// CFSTweak

ETweakStatus CFSTweak::GetStatus() const
{
    // todo
    return ETweakStatus::eNotSet;
}

STATUS CFSTweak::Apply(ETweakMode Mode)
{
    CTweak::Apply(Mode);
    // todo
    return ERR(-1);
}

STATUS CFSTweak::Undo(ETweakMode Mode)
{
    CTweak::Undo(Mode);
    // todo
    return ERR(-1);
}

void CFSTweak::WriteVariant(CVariant& Data) const
{
	CTweak::WriteVariant(Data);
    
    Data.Write(SVC_API_TWEAK_TYPE, SVC_API_TWEAK_TYPE_FS);

	Data.Write(SVC_API_TWEAK_PATH, m_PathPattern);
}

////////////////////////////////////////////
// CExecTweak

ETweakStatus CExecTweak::GetStatus() const
{
    // todo
    return ETweakStatus::eNotSet;
}

STATUS CExecTweak::Apply(ETweakMode Mode)
{
    CTweak::Apply(Mode);
    // todo
    return ERR(-1);
}

STATUS CExecTweak::Undo(ETweakMode Mode)
{
    CTweak::Undo(Mode);
    // todo
    return ERR(-1);
}

void CExecTweak::WriteVariant(CVariant& Data) const
{
	CTweak::WriteVariant(Data);
    
    Data.Write(SVC_API_TWEAK_TYPE, SVC_API_TWEAK_TYPE_EXEC);

	Data.Write(SVC_API_TWEAK_PATH, m_PathPattern);
}

////////////////////////////////////////////
// CFwTweak

ETweakStatus CFwTweak::GetStatus() const
{
    // todo
    return ETweakStatus::eNotSet;
}

STATUS CFwTweak::Apply(ETweakMode Mode)
{
    CTweak::Apply(Mode);
    // todo
    return ERR(-1);
}

STATUS CFwTweak::Undo(ETweakMode Mode)
{
    CTweak::Undo(Mode);
    // todo
    return ERR(-1);
}

void CFwTweak::WriteVariant(CVariant& Data) const
{
	CTweak::WriteVariant(Data);
    
    Data.Write(SVC_API_TWEAK_TYPE, SVC_API_TWEAK_TYPE_FW);

	Data.WriteVariant(SVC_API_TWEAK_PROG_ID, m_ProgID.ToVariant());
}
