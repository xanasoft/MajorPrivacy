#include "pch.h"
#include "DnsConfigurator.h"

#include <windows.h>
#include <winreg.h>
#include <windns.h>
#include <string>
#include <vector>
#include <algorithm>

typedef DWORD (WINAPI *P_DhcpNotifyConfigChange)(
    LPCWSTR ServerName,
    LPCWSTR AdapterName,
    BOOL NewIpAddress,
    DWORD IpIndex,
    DWORD IpAddress,
    DWORD SubnetMask,
    int DhcpAction
);

typedef DNS_STATUS (WINAPI *P_DnsFlushResolverCache)(void);


const std::wstring CDnsConfigurator::NameServerKey = L"NameServer";
const std::wstring CDnsConfigurator::NetworkInterfacesKey = L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces";
const std::wstring CDnsConfigurator::NetworkInterfacesV6Key = L"SYSTEM\\CurrentControlSet\\Services\\Tcpip6\\Parameters\\Interfaces";
const std::wstring CDnsConfigurator::LocalHost = L"127.0.0.1";
const std::wstring CDnsConfigurator::LocalHostV6 = L"::1";


void CDnsConfigurator::ApplyChanges(const std::wstring& adapterName) 
{
	static P_DhcpNotifyConfigChange pDhcpNotifyConfigChange = nullptr;
    if (!pDhcpNotifyConfigChange) {
		HMODULE hModule = LoadLibraryW(L"dhcpcsvc.dll");
		if (hModule)
			pDhcpNotifyConfigChange = reinterpret_cast<P_DhcpNotifyConfigChange>(GetProcAddress(hModule, "DhcpNotifyConfigChange"));
    }

    static P_DnsFlushResolverCache pDnsFlushResolverCache = nullptr;
	if (!pDnsFlushResolverCache) {
		HMODULE hModule = LoadLibraryW(L"dnsapi.dll");
		if (hModule)
			pDnsFlushResolverCache = reinterpret_cast<P_DnsFlushResolverCache>(GetProcAddress(hModule, "DnsFlushResolverCache"));
	}


    // Call the DHCP notification with default parameters
    if(pDhcpNotifyConfigChange)
        pDhcpNotifyConfigChange(nullptr, adapterName.c_str(), FALSE, 0, 0, 0, 0);

    // Flush the DNS cache
    if(pDnsFlushResolverCache)
        pDnsFlushResolverCache();
}

std::vector<std::wstring> CDnsConfigurator::EnumerateSubKeys(HKEY hKey) 
{
    std::vector<std::wstring> subkeys;
    DWORD index = 0;
    WCHAR name[256];
    DWORD nameSize = sizeof(name) / sizeof(WCHAR);
    while (true) {
        nameSize = sizeof(name) / sizeof(WCHAR);
        LONG ret = RegEnumKeyExW(hKey, index, name, &nameSize, nullptr, nullptr, nullptr, nullptr);
        if (ret == ERROR_NO_MORE_ITEMS)
            break;
        if (ret == ERROR_SUCCESS)
            subkeys.push_back(name);
        ++index;
    }
    return subkeys;
}

std::vector<std::wstring> CDnsConfigurator::EnumerateValueNames(HKEY hKey) 
{
    std::vector<std::wstring> valueNames;
    DWORD index = 0;
    WCHAR name[256];
    DWORD nameSize = 0;
    DWORD type = 0;
    while (true) {
        nameSize = sizeof(name) / sizeof(WCHAR);
        LONG ret = RegEnumValueW(hKey, index, name, &nameSize, nullptr, &type, nullptr, nullptr);
        if (ret == ERROR_NO_MORE_ITEMS)
            break;
        if (ret == ERROR_SUCCESS)
            valueNames.push_back(name);
        ++index;
    }
    return valueNames;
}

bool CDnsConfigurator::IsLocalDNS(const std::wstring& regKey, const std::wstring& value) 
{
    HKEY hKey;
    LONG ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regKey.c_str(), 0, KEY_READ, &hKey);
    if (ret != ERROR_SUCCESS)
        return true; // if key not found, assume true

    auto subkeys = EnumerateSubKeys(hKey);
    RegCloseKey(hKey);

    for (const auto& subkeyName : subkeys) {
        std::wstring fullPath = regKey + L"\\" + subkeyName;
        HKEY hSubKey;
        ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, fullPath.c_str(), 0, KEY_READ, &hSubKey);
        if (ret != ERROR_SUCCESS)
            continue;
        DWORD type = 0;
        DWORD dataSize = 0;
        ret = RegQueryValueExW(hSubKey, NameServerKey.c_str(), nullptr, &type, nullptr, &dataSize);
        if (ret != ERROR_SUCCESS || type != REG_SZ) {
            RegCloseKey(hSubKey);
            return false;
        }
        std::vector<wchar_t> data(dataSize / sizeof(wchar_t));
        ret = RegQueryValueExW(hSubKey, NameServerKey.c_str(), nullptr, &type,
            reinterpret_cast<LPBYTE>(data.data()), &dataSize);
        RegCloseKey(hSubKey);
        if (ret != ERROR_SUCCESS)
            return false;
        std::wstring currentValue(data.data());
        if (currentValue != value)
            return false;
    }
    return true;
}

