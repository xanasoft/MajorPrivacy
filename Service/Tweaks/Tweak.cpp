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
#include "..\Library\Common\Strings.h"
#include "..\Access\AccessManager.h"
#include "..\Programs\ProgramManager.h"
#include "..\Network\NetworkManager.h"
#include "..\Network\Firewall\Firewall.h"
#include "..\Network\Firewall\WindowsFirewall.h"


std::wstring snakeToCamel(const std::wstring& snake) {
    std::wstring camel;
    bool capitalize = true;

    for (char c : snake) {
        if (c == '_') {
            capitalize = true;
        } else if (capitalize) {
            camel += std::toupper(c);
            capitalize = false;
        } else {
            camel += c;
        }
    }

    return camel;
}

std::wstring camelToSnake(const std::wstring& camel) {
    std::wstring snake;

    for (char c : camel) {
        if (std::isupper(c)) {
            snake += '_';
            snake += std::tolower(c);
        } else {
            snake += c;
        }
    }

    return snake;
}

std::wstring spaceToCamel(const std::wstring& text) {
    std::wstring camel;
    bool capitalize = false;

    for (char c : text) {
        if (std::isspace(c)) {
            capitalize = true;
        } else if (capitalize) {
            camel += std::toupper(c);
            capitalize = false;
        } else {
            camel += std::tolower(c);
        }
    }

    return camel;
}

std::wstring spaceToSnake(const std::wstring& text) {
    std::wstring snake;

    for (char c : text) {
        if (std::isspace(c)) {
            snake += '_';
        } else {
            snake += std::tolower(c);
        }
    }

    return snake;
}



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

CAbstractTweak::CAbstractTweak(const std::wstring& Id) 
{ 
    m_Id = snakeToCamel(Id); 
}

StVariant CAbstractTweak::ToVariant(const SVarWriteOpt& Opts) const
{
    std::unique_lock Lock(m_Mutex);

    StVariantWriter Tweak;
    if (Opts.Format == SVarWriteOpt::eIndex) {
        Tweak.BeginIndex();
        WriteIVariant(Tweak, Opts);
    } else {  
        Tweak.BeginMap();
        WriteMVariant(Tweak, Opts);
    }
    return Tweak.Finish();
}

NTSTATUS CAbstractTweak::FromVariant(const class StVariant& Data)
{
    if (Data.GetType() == VAR_TYPE_MAP)         StVariantReader(Data).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
    else if (Data.GetType() == VAR_TYPE_INDEX)  StVariantReader(Data).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
    else
        return STATUS_UNKNOWN_REVISION;
    return STATUS_SUCCESS;
}

ETweakType CAbstractTweak::ReadType(const StVariant& Data, SVarWriteOpt::EFormat& Format)
{
    ETweakType Type = ETweakType::eUnknown;
    if (Data.GetType() == VAR_TYPE_MAP)
    {
        Format = SVarWriteOpt::eMap;
        std::string TypeStr = StVariantReader(Data).Find(API_S_TWEAK_TYPE);
		if (TypeStr == API_S_TWEAK_TYPE_GROUP) Type = ETweakType::eGroup;
		else if (TypeStr == API_S_TWEAK_TYPE_SET) Type = ETweakType::eSet;
		else if (TypeStr == API_S_TWEAK_TYPE_REG) Type = ETweakType::eReg;
		else if (TypeStr == API_S_TWEAK_TYPE_GPO) Type = ETweakType::eGpo;
		else if (TypeStr == API_S_TWEAK_TYPE_SVC) Type = ETweakType::eSvc;
		else if (TypeStr == API_S_TWEAK_TYPE_TASK) Type = ETweakType::eTask;
		else if (TypeStr == API_S_TWEAK_TYPE_RES) Type = ETweakType::eRes;
		else if (TypeStr == API_S_TWEAK_TYPE_EXEC) Type = ETweakType::eExec;
		else if (TypeStr == API_S_TWEAK_TYPE_FW) Type = ETweakType::eFw;
    }
    else if (Data.GetType() == VAR_TYPE_INDEX)
    {
        Format = SVarWriteOpt::eIndex;
        Type = (ETweakType)StVariantReader(Data).Find(API_V_TWEAK_TYPE).To<uint32>();
    }
    return Type;
}

