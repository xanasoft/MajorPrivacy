#pragma once
#include "../lib_global.h"
#include "../Common/StVariant.h"

LIBRARY_EXPORT bool RegEnumKeys(HANDLE keyHandle, ULONG Index, std::wstring& OutName);

LIBRARY_EXPORT std::wstring RegQueryWString(HANDLE keyHandle, const std::wstring& valueName);
LIBRARY_EXPORT DWORD RegQueryDWord(HKEY hKey, const WCHAR* subKey, const WCHAR* valueName, DWORD defaultValue);
LIBRARY_EXPORT DWORD RegSetDWord(HKEY hKey, const WCHAR* subKey, const WCHAR* valueName, DWORD value);

LIBRARY_EXPORT StVariant RegQuery(HKEY hKey, const WCHAR* subKey, const WCHAR* valueName, const StVariant& defaultValue = StVariant());
LIBRARY_EXPORT StVariant RegQuery(HKEY hKey, const WCHAR* valueName, const StVariant& defaultValue = StVariant());

LIBRARY_EXPORT DWORD RegSet(HKEY hKey, const WCHAR* subKey, const WCHAR* valueName, const StVariant& value);
LIBRARY_EXPORT DWORD RegSet(HKEY hKey, const WCHAR* valueName, const StVariant& value);

LIBRARY_EXPORT DWORD RegDelValue(HKEY hKey, const WCHAR* subKey, const WCHAR* valueName);
LIBRARY_EXPORT DWORD RegDelKey(HKEY hKey, const WCHAR* subKey);

LIBRARY_EXPORT std::pair<HKEY, const wchar_t*> SplitRegKeyPath(const std::wstring& Key);

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CRegWatcher

class LIBRARY_EXPORT CRegWatcher
{
public:
    CRegWatcher();
    ~CRegWatcher();

    bool AddKey(const std::wstring& Key);

    void RegisterHandler(const std::function<void(const std::wstring& Key)>& Handler) { 
		std::unique_lock<std::mutex> Lock(m_HandlersMutex);
		m_Handlers.push_back(Handler); 
	}
	template<typename T, class C>
    void RegisterHandler(T Handler, C This) { RegisterHandler(std::bind(Handler, This, std::placeholders::_1)); }

    bool Start();
    void Stop();

protected:
    std::vector<std::wstring> m_KeysToWatch;
    std::vector<HANDLE> m_EventHandles;
    std::vector<HKEY> m_KeyHandles;
    std::vector<std::wstring> m_WatchedKeys;
    std::thread m_WatchThread;
    HANDLE m_StopEvent = NULL;
    bool m_IsWatching = false;
    
	std::mutex m_HandlersMutex;
	std::vector<std::function<void(const std::wstring& Key)>> m_Handlers;

    void WatchKeys();
};
