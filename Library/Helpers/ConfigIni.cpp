#include "pch.h"
#include "ConfigIni.h"

#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>

#include "../Common/Strings.h"

#define MAX_CONF_LEN 32767 // Maximum size for profile keys

CConfigIni::CConfigIni(const std::wstring& iniFile, bool bWatchForChanges)
    : m_iniFile(iniFile)
{
    //LoadIniFile();

    if (bWatchForChanges){
        m_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (m_overlapped.hEvent != NULL) {
            if (!StartFileWatcher()) {
                //std::wcerr << L"Failed to start file watcher." << std::endl;
            }
        }
        else {
            //std::wcerr << L"Failed to create event for overlapped I/O." << std::endl;
        }
    }
}

CConfigIni::~CConfigIni()
{
    if (m_overlapped.hEvent != NULL) {
        StopFileWatcher();
        CloseHandle(m_overlapped.hEvent);
    }
}

std::wstring CConfigIni::GetValue(const std::string& section, const std::string& key, const std::wstring& defaultValue, bool* pOk) 
{
    std::lock_guard lock(m_mutex);

    // Check if value is cached
    std::string cacheKey = section + "." + key;
    auto it = m_cache.find(cacheKey);
    if (it != m_cache.end()) {
        if(pOk) *pOk = true;
        return it->second;
    }

    // Load value from INI file
    wchar_t buffer[MAX_CONF_LEN];
    if (!GetPrivateProfileStringW(s2w(section).c_str(), s2w(key).c_str(), defaultValue.c_str(), buffer, ARRAYSIZE(buffer), m_iniFile.c_str())) {
        //std::wcerr << L"Failed to read value from INI file." << std::endl;
        if(pOk) *pOk = false;
        return defaultValue;
    }
    std::wstring value(buffer);

    // Cache the value
    m_cache[cacheKey] = value;

    if(pOk) *pOk = true;
    return value;
}

void CConfigIni::SetValue(const std::string& section, const std::string& key, const std::wstring& value) 
{
    std::lock_guard lock(m_mutex);

    // Update cache
    std::string cacheKey = section + "." + key;
    m_cache[cacheKey] = value;

    // Write value to INI file
    if (!WritePrivateProfileStringW(s2w(section).c_str(), s2w(key).c_str(), value.c_str(), m_iniFile.c_str())) {
        //std::wcerr << L"Failed to write value to INI file." << std::endl;
    }
}

void CConfigIni::DeleteValue(const std::string& section, const std::string& key) 
{
    std::lock_guard lock(m_mutex);
    WritePrivateProfileStringW(s2w(section).c_str(), s2w(key).c_str(), NULL, m_iniFile.c_str());
    m_cache.erase(section + "." + key); // Remove from cache if present
}

void CConfigIni::DeleteSection(const std::string& section) 
{
    std::lock_guard lock(m_mutex);
    WritePrivateProfileStringW(s2w(section).c_str(), NULL, NULL, m_iniFile.c_str());

    // Remove all keys from cache belonging to the section
    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        if (it->first.find(section + ".") == 0)
            it = m_cache.erase(it);
        else
            ++it;
    }
}

std::vector<std::string> CConfigIni::ListSections() 
{
    wchar_t bufferW[MAX_CONF_LEN];
    GetPrivateProfileSectionNamesW(bufferW, sizeof(bufferW), m_iniFile.c_str());

    std::vector<std::string> sections;
    wchar_t* ptr = bufferW;
    while (*ptr != '\0') {
        sections.push_back(w2s(ptr));
        ptr += wcslen(ptr) + 1;
    }
    return sections;
}

std::vector<std::string> CConfigIni::ListKeys(const std::string& section) 
{
    wchar_t bufferW[MAX_CONF_LEN];
    GetPrivateProfileStringW(s2w(section).c_str(), NULL, L"", bufferW, sizeof(bufferW), m_iniFile.c_str());

    std::vector<std::string> keys;
    wchar_t* ptr = bufferW;
    while (*ptr != '\0') {
        keys.push_back(w2s(ptr));
        ptr += wcslen(ptr) + 1;
    }
    return keys;
}

void CConfigIni::InvalidateCache() 
{
    std::lock_guard lock(m_mutex);
    m_cache.clear();
}

