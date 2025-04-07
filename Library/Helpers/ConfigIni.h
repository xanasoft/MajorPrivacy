#pragma once
#include "../lib_global.h"
#include "../Status.h"

class LIBRARY_EXPORT CConfigIni
{
public:
	CConfigIni(const std::wstring& iniFile, bool bWatchForChanges = false);
	virtual ~CConfigIni();

    /*
    StVariant GetValue(const std::string& section, const std::string& key, const StVariant& defaultValue = StVariant());

    std::string GetStr(const std::string& section, const std::string& key, const std::wstring& defaultValue = L"") {
    return GetValue(section, key, defaultValue).AsStr();
    }

    sint32 GetInt(const std::string& section, const std::string& key, sint32 defaultValue) {
    bool bOk = false;
    sint32 Val = GetValue(section, key, defaultValue).AsNum<sint32>(&bOk);
    return bOk ? Val : defaultValue;
    }

    uint32 GetUInt(const std::string& section, const std::string& key, uint32 defaultValue) {
    bool bOk = false;
    uint32 Val = GetValue(section, key, defaultValue).AsNum<uint32>(&bOk);
    return bOk ? Val : defaultValue;
    }

    int64_t GetInt64(const std::string& section, const std::string& key, int64_t defaultValue) {
    bool bOk = false;
    sint64 Val = GetValue(section, key, defaultValue).AsNum<sint64>(&bOk);
    return bOk ? Val : defaultValue;
    }

    uint64_t GetUInt64(const std::string& section, const std::string& key, uint64_t defaultValue) {
    bool bOk = false;
    uint64 Val = GetValue(section, key, defaultValue).AsNum<uint64>(&bOk);
    return bOk ? Val : defaultValue;
    }

    bool GetBool(const std::string& section, const std::string& key, bool defaultValue) {
    StVariant value = GetValue(section, key, defaultValue);
    if(value.GetType() == VAR_TYPE_UINT || value.GetType() == VAR_TYPE_SINT)
    return value.AsNum<uint32>() != 0;
    std::wstring str = value.AsStr();
    return _wcsicmp(str.c_str(), L"true") == 0 || str == L"1";
    }

    void SetValue(const std::string& section, const std::string& key, const StVariant& value);
    */
	
	std::wstring GetValue(const std::string& section, const std::string& key, const std::wstring& defaultValue = L"", bool* pOk = NULL);

	sint32 GetInt(const std::string& section, const std::string& key, sint32 defaultValue) {
        return std::stoi(GetValue(section, key, std::to_wstring(defaultValue)));
    }

    uint32 GetUInt(const std::string& section, const std::string& key, uint32 defaultValue) {
        return std::stoul(GetValue(section, key, std::to_wstring(defaultValue)));
    }

    int64_t GetInt64(const std::string& section, const std::string& key, int64_t defaultValue) {
        return std::stoll(GetValue(section, key, std::to_wstring(defaultValue)));
    }

    uint64_t GetUInt64(const std::string& section, const std::string& key, uint64_t defaultValue) {
        return std::stoull(GetValue(section, key, std::to_wstring(defaultValue)));
    }

    bool GetBool(const std::string& section, const std::string& key, bool defaultValue) {
        std::wstring value = GetValue(section, key, defaultValue ? L"true" : L"false");
        return value == L"true" || value == L"1";
    }


	void SetValue(const std::string& section, const std::string& key, const std::wstring& value);

    void SetInt(const std::string& section, const std::string& key, sint32 value) {
        SetValue(section, key, std::to_wstring(value));
    }

    void SetUInt(const std::string& section, const std::string& key, uint32 value) {
        SetValue(section, key, std::to_wstring(value));
    }

    void SetInt64(const std::string& section, const std::string& key, int64_t value) {
        SetValue(section, key, std::to_wstring(value));
    }

    void SetUInt64(const std::string& section, const std::string& key, uint64_t value) {
        SetValue(section, key, std::to_wstring(value));
    }

    void SetBool(const std::string& section, const std::string& key, bool value) {
        SetValue(section, key, value ? L"true" : L"false");
    }


    void DeleteValue(const std::string& section, const std::string& key);

    void DeleteSection(const std::string& section);

    std::vector<std::string> ListSections();
    std::vector<std::string> ListKeys(const std::string& section);

	void InvalidateCache();

	static std::wstring GetAppDataFolder(bool bUser = false);

protected:

    //void LoadIniFile();
    //void WriteIniFile();

	bool StartFileWatcher();
	void StopFileWatcher();
	void FileWatcher();

	std::wstring m_iniFile;
    //std::map<std::string, std::map<std::string, std::wstring>> m_content;
    std::map<std::string, std::wstring> m_cache;
    std::thread m_watcherThread;
    std::atomic<bool> m_stopWatcher;
    std::mutex m_mutex;
    OVERLAPPED m_overlapped = {};
};