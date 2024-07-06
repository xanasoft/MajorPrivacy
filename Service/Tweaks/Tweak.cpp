#include "pch.h"
#include "Tweak.h"
#include "..\ServiceCore.h"
#include "../Library/API/PrivacyAPI.h"
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

CVariant CAbstractTweak::ToVariant(const SVarWriteOpt& Opts) const
{
    std::unique_lock Lock(m_Mutex);

    CVariant Rule;
    if (Opts.Format == SVarWriteOpt::eIndex) {
        Rule.BeginIMap();
        WriteIVariant(Rule, Opts);
    } else {  
        Rule.BeginMap();
        WriteMVariant(Rule, Opts);
    }
    Rule.Finish();
    return Rule;
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

////////////////////////////////////////////
// CGpoTweak

ETweakStatus CGpoTweak::GetStatus() const
{
    CGPO* pGPO = theCore->TweakManager()->GetGPOLocked();

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
    CGPO* pGPO = theCore->TweakManager()->GetGPOLocked();

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
    CGPO* pGPO = theCore->TweakManager()->GetGPOLocked();

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
            SetTaskEnabled(path, task, setValue);
        }
        return true;
    }

    return SetTaskEnabled(path, taskName, setValue);
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
        return m_Set ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
    return m_Set ? ETweakStatus::eSet : ETweakStatus::eApplied;
}

STATUS CTaskTweak::Apply(ETweakMode Mode)
{
    CTweak::Apply(Mode);

    if(SetTaskEnabledEx(m_Folder.c_str(), m_Entry.c_str(), false))
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