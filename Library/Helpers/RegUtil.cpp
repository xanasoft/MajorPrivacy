#include "pch.h"
#include <ph.h>
#include "RegUtil.h"


bool RegEnumKeys(HANDLE keyHandle, ULONG Index, std::wstring& OutName)
{
    ULONG resultLength = 512;
    for (;;) {
        std::vector<BYTE> buffer(resultLength);
        NTSTATUS status = NtEnumerateKey(keyHandle, Index, KeyBasicInformation, buffer.data(), (ULONG)buffer.size(), &resultLength);
        if (NT_SUCCESS(status)) 
        {
            KEY_BASIC_INFORMATION* keyInfo = (KEY_BASIC_INFORMATION*)buffer.data();
            OutName = std::wstring(keyInfo->Name, keyInfo->NameLength / sizeof(WCHAR));
            return true;
        }
        if (!(status == STATUS_BUFFER_TOO_SMALL || status == STATUS_BUFFER_OVERFLOW) || resultLength <= buffer.size())
            break;
    }
    return false;
}

std::wstring RegQueryWString(HANDLE keyHandle, const std::wstring& valueName) 
{
    UNICODE_STRING uniValueName;
    RtlInitUnicodeString(&uniValueName, valueName.c_str());

    ULONG resultLength = 0x1000;
    for (;;) {
        std::vector<BYTE> buffer(resultLength);
        NTSTATUS status = NtQueryValueKey(keyHandle, &uniValueName, KeyValuePartialInformation, buffer.data(), (ULONG)buffer.size(), &resultLength);
        if (NT_SUCCESS(status)) 
        {
            KEY_VALUE_PARTIAL_INFORMATION* keyValueInfo = reinterpret_cast<KEY_VALUE_PARTIAL_INFORMATION*>(buffer.data());
            if (keyValueInfo->Type != REG_SZ && keyValueInfo->Type != REG_EXPAND_SZ) 
                return L"";
            return std::wstring((wchar_t*)keyValueInfo->Data, wcsnlen_s((wchar_t*)keyValueInfo->Data, keyValueInfo->DataLength / sizeof(WCHAR)));
        }
        if (!(status == STATUS_BUFFER_TOO_SMALL || status == STATUS_BUFFER_OVERFLOW) || resultLength <= buffer.size())
            break;
    }
    return L"";
}

