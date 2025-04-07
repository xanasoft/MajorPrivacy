#pragma once
#include "../lib_global.h"

class StVariant;
class CBuffer;

LIBRARY_EXPORT std::string ToPlatformNotation(const std::wstring& Path);
LIBRARY_EXPORT bool FileExists(const std::wstring& Path);
LIBRARY_EXPORT time_t GetFileModificationUnixTime(const std::wstring& Path);
LIBRARY_EXPORT bool SetFileModificationUnixTime(const std::wstring& Path, time_t newTime);
LIBRARY_EXPORT bool RemoveFile(const std::wstring& Path);
LIBRARY_EXPORT bool RenameFile(const std::wstring& OldPath, const std::wstring& NewPath);
//LIBRARY_EXPORT bool WriteFile(const std::wstring& Path, const std::wstring& Data);
//LIBRARY_EXPORT bool ReadFile(const std::wstring& Path, std::wstring& Data);
//LIBRARY_EXPORT bool WriteFile(const std::wstring& Path, const std::string& Data);
//LIBRARY_EXPORT bool ReadFile(const std::wstring& Path, std::string& Data);
LIBRARY_EXPORT bool WriteFile(const std::wstring& Path, const StVariant& Data);
LIBRARY_EXPORT bool ReadFile(const std::wstring& Path, StVariant& Data);
LIBRARY_EXPORT bool WriteFile(const std::wstring& Path, const CBuffer& Data, uint64 Offset = -1);
LIBRARY_EXPORT bool ReadFile(const std::wstring& Path, CBuffer& Data, uint64 Offset = -1);
LIBRARY_EXPORT bool WriteFile(const std::wstring& Path, const std::vector<BYTE>& Data);
LIBRARY_EXPORT bool ReadFile(const std::wstring& Path, std::vector<BYTE>& Data);
LIBRARY_EXPORT bool ListDir(const std::wstring& Path, std::vector<std::wstring>& Entries, const wchar_t* Filter = NULL);
LIBRARY_EXPORT unsigned long long GetFileSize(const std::wstring& Path);
LIBRARY_EXPORT bool CreateFullPath(std::wstring Path);