bool CDnsConfigurator::SetLocalDNS(const std::wstring& regKey, const std::wstring& value) 
{
    HKEY hKey;
    LONG ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regKey.c_str(), 0, KEY_READ | KEY_WRITE, &hKey);
    if (ret != ERROR_SUCCESS)
        return false;

    auto subkeys = EnumerateSubKeys(hKey);
    for (const auto& subkeyName : subkeys) {
        std::wstring fullPath = regKey + L"\\" + subkeyName;
        HKEY hSubKey;
        ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, fullPath.c_str(), 0, KEY_READ | KEY_WRITE, &hSubKey);
        if (ret != ERROR_SUCCESS)
            continue;

        auto valueNames = EnumerateValueNames(hSubKey);
        bool hasNameServer = (std::find(valueNames.begin(), valueNames.end(), NameServerKey) != valueNames.end());
        if (hasNameServer) {
            // Read current value.
            DWORD type = 0;
            DWORD dataSize = 0;
            ret = RegQueryValueExW(hSubKey, NameServerKey.c_str(), nullptr, &type, nullptr, &dataSize);
            if (ret == ERROR_SUCCESS && type == REG_SZ) {
                std::vector<wchar_t> data(dataSize / sizeof(wchar_t));
                ret = RegQueryValueExW(hSubKey, NameServerKey.c_str(), nullptr, &type,
                    reinterpret_cast<LPBYTE>(data.data()), &dataSize);
                if (ret == ERROR_SUCCESS) {
                    std::wstring currentValue(data.data());
                    if (currentValue != value) {
                        // If not already backed up, save the current value to NameServer_old.
                        bool hasOld = (std::find(valueNames.begin(), valueNames.end(), NameServerKey + L"_old") != valueNames.end());
                        if (!hasOld) {
                            RegSetValueExW(hSubKey, (NameServerKey + L"_old").c_str(), 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(currentValue.c_str()),
                                static_cast<DWORD>((currentValue.size() + 1) * sizeof(wchar_t)));
                        }
                        // Set the new NameServer value.
                        RegSetValueExW(hSubKey, NameServerKey.c_str(), 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(value.c_str()),
                            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
                        // Notify the system of the change.
                        ApplyChanges(subkeyName);
                    }
                }
            }
        }
        RegCloseKey(hSubKey);
    }
    RegCloseKey(hKey);
    return true;
}

void CDnsConfigurator::RestoreDNS(const std::wstring& regKey) 
{
    HKEY hKey;
    LONG ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regKey.c_str(), 0, KEY_READ | KEY_WRITE, &hKey);
    if (ret != ERROR_SUCCESS)
        return;

    auto subkeys = EnumerateSubKeys(hKey);
    for (const auto& subkeyName : subkeys) {
        std::wstring fullPath = regKey + L"\\" + subkeyName;
        HKEY hSubKey;
        ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, fullPath.c_str(), 0, KEY_READ | KEY_WRITE, &hSubKey);
        if (ret != ERROR_SUCCESS)
            continue;

        DWORD type = 0;
        DWORD dataSize = 0;
        ret = RegQueryValueExW(hSubKey, (NameServerKey + L"_old").c_str(), nullptr, &type, nullptr, &dataSize);
        if (ret == ERROR_SUCCESS && type == REG_SZ) {
            std::vector<wchar_t> data(dataSize / sizeof(wchar_t));
            ret = RegQueryValueExW(hSubKey, (NameServerKey + L"_old").c_str(), nullptr, &type,
                reinterpret_cast<LPBYTE>(data.data()), &dataSize);
            if (ret == ERROR_SUCCESS) {
                std::wstring oldValue(data.data());
                // Restore the NameServer value.
                RegSetValueExW(hSubKey, NameServerKey.c_str(), 0, REG_SZ,
                    reinterpret_cast<const BYTE*>(oldValue.c_str()),
                    static_cast<DWORD>((oldValue.size() + 1) * sizeof(wchar_t)));
                ApplyChanges(subkeyName);
                // Remove the backup value.
                RegDeleteValueW(hSubKey, (NameServerKey + L"_old").c_str());
            }
        }
        RegCloseKey(hSubKey);
    }
    RegCloseKey(hKey);
}

bool CDnsConfigurator::IsAnyLocalDNS(const std::wstring& regKey) 
{
    HKEY hKey;
    LONG ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, regKey.c_str(), 0, KEY_READ, &hKey);
    if (ret != ERROR_SUCCESS)
        return false;
    auto subkeys = EnumerateSubKeys(hKey);
    RegCloseKey(hKey);
    for (const auto& subkeyName : subkeys) {
        std::wstring fullPath = regKey + L"\\" + subkeyName;
        HKEY hSubKey;
        ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, fullPath.c_str(), 0, KEY_READ, &hSubKey);
        if (ret != ERROR_SUCCESS)
            continue;
        auto valueNames = EnumerateValueNames(hSubKey);
        RegCloseKey(hSubKey);
        if (std::find(valueNames.begin(), valueNames.end(), NameServerKey + L"_old") != valueNames.end())
            return true;
    }
    return false;
}