DWORD RegQueryDWord(HKEY hKey, const WCHAR* subKey, const WCHAR* valueName, DWORD defaultValue)
{
    DWORD res = defaultValue;
    HKEY key;
    if (RegOpenKeyEx(hKey, subKey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD dataSize = sizeof(DWORD);
        RegQueryValueEx(key, valueName, NULL, NULL, (LPBYTE)&res, &dataSize);
        RegCloseKey(key);
    }
    return res;
}

DWORD RegSetDWord(HKEY hKey, const WCHAR* subKey, const WCHAR* valueName, DWORD value)
{
	HKEY key;
	if (RegCreateKeyEx(hKey, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
		DWORD res = RegSetValueEx(key, valueName, 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
		RegCloseKey(key);
		return res;
	}
	return ERROR_OPEN_FAILED;
}

StVariant RegQuery(HKEY hKey, const WCHAR* subKey, const WCHAR* valueName, const StVariant& defaultValue)
{
    StVariant res = defaultValue;
    HKEY key;
    if (RegOpenKeyEx(hKey, subKey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        res = RegQuery(key, valueName, defaultValue);
        RegCloseKey(key);
    }
    return res;
}

StVariant RegQuery(HKEY hKey, const WCHAR* valueName, const StVariant& defaultValue)
{
    StVariant res = defaultValue;
	DWORD dataSize = 0;
	DWORD dataType = 0;
	RegQueryValueEx(hKey, valueName, NULL, &dataType, NULL, &dataSize);
	if (dataSize > 0) {
		std::vector<BYTE> buffer(dataSize);
		RegQueryValueEx(hKey, valueName, NULL, &dataType, buffer.data(), &dataSize);
		switch (dataType) {
		case REG_DWORD:
			res = *(uint32*)buffer.data();
			break;
		case REG_QWORD:
			res = *(uint64*)buffer.data();
			break;
		case REG_SZ:
		case REG_EXPAND_SZ:
			res = std::wstring((wchar_t*)buffer.data(), wcsnlen_s((wchar_t*)buffer.data(), dataSize / sizeof(WCHAR)));
			break;
		//case REG_MULTI_SZ:
		//	res = std::wstring((wchar_t*)buffer.data(), dataSize / sizeof(WCHAR));
		//	break;
		case REG_BINARY:
			res = buffer;
			break;
		}
	}
	return res;
}

DWORD RegSet(HKEY hKey, const WCHAR* subKey, const WCHAR* valueName, const StVariant& value)
{
	HKEY key;
    DWORD disposition;
	if (RegCreateKeyExW(hKey, subKey, 0, NULL, 0, KEY_WRITE, NULL, &key, &disposition) == ERROR_SUCCESS) {
        DWORD res = RegSet(key, valueName, value);
		RegCloseKey(key);
		return res;
	}
	return ERROR_OPEN_FAILED;
}

DWORD RegSet(HKEY hKey, const WCHAR* valueName, const StVariant& value)
{
    switch (value.GetType()) {
    case VAR_TYPE_UINT:
    case VAR_TYPE_SINT:
        if (value.GetSize() <= sizeof(uint32)) {
            uint32 data = value;
            return RegSetValueEx(hKey, valueName, 0, REG_DWORD, (const BYTE*)&data, sizeof(uint32));
        }
        else {
            uint64 data = value;
            return RegSetValueEx(hKey, valueName, 0, REG_QWORD, (const BYTE*)&data, sizeof(uint64));
        }
    case VAR_TYPE_ASCII:
    case VAR_TYPE_UTF8:
    case VAR_TYPE_UNICODE: {
        std::wstring str = value.AsStr();
        return RegSetValueEx(hKey, valueName, 0, REG_SZ, (const BYTE*)str.c_str(), (DWORD)(str.size() + 1) * sizeof(WCHAR));
    }
    case VAR_TYPE_BYTES:
        return RegSetValueEx(hKey, valueName, 0, REG_BINARY, (const BYTE*)value.GetData(), (DWORD)value.GetSize());
    }
    return ERROR_OPEN_FAILED;
}

DWORD RegDelValue(HKEY hKey, const WCHAR* subKey, const WCHAR* valueName)
{
	HKEY key;
	if (RegOpenKeyEx(hKey, subKey, 0, KEY_WRITE, &key) == ERROR_SUCCESS) {
		DWORD res = RegDeleteValue(key, valueName);
		RegCloseKey(key);
		return res;
	}
	return ERROR_OPEN_FAILED;
}

DWORD RegDelKey(HKEY hKey, const WCHAR* subKey)
{
	return RegDeleteKey(hKey, subKey);
}

std::pair<HKEY, const wchar_t*> SplitRegKeyPath(const std::wstring& Key)
{
    size_t pos = Key.find(L'\\');
    if(pos == -1)
        return std::make_pair<HKEY, const wchar_t*>(NULL, NULL);

    HKEY hRoot = NULL;
    if (_wcsnicmp(Key.c_str(), L"HKLM", pos) == 0       || (pos > 5 && _wcsnicmp(Key.c_str(), L"HKEY_LOCAL_MACHINE", pos) == 0))
        hRoot = HKEY_LOCAL_MACHINE;
    else if (_wcsnicmp(Key.c_str(), L"HKCU", pos) == 0  || (pos > 5 && _wcsnicmp(Key.c_str(), L"HKEY_CURRENT_USER", pos) == 0))
        hRoot = HKEY_CURRENT_USER;
    else if (_wcsnicmp(Key.c_str(), L"HKCR", pos) == 0  || (pos > 5 && _wcsnicmp(Key.c_str(), L"HKEY_CLASSES_ROOT", pos) == 0))
        hRoot = HKEY_CLASSES_ROOT;
    else if (_wcsnicmp(Key.c_str(), L"HKU", pos) == 0   || (pos > 5 && _wcsnicmp(Key.c_str(), L"HKEY_USERS", pos) == 0))
        hRoot = HKEY_USERS;
        
    return std::make_pair(hRoot, Key.c_str() + pos + 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CRegWatcher

CRegWatcher::CRegWatcher()
{
    m_StopEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
}

CRegWatcher::~CRegWatcher() 
{
    Stop();
    if (m_StopEvent) 
        CloseHandle(m_StopEvent);
}

bool CRegWatcher::AddKey(const std::wstring& Key) 
{ 
    if (m_IsWatching) 
        return false;  
    m_KeysToWatch.push_back(Key); 
    return true; 
}

bool CRegWatcher::Start() 
{
    if (m_IsWatching) 
        return false;
    if (m_KeysToWatch.empty())
        return false;

    m_EventHandles.reserve(m_KeysToWatch.size() + 1);
    m_KeyHandles.reserve(m_KeysToWatch.size());

    for (const auto& Key : m_KeysToWatch)
    {
        HKEY hKey;
        auto HiveKey = SplitRegKeyPath(Key);
        if (HiveKey.first && RegOpenKeyEx(HiveKey.first, HiveKey.second, 0, KEY_NOTIFY, &hKey) == ERROR_SUCCESS) 
        {
            HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (hEvent)
            {
                m_EventHandles.push_back(hEvent);
                m_KeyHandles.push_back(hKey);
                m_WatchedKeys.push_back(Key);
                RegNotifyChangeKeyValue(hKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET /*| REG_NOTIFY_CHANGE_ATTRIBUTES | REG_NOTIFY_CHANGE_SECURITY*/, hEvent, TRUE);
            }
            else
                RegCloseKey(hKey);
        }
    }

    if (m_EventHandles.empty())
        return false;

    m_EventHandles.push_back(m_StopEvent);

    ResetEvent(m_StopEvent);
    m_IsWatching = true;
    m_WatchThread = std::thread(&CRegWatcher::WatchKeys, this);

    return true;
}

void CRegWatcher::Stop()
{
    if (!m_IsWatching)
        return;
    
    SetEvent(m_StopEvent);
    m_WatchThread.join();
    m_IsWatching = false;

    for (size_t i = 0; i < m_KeyHandles.size(); i++)
        RegCloseKey(m_KeyHandles[i]);
    for (size_t i = 0; i < m_EventHandles.size()-1; i++)
        CloseHandle(m_EventHandles[i]);
}

void CRegWatcher::WatchKeys()
{
    while (m_IsWatching) 
    {
        DWORD waitStatus = WaitForMultipleObjects((DWORD)m_EventHandles.size(), m_EventHandles.data(), FALSE, INFINITE);
        
        if (waitStatus == WAIT_OBJECT_0 + m_EventHandles.size() - 1)
            break; // stop event

        if (waitStatus >= WAIT_OBJECT_0 && waitStatus < WAIT_OBJECT_0 + m_EventHandles.size()) 
        {    
            int index = waitStatus - WAIT_OBJECT_0;
            RegNotifyChangeKeyValue(m_KeyHandles[index], TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, m_EventHandles[index], TRUE);

            std::unique_lock<std::mutex> Lock(m_HandlersMutex);

	        for (auto Handler : m_Handlers)
		        Handler(m_WatchedKeys[index]);
        }
    }
}