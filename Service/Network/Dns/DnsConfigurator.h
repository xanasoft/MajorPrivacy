#pragma once

class CDnsConfigurator
{
public:
    // Returns true if every interface under both IPv4 and IPv6 registry keys
    // has its NameServer value equal to the local host address.
    static bool IsLocalDNS() 
    {
        return IsLocalDNS(NetworkInterfacesKey, LocalHost) &&
            IsLocalDNS(NetworkInterfacesV6Key, LocalHostV6);
    }

    // Sets the NameServer value for every interface to the local host address.
    // (It also saves the old value under a NameServer_old value if not already saved.)
    static bool SetLocalDNS() 
    {
        return SetLocalDNS(NetworkInterfacesKey, LocalHost) &&
            SetLocalDNS(NetworkInterfacesV6Key, LocalHostV6);
    }

    // Restores the NameServer value from NameServer_old for every interface,
    // and then deletes the NameServer_old value.
    static void RestoreDNS() 
    {
        RestoreDNS(NetworkInterfacesKey);
        RestoreDNS(NetworkInterfacesV6Key);
    }

    // Returns true if any interface has a NameServer_old value.
    static bool IsAnyLocalDNS() 
    {
        return IsAnyLocalDNS(NetworkInterfacesKey) ||
            IsAnyLocalDNS(NetworkInterfacesV6Key);
    }

protected:
    // Registry key and value constants.
    static const std::wstring NameServerKey;
    static const std::wstring NetworkInterfacesKey;
    static const std::wstring NetworkInterfacesV6Key;
    static const std::wstring LocalHost;
    static const std::wstring LocalHostV6;

    // Applies the change notification and flushes the DNS resolver cache.
    static void ApplyChanges(const std::wstring& adapterName);

    // Helper: Enumerate subkeys under an open registry key.
    static std::vector<std::wstring> EnumerateSubKeys(HKEY hKey);

    // Helper: Enumerate value names under an open registry key.
    static std::vector<std::wstring> EnumerateValueNames(HKEY hKey);

    // Checks that every subkey under the given registry key has a NameServer value equal to 'value'.
    static bool IsLocalDNS(const std::wstring& regKey, const std::wstring& value);

    // Sets the NameServer value in every subkey under the given registry key to 'value'.
    // Saves the current value to NameServer_old if it isn’t already saved.
    static bool SetLocalDNS(const std::wstring& regKey, const std::wstring& value);

    // Restores the NameServer value from NameServer_old (if it exists) and deletes NameServer_old.
    static void RestoreDNS(const std::wstring& regKey);

    // Returns true if any subkey under the given registry key contains a NameServer_old value.
    static bool IsAnyLocalDNS(const std::wstring& regKey);
};