/*std::wstring CConfigIni::GetValue(const std::string& section, const std::string& key, const std::wstring& defaultValue) 
{
    std::lock_guard lock(m_mutex);
    auto sec = m_content.find(section);
    if (sec != m_content.end()) {
        auto val = sec->second.find(key);
        if (val != sec->second.end()) {
            return val->second;
        }
    }
    return defaultValue;
}

void CConfigIni::SetValue(const std::string& section, const std::string& key, const std::wstring& value) 
{
    std::lock_guard lock(m_mutex);
    m_content[section][key] = value;
    WriteIniFile();
}

void CConfigIni::DeleteValue(const std::string& section, const std::string& key) 
{
    std::lock_guard lock(m_mutex);
    auto sec = m_content.find(section);
    if (sec != m_content.end()) {
        sec->second.erase(key);
        if (sec->second.empty()) {
            m_content.erase(sec);
        }
        WriteIniFile();
    }
}

void CConfigIni::DeleteSection(const std::string& section) 
{
    std::lock_guard lock(m_mutex);
    m_content.erase(section);
    WriteIniFile();
}

std::vector<std::string> CConfigIni::ListSections() 
{
    std::lock_guard lock(m_mutex);
    std::vector<std::string> sections;
    for (const auto& sec : m_content) {
        sections.push_back(sec.first);
    }
    return sections;
}

std::vector<std::string> CConfigIni::ListKeys(const std::string& section) 
{
    std::lock_guard lock(m_mutex);
    std::vector<std::string> keys;
    auto sec = m_content.find(section);
    if (sec != m_content.end()) {
        for (const auto& key : sec->second) {
            keys.push_back(key.first);
        }
    }
    return keys;
}

void CConfigIni::LoadIniFile() 
{
    std::wifstream file(m_iniFile);
    if (!file.is_open()) {
        return;
    }

    std::wstring line, section;
    while (std::getline(file, line)) 
    {
        line = Trim(line);

        if (line.empty() || line[0] == L';')
            continue; // Skip empty lines and comments

        if (line.front() == L'[' && line.back() == L']') {
            section = line.substr(1, line.size() - 2);
            continue;
        } 
            
        auto delimiterPos = line.find(L'=');
        if (delimiterPos != std::wstring::npos) {
            std::string key = Trim(line.substr(0, delimiterPos));
            std::string value = Trim(line.substr(delimiterPos + 1));
            m_content[section][key] = value;
        }
    }
}

void CConfigIni::WriteIniFile()
{
    std::wofstream file(m_iniFile);
    if (!file.is_open()) {
        //std::wcerr << L"Unable to open file for writing: " << m_iniFile << std::endl;
        return;
    }

    for (const auto& section : m_content) 
    {
        file << L'[' << section.first << L']' << std::endl;
        for (const auto& key_value : section.second)
            file << key_value.first << L'=' << key_value.second << std::endl;
        file << std::endl;
    }

    file.close();
}*/

bool CConfigIni::StartFileWatcher() 
{
    if (m_watcherThread.joinable())
        return true;
    m_stopWatcher.store(false);
    m_watcherThread = std::thread(&CConfigIni::FileWatcher, this);
    return m_watcherThread.joinable();
}

void CConfigIni::StopFileWatcher()
{
    m_stopWatcher.store(true);
    SetEvent(m_overlapped.hEvent); // Signal the event to wake up the watcher thread
    if (m_watcherThread.joinable())
        m_watcherThread.join();
}

void CConfigIni::FileWatcher() 
{
    std::wstring directory = m_iniFile.substr(0, m_iniFile.find_last_of(L"\\/"));

    HANDLE hDir = CreateFileW(
        directory.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);

    if (hDir == INVALID_HANDLE_VALUE) {
        //std::wcerr << L"Failed to create file handle for directory watching." << std::endl;
        return;
    }

    char buffer[1024];
    DWORD bytesReturned;

    while (!m_stopWatcher.load()) {
        if (!ReadDirectoryChangesW(
                hDir, buffer, sizeof(buffer), FALSE,
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                &bytesReturned, &m_overlapped, NULL)) {
            //std::wcerr << L"Failed to read directory changes." << std::endl;
            break;
        }

        if (WaitForSingleObject(m_overlapped.hEvent, INFINITE) == WAIT_OBJECT_0) {
            ResetEvent(m_overlapped.hEvent);
            if (m_stopWatcher.load())
                break;
            InvalidateCache();
            //LoadIniFile();
        }
    }

    CloseHandle(hDir);
}

std::wstring CConfigIni::GetAppDataFolder(bool bUser)
{
    std::wstring AppData;

    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(bUser ? FOLDERID_RoamingAppData : FOLDERID_ProgramData, 0, NULL, &path)))
        AppData = std::wstring(path);
    if (path)
        CoTaskMemFree(path);

    return AppData;
}