void CAbstractTweak::Dump(class CConfigIni* pIni)
{
    //auto pParent = m_Parent.lock();

    pIni->SetValue(w2s(GetId()), "Name", m_Name);

    //std::wstring Parent = pParent->GetId();
    //if (!Parent.empty()) pIni->SetValue(w2s(GetId()), "Parent", Parent);
    if (!m_ParentId.empty()) pIni->SetValue(w2s(GetId()), "Parent", m_ParentId);

    std::wstring Hint;
	switch (m_Hint) {
	case ETweakHint::eNone:             Hint = L"None"; break;
	case ETweakHint::eRecommended:      Hint = L"Recommended"; break;
	case ETweakHint::eNotRecommended:   Hint = L"NotRecommended"; break;
	case ETweakHint::eBreaking:         Hint = L"Breaking"; break;
	}
	pIni->SetValue(w2s(GetId()), "Hint", Hint);
}

std::shared_ptr<CAbstractTweak> CAbstractTweak::CreateFromIni(const std::string& Id, class CConfigIni* pIni)
{
    std::string Type = w2s(pIni->GetValue(Id, "Type", L""));
    if (Type.empty())
		return nullptr;

    std::wstring Name = pIni->GetValue(Id, "Name", L"");

    ETweakHint eHint = ETweakHint::eNone;
    std::wstring Hint = pIni->GetValue(Id, "Hint", L"");
    if (Hint == L"None")                eHint = ETweakHint::eNone;
    else if (Hint == L"Recommended")    eHint = ETweakHint::eRecommended;
	else if (Hint == L"NotRecommended") eHint = ETweakHint::eNotRecommended;
	else if (Hint == L"Breaking")       eHint = ETweakHint::eBreaking;

	std::shared_ptr<CAbstractTweak> pTweak;
	if (Type == API_S_TWEAK_TYPE_GROUP) {
		pTweak = std::make_shared<CTweakGroup>(Name, s2w(Id));
	}
	else if (Type == API_S_TWEAK_TYPE_SET) {
		pTweak = std::make_shared<CTweakSet>(Name, s2w(Id), eHint);
	}
	else if (Type == API_S_TWEAK_TYPE_REG) {
		pTweak = std::make_shared<CRegTweak>(Name, s2w(Id), CTweak::Str2Ver(pIni->GetValue(Id, "WinVer", L"")),
			pIni->GetValue(Id, "Key", L""), pIni->GetValue(Id, "Value", L""), CTweak::Str2Var(pIni->GetValue(Id, "Data", L"")), eHint);
	}
	else if (Type == API_S_TWEAK_TYPE_GPO) {
		pTweak = std::make_shared<CGpoTweak>(Name, s2w(Id), CTweak::Str2Ver(pIni->GetValue(Id, "WinVer", L"")),
            pIni->GetValue(Id, "Key", L""), pIni->GetValue(Id, "Value", L""), CTweak::Str2Var(pIni->GetValue(Id, "Data", L"")), eHint);
	}
	else if (Type == API_S_TWEAK_TYPE_SVC) {
		pTweak = std::make_shared<CSvcTweak>(Name, s2w(Id), CTweak::Str2Ver(pIni->GetValue(Id, "WinVer", L"")),
			pIni->GetValue(Id, "Service", L""), eHint);
	}
	else if (Type == API_S_TWEAK_TYPE_TASK) {
		pTweak = std::make_shared<CTaskTweak>(Name, s2w(Id), CTweak::Str2Ver(pIni->GetValue(Id, "WinVer", L"")),
			pIni->GetValue(Id, "Folder", L""), pIni->GetValue(Id, "Entry", L""), eHint);
	}
	else if (Type == API_S_TWEAK_TYPE_RES) {
		pTweak = std::make_shared<CResTweak>(Name, s2w(Id), CTweak::Str2Ver(pIni->GetValue(Id, "WinVer", L"")),
			pIni->GetValue(Id, "Path", L""), eHint);
	}
	else if (Type == API_S_TWEAK_TYPE_EXEC) {
		pTweak = std::make_shared<CExecTweak>(Name, s2w(Id), CTweak::Str2Ver(pIni->GetValue(Id, "WinVer", L"")),
			pIni->GetValue(Id, "Path", L""), eHint);
	}
	else if (Type == API_S_TWEAK_TYPE_FW) {
		pTweak = std::make_shared<CFwTweak>(Name, s2w(Id), CTweak::Str2Ver(pIni->GetValue(Id, "WinVer", L"")),
			pIni->GetValue(Id, "Path", L""), pIni->GetValue(Id, "Service", L""), eHint);
	}
	return pTweak;
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

STATUS CTweakList::Apply(uint32 CallerPID, ETweakMode Mode)
{
    int FailCount = 0;
    for (auto pTweak : m_List) {
        if (!pTweak->IsAvailable())
            continue;
        auto Hint = pTweak->GetHint();
        if (Hint == ETweakHint::eBreaking || (Hint == ETweakHint::eNotRecommended && !((int)Mode & (int)ETweakMode::eAll)))
            continue;
        STATUS Status = pTweak->Apply(CallerPID, Mode);
        if (!Status) FailCount++;
    }
    if(FailCount)
        return ERR(STATUS_UNSUCCESSFUL);
    return OK;
}

STATUS CTweakList::Undo(uint32 CallerPID, ETweakMode Mode)
{
    for (auto pTweak : m_List) {
        if (!pTweak->IsAvailable())
            continue;
        pTweak->Undo(CallerPID, Mode);
    }
    return OK;
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweakGroup

void CTweakGroup::Dump(class CConfigIni* pIni)
{
    pIni->SetValue(w2s(GetId()), "Type",  WIDEN(API_S_TWEAK_TYPE_GROUP));

    CTweakList::Dump(pIni);
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweakSet

ETweakStatus CTweakSet::GetStatus(uint32 CallerPID) const
{
    int State = (int)ETweakStatus::eNotAvailable;
    for (auto pTweak : m_List) {
        ETweakStatus eState = pTweak->GetStatus(CallerPID);
		if (eState == ETweakStatus::eNotAvailable)
			continue;
        if ((int)eState > State)
            State = (int)eState;
    }
    return (ETweakStatus)State;
}

void CTweakSet::Dump(class CConfigIni* pIni)
{
	pIni->SetValue(w2s(GetId()), "Type", WIDEN(API_S_TWEAK_TYPE_SET));

	CTweakList::Dump(pIni);
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweak

STATUS CTweak::Apply(uint32 CallerPID, ETweakMode Mode)
{
    if (!((int)Mode & (int)ETweakMode::eDontSet))
        m_Set = true;
    return OK;
}

STATUS CTweak::Undo(uint32 CallerPID, ETweakMode Mode)
{
    if (!((int)Mode & (int)ETweakMode::eDontSet))
        m_Set = false;
    return OK;
}


void CTweak::Dump(class CConfigIni* pIni)
{
    CAbstractTweak::Dump(pIni);

    std::wstring WinVer = Ver2Str(m_Ver);
    if (!WinVer.empty()) pIni->SetValue(w2s(GetId()), "WinVer", WinVer);
}

std::wstring CTweak::Ver2Str(const SWinVer& Ver)
{
	std::vector<std::wstring> Str;
	if (Ver.MinBuild) Str.push_back(L"MinBuild:" + std::to_wstring(Ver.MinBuild));
	if (Ver.MaxBuild) Str.push_back(L"MaxBuild:" + std::to_wstring(Ver.MaxBuild));
    std::wstring WinEd;
    if (Ver.Win10Ed == EWinEd::Home) WinEd = L"Home"; 
    else if (Ver.Win10Ed == EWinEd::Pro) WinEd = L"Pro"; 
    else if (Ver.Win10Ed == EWinEd::Enterprise) WinEd = L"Ent";
	if (!WinEd.empty()) Str.push_back(L"WinEd:" + WinEd);
	return JoinStr(Str, L";");
}

SWinVer CTweak::Str2Ver(const std::wstring& Str)
{
	SWinVer Ver;
	std::vector<std::wstring> Tokens = SplitStr(Str, L";");
	for (const auto& Token : Tokens) {
		if (Token.find(L"MinBuild:") == 0)
			Ver.MinBuild = std::stoul(Token.substr(9));
		else if (Token.find(L"MaxBuild:") == 0)
			Ver.MaxBuild = std::stoul(Token.substr(9));
        else if (Token.find(L"WinEd:") == 0)
        {
			std::wstring WinEd = Token.substr(8);
			if (WinEd == L"Home") Ver.Win10Ed = EWinEd::Home;
			else if (WinEd == L"Pro") Ver.Win10Ed = EWinEd::Pro;
			else if (WinEd == L"Ent") Ver.Win10Ed = EWinEd::Enterprise;
        }
	}
	return Ver;
}

std::wstring CTweak::Var2Str(const StVariant& Var)
{
    switch (Var.GetType()) {
    case VAR_TYPE_ASCII:
    case VAR_TYPE_UTF8:
    case VAR_TYPE_UNICODE:
        return L"\"" + StrReplaceAll(Var.AsStr(), L"\"", L"\\\"") + L"\"";
    case VAR_TYPE_UINT:
    case VAR_TYPE_SINT:
        return Var.AsStr();
    default: return L"";
    }
}

StVariant CTweak::Str2Var(const std::wstring& Str)
{
    StVariant Var;
	if (Str.empty())
		return Var;
	if (Str[0] == L'"' && Str[Str.length() - 1] == L'"') {
		std::wstring Value = Str.substr(1, Str.length() - 2);
		Value = StrReplaceAll(Value, L"\\\"", L"\"");
		Var = Value;
	}
	else 
		Var = _wtoi(Str.c_str());
	return Var;
}

//
// ************************************************************************************
// Tweaks
//

////////////////////////////////////////////
// CRegTweak

std::pair<HKEY, std::wstring> SplitRegKeyPathEx(const std::wstring& Key, uint32 CallerPID)
{
    size_t pos = Key.find(L'\\');
    if(pos == -1)
        return std::make_pair<HKEY, const wchar_t*>(NULL, NULL);

    HKEY hRoot = NULL;
    if (_wcsnicmp(Key.c_str(), L"HKLM", pos) == 0       || (pos > 5 && _wcsnicmp(Key.c_str(), L"HKEY_LOCAL_MACHINE", pos) == 0))
        hRoot = HKEY_LOCAL_MACHINE;
    else if (_wcsnicmp(Key.c_str(), L"HKCU", pos) == 0  || (pos > 5 && _wcsnicmp(Key.c_str(), L"HKEY_CURRENT_USER", pos) == 0))
    {
        hRoot = HKEY_CURRENT_USER;
		if (CallerPID != -1) {
			std::wstring SID = CServiceCore::GetCallerSID(CallerPID);
            if (!SID.empty())
                return std::make_pair(HKEY_USERS, SID + (Key.c_str() + pos));
        }
    }
    //else if (_wcsnicmp(Key.c_str(), L"HKCR", pos) == 0  || (pos > 5 && _wcsnicmp(Key.c_str(), L"HKEY_CLASSES_ROOT", pos) == 0))
    //    hRoot = HKEY_CLASSES_ROOT;
    //else if (_wcsnicmp(Key.c_str(), L"HKU", pos) == 0   || (pos > 5 && _wcsnicmp(Key.c_str(), L"HKEY_USERS", pos) == 0))
    //    hRoot = HKEY_USERS;
    else
        ASSERT(0);

    return std::make_pair(hRoot, Key.c_str() + pos + 1);
}

ETweakStatus CRegTweak::GetStatus(uint32 CallerPID) const
{
	if (!IsAvailable())
		return ETweakStatus::eNotAvailable;

    std::unique_lock Lock(m_Mutex);

    std::pair<HKEY, std::wstring> RegPath = SplitRegKeyPathEx(m_Key, CallerPID);

	// we usually run as service current user is SYSTEM, we need the CallerPID to access the right user by SID
    if (RegPath.first == HKEY_CURRENT_USER)
        return ETweakStatus::eNotSet;

    StVariant Value = RegQuery(RegPath.first, RegPath.second.c_str(), m_Value.c_str());
    bool bApplied = Value == m_Data;

    if (bApplied)
        return IsSet() ? ETweakStatus::eSet : ETweakStatus::eApplied;
    return IsSet() ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CRegTweak::Apply(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Apply(CallerPID, Mode);

    std::pair<HKEY, std::wstring> RegPath = SplitRegKeyPathEx(m_Key, CallerPID);

    if (RegSet(RegPath.first, RegPath.second.c_str(), m_Value.c_str(), m_Data) != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);

    return OK;
}

STATUS CRegTweak::Undo(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Undo(CallerPID, Mode);

    std::pair<HKEY, std::wstring> RegPath = SplitRegKeyPathEx(m_Key, CallerPID);

    CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
    if(RegOpenKeyExW(RegPath.first, RegPath.second.c_str(), 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return OK;
    
    LSTATUS ret = RegDeleteValueW(hKey, m_Value.c_str());
    if (ret == ERROR_FILE_NOT_FOUND)
        return OK; // not set all ok
    if (ret != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);

	return OK;    
}

void CRegTweak::Dump(class CConfigIni* pIni)
{
	pIni->SetValue(w2s(GetId()), "Type", WIDEN(API_S_TWEAK_TYPE_REG));

	CTweak::Dump(pIni);

	pIni->SetValue(w2s(GetId()), "Key", m_Key);
	pIni->SetValue(w2s(GetId()), "Value", m_Value);
    pIni->SetValue(w2s(GetId()), "Data", Var2Str(m_Data));
}

////////////////////////////////////////////
// CGpoTweak

typedef void(*ReleaseGPO)(CGPO*);

ETweakStatus CGpoTweak::GetStatus(uint32 CallerPID) const
{
    if (!IsAvailable())
        return ETweakStatus::eNotAvailable;

    CScopedHandle<CGPO*, void(*)(CGPO*)> pGPO = CScopedHandle(theCore->TweakManager()->GetLockedGPO(), (ReleaseGPO)[](CGPO* pGPO) {theCore->TweakManager()->ReleaseGPO(pGPO);});
    if(!pGPO) return ETweakStatus::eIneffective;

    std::unique_lock Lock(m_Mutex);

    std::pair<HKEY, const wchar_t*> RegPath = SplitRegKeyPath(m_Key);

    CScopedHandle hRoot = CScopedHandle((HKEY)0, RegCloseKey);
    if (RegPath.first == HKEY_LOCAL_MACHINE)
        *&hRoot = pGPO->GetMachineRoot();
    else if (RegPath.first == HKEY_CURRENT_USER)
        *&hRoot = pGPO->GetUserRoot();
    bool bApplied = false;
    if (hRoot)
    {
        CScopedHandle hKey = CScopedHandle((HKEY)0, RegCloseKey);
        if (RegOpenKeyExW(hRoot, RegPath.second, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            StVariant Value = RegQuery(hKey, m_Value.c_str());
            bApplied = Value == m_Data;
        }
    }

    std::pair<HKEY, std::wstring> RegPath2 = SplitRegKeyPathEx(m_Key, CallerPID);

    StVariant Value = RegQuery(RegPath2.first, RegPath2.second.c_str(), m_Value.c_str());
    bool bEffective = Value == m_Data;

    if (bApplied) {
        if (!bEffective)
            return ETweakStatus::eIneffective;
        return IsSet() ? ETweakStatus::eSet : ETweakStatus::eApplied;
    }
    return IsSet() ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CGpoTweak::Apply(uint32 CallerPID, ETweakMode Mode)
{
    CScopedHandle<CGPO*, void(*)(CGPO*)> pGPO = CScopedHandle(theCore->TweakManager()->GetLockedGPO(), (ReleaseGPO)[](CGPO* pGPO) {theCore->TweakManager()->ReleaseGPO(pGPO);});
    if(!pGPO) return ERR(STATUS_UNSUCCESSFUL);

    std::unique_lock Lock(m_Mutex);

    CTweak::Apply(CallerPID, Mode);

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

	HRESULT hr = pGPO->SaveGPO();
	if (FAILED(hr)) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_GPO_FAILED, L"Failed to save GPO: 0x%08X", hr);
		return ERR(STATUS_UNSUCCESSFUL);
	}

    return OK;
}

STATUS CGpoTweak::Undo(uint32 CallerPID, ETweakMode Mode)
{
    CScopedHandle<CGPO*, void(*)(CGPO*)> pGPO = CScopedHandle(theCore->TweakManager()->GetLockedGPO(), (ReleaseGPO)[](CGPO* pGPO) {theCore->TweakManager()->ReleaseGPO(pGPO);});
    if(!pGPO) return ERR(STATUS_UNSUCCESSFUL);

    std::unique_lock Lock(m_Mutex);

    CTweak::Undo(CallerPID, Mode);

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

    LSTATUS ret = RegDeleteValueW(hKey, m_Value.c_str());
    if (ret == ERROR_FILE_NOT_FOUND)
        return OK; // not set all ok
    if (ret != ERROR_SUCCESS)
        return ERR(STATUS_UNSUCCESSFUL);

    HRESULT hr = pGPO->SaveGPO();
    if (FAILED(hr)) {
        theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_GPO_FAILED, L"Failed to save GPO: 0x%08X", hr);
        return ERR(STATUS_UNSUCCESSFUL);
    }
    return OK;    
}

void CGpoTweak::Dump(class CConfigIni* pIni)
{
	pIni->SetValue(w2s(GetId()), "Type", WIDEN(API_S_TWEAK_TYPE_GPO));

    CTweak::Dump(pIni);

    pIni->SetValue(w2s(GetId()), "Key", m_Key);
    pIni->SetValue(w2s(GetId()), "Value", m_Value);
    pIni->SetValue(w2s(GetId()), "Data", Var2Str(m_Data));
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

ETweakStatus CSvcTweak::GetStatus(uint32 CallerPID) const
{
    if (!IsAvailable())
        return ETweakStatus::eNotAvailable;

    std::unique_lock Lock(m_Mutex);

    // faster then querygin the SCM
    DWORD SvcStart = RegQueryDWord(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"Start", -1);

    bool bApplied = SvcStart == SERVICE_DISABLED;

    if (bApplied)
        return IsSet() ? ETweakStatus::eSet : ETweakStatus::eApplied;
    return IsSet() ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CSvcTweak::Apply(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Apply(CallerPID, Mode);

    DWORD SvcStart = RegQueryDWord(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"Start", -1);
    RegSetDWord(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"OldStart", SvcStart);

    return SetServiceStart(m_SvcTag.c_str(), SERVICE_DISABLED);
}

STATUS CSvcTweak::Undo(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Undo(CallerPID, Mode);

    DWORD SvcStart = RegQueryDWord(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"OldStart", -1);
    if (SvcStart == -1) {
        SvcStart = SERVICE_DEMAND_START;
        //return ERR(STATUS_NOT_FOUND);
    }
    
    STATUS Status = SetServiceStart(m_SvcTag.c_str(), SvcStart);
    if (!Status)
		return Status;
    RegDelValue(HKEY_LOCAL_MACHINE, (g_RegKeySCM + m_SvcTag).c_str(), L"OldStart");
    return OK;
}

void CSvcTweak::Dump(class CConfigIni* pIni)
{
	pIni->SetValue(w2s(GetId()), "Type", WIDEN(API_S_TWEAK_TYPE_SVC));

	CTweak::Dump(pIni);

	pIni->SetValue(w2s(GetId()), "Service", m_SvcTag);
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

ETweakStatus CTaskTweak::GetStatus(uint32 CallerPID) const
{
    if (!IsAvailable())
        return ETweakStatus::eNotAvailable;

    std::unique_lock Lock(m_Mutex);

    if (!IsTaskEnabledEx(m_Folder.c_str(), m_Entry.c_str()))
        return IsSet() ? ETweakStatus::eSet : ETweakStatus::eApplied;
    return IsSet() ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CTaskTweak::Apply(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Apply(CallerPID, Mode);

    if(!SetTaskEnabledEx(m_Folder.c_str(), m_Entry.c_str(), false))
        return ERR(STATUS_UNSUCCESSFUL);
    return OK;
}

STATUS CTaskTweak::Undo(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Undo(CallerPID, Mode);

    if(!SetTaskEnabledEx(m_Folder.c_str(), m_Entry.c_str(), true))
        return ERR(STATUS_UNSUCCESSFUL);
    return OK;
}

void CTaskTweak::Dump(class CConfigIni* pIni)
{
	pIni->SetValue(w2s(GetId()), "Type", WIDEN(API_S_TWEAK_TYPE_TASK));

	CTweak::Dump(pIni);

	pIni->SetValue(w2s(GetId()), "Folder", m_Folder);
	pIni->SetValue(w2s(GetId()), "Entry", m_Entry);
}

////////////////////////////////////////////
// CResTweak

ETweakStatus CResTweak::GetStatus(uint32 CallerPID) const
{
    if (!IsAvailable())
        return ETweakStatus::eNotAvailable;

    std::unique_lock Lock(m_Mutex);

    CAccessRulePtr pRule = theCore->AccessManager()->GetRule(L"ResTweak:" + m_Id);
    if (pRule && pRule->IsEnabled())
        return IsSet() ? ETweakStatus::eSet : ETweakStatus::eApplied;
    return IsSet() ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CResTweak::Apply(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    WCHAR expanded[MAX_PATH];
    DWORD len = ExpandEnvironmentStringsW(m_PathPattern.c_str(), expanded, MAX_PATH);
    if (len == 0 || len > MAX_PATH || wcschr(expanded, L'%'))
        return ERR(STATUS_OBJECT_PATH_INVALID);

    CTweak::Apply(CallerPID, Mode);

    CProgramItemPtr pAllPrograms = theCore->ProgramManager()->GetAllItem();

    CAccessRulePtr pRule = std::make_shared<CAccessRule>(pAllPrograms->GetID());
    pRule->SetStrGuid(L"ResTweak:" + m_Id);
    pRule->SetType(EAccessRuleType::eEnum);
    pRule->SetAccessPath(expanded);
    pRule->SetName(m_Name);

    return theCore->AccessManager()->AddRule(pRule);
}

STATUS CResTweak::Undo(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Undo(CallerPID, Mode);
  
    return theCore->AccessManager()->RemoveRule(L"ResTweak:" + m_Id);
}

void CResTweak::Dump(class CConfigIni* pIni)
{
	pIni->SetValue(w2s(GetId()), "Type", WIDEN(API_S_TWEAK_TYPE_RES));

	CTweak::Dump(pIni);

	pIni->SetValue(w2s(GetId()), "Path", m_PathPattern);
}

////////////////////////////////////////////
// CExecTweak

ETweakStatus CExecTweak::GetStatus(uint32 CallerPID) const
{
    if (!IsAvailable())
        return ETweakStatus::eNotAvailable;

    std::unique_lock Lock(m_Mutex);

    CProgramRulePtr pRule = theCore->ProgramManager()->GetRule(L"ExecTweak:" + m_Id);
    if (pRule && pRule->IsEnabled())
        return IsSet() ? ETweakStatus::eSet : ETweakStatus::eApplied;
    return IsSet() ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CExecTweak::Apply(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    WCHAR expanded[MAX_PATH];
    DWORD len = ExpandEnvironmentStringsW(m_PathPattern.c_str(), expanded, MAX_PATH);
    if (len == 0 || len > MAX_PATH || wcschr(expanded, L'%'))
        return ERR(STATUS_OBJECT_PATH_INVALID);

    CTweak::Apply(CallerPID, Mode);

    CProgramItemPtr pProgram = theCore->ProgramManager()->GetProgramFile(expanded);

    CProgramRulePtr pRule = std::make_shared<CProgramRule>(pProgram->GetID());
    pRule->SetStrGuid(L"ExecTweak:" + m_Id);
    pRule->SetType(EExecRuleType::eBlock);
    pRule->SetName(m_Name);

    return theCore->ProgramManager()->AddRule(pRule);
}

STATUS CExecTweak::Undo(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Undo(CallerPID, Mode);

    return theCore->ProgramManager()->RemoveRule(L"ExecTweak:" + m_Id);
}

void CExecTweak::Dump(class CConfigIni* pIni)
{
	pIni->SetValue(w2s(GetId()), "Type", WIDEN(API_S_TWEAK_TYPE_EXEC));

	CTweak::Dump(pIni);

	pIni->SetValue(w2s(GetId()), "Path", m_PathPattern);
}

////////////////////////////////////////////
// CFwTweak

ETweakStatus CFwTweak::GetStatus(uint32 CallerPID) const
{
    if (!IsAvailable())
        return ETweakStatus::eNotAvailable;

    std::unique_lock Lock(m_Mutex);

    CFirewallRulePtr pRule = theCore->NetworkManager()->Firewall()->GetRule(L"FwTweak:" + m_Id);
    if (pRule && pRule->IsEnabled())
        return IsSet() ? ETweakStatus::eSet : ETweakStatus::eApplied;
    return IsSet() ? ETweakStatus::eMissing : ETweakStatus::eNotSet;
}

STATUS CFwTweak::Apply(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    WCHAR expanded[MAX_PATH];
    DWORD len = ExpandEnvironmentStringsW(m_PathPattern.c_str(), expanded, MAX_PATH);
    if (len == 0 || len > MAX_PATH || wcschr(expanded, L'%'))
        return ERR(STATUS_OBJECT_PATH_INVALID);

    CTweak::Apply(CallerPID, Mode);

    CProgramFilePtr pProgram = theCore->ProgramManager()->GetProgramFile(expanded);
	CProgramID ID = pProgram->GetID();

	std::shared_ptr<SWindowsFwRule> Rule = std::make_shared<SWindowsFwRule>();
    Rule->BinaryPath = ID.GetFilePath();
    Rule->ServiceTag = ID.GetServiceTag();
    if (ID.GetType() == EProgramType::eAppPackage)
        Rule->AppContainerSid = ID.GetAppContainerSid();

	Rule->Guid = L"FwTweak:" + m_Id;
	Rule->Name = m_Name;
    Rule->Enabled = true;
    Rule->Profile = (int)EFwProfiles::All;
    Rule->Protocol = EFwKnownProtocols::Any;
    Rule->Interface = (int)EFwInterfaces::All;
	Rule->Action = EFwActions::Block;
	Rule->Direction = EFwDirections::Outbound;

    CFirewallRulePtr pRule = std::make_shared<CFirewallRule>(Rule);
	return theCore->NetworkManager()->Firewall()->SetRule(pRule);
}

STATUS CFwTweak::Undo(uint32 CallerPID, ETweakMode Mode)
{
    std::unique_lock Lock(m_Mutex);

    CTweak::Undo(CallerPID, Mode);
    
	return theCore->NetworkManager()->Firewall()->DelRule(L"FwTweak:" + m_Id);
}

//std::wstring CFwTweak::ToString() const 
//{ 
//    std::wstring str = L"fw:///" + m_PathPattern;
//    str += L"?action=block";
//    if(!m_SvcTag.empty())
//        str += L"&svc=" + m_SvcTag;
//    return str;
//}

void CFwTweak::Dump(class CConfigIni* pIni)
{
	pIni->SetValue(w2s(GetId()), "Type", WIDEN(API_S_TWEAK_TYPE_FW));
	
    CTweak::Dump(pIni);

	pIni->SetValue(w2s(GetId()), "Path", m_PathPattern);
	pIni->SetValue(w2s(GetId()), "Service", m_SvcTag);
}