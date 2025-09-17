#include "pch.h"
#include "WinUtil.h"
#include <ph.h>
#include <shlwapi.h>

uint32& g_WindowsVersion = *(uint32*)&WindowsVersion;
RTL_OSVERSIONINFOEXW& g_OsVersion = *(RTL_OSVERSIONINFOEXW*)&PhOsVersion;
EWinEd m_OsEdition = EWinEd::Any;

void InitSysInfo()
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD type;
        DWORD bufSize;

        WCHAR productName[256];
        bufSize = ARRAYSIZE(productName);
        if (RegQueryValueEx(hKey, L"ProductName", NULL, &type, (LPBYTE)productName, &bufSize) != ERROR_SUCCESS)
            productName[0] = '\0';

        WCHAR installationType[256];
        bufSize = ARRAYSIZE(installationType);
        if (RegQueryValueEx(hKey, L"InstallationType", NULL, &type, (LPBYTE)installationType, &bufSize) != ERROR_SUCCESS)
            installationType[0] = '\0';
            
        RegCloseKey(hKey);

        if (_wcsicmp(installationType, L"Server") == 0 || StrStrIW(productName, L"Education") != NULL || StrStrIW(productName, L"Enterprise") != NULL)
            m_OsEdition = EWinEd::Enterprise;
        else //if (_wcsicmp(typeStr.c_str(), L"Client") == 0)
        {
            if (StrStrIW(productName, L"Pro") != NULL)
                m_OsEdition = EWinEd::Pro;
            else if (StrStrIW(productName, L"Home") != NULL)
                m_OsEdition = EWinEd::Home;
        }
    }
}

VOID MySetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription)
{
    typedef HRESULT(WINAPI* P_SetThreadDescription)(HANDLE hThread, PCWSTR lpThreadDescription);
    P_SetThreadDescription pSetThreadDescription = (P_SetThreadDescription) -1;
    if (pSetThreadDescription == (P_SetThreadDescription)-1)
    {
        HMODULE hMod = GetModuleHandleW(L"kernel32.dll");
        if (hMod)
            pSetThreadDescription = (P_SetThreadDescription)GetProcAddress(hMod, "SetThreadDescription");
    }

    if (pSetThreadDescription)
        pSetThreadDescription(hThread, lpThreadDescription);
}