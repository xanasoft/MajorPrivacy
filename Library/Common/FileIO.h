#pragma once
#include "../lib_global.h"

class CVariant;
class CBuffer;

LIBRARY_EXPORT std::string ToPlatformNotation(const std::wstring& Path);
LIBRARY_EXPORT bool RemoveFile(const std::wstring& Path);
LIBRARY_EXPORT bool RenameFile(const std::wstring& OldPath, const std::wstring& NewPath);
LIBRARY_EXPORT bool WriteFile(const std::wstring& Path, const std::wstring& Data);
LIBRARY_EXPORT bool ReadFile(const std::wstring& Path, std::wstring& Data);
LIBRARY_EXPORT bool WriteFile(const std::wstring& Path, const std::string& Data);
LIBRARY_EXPORT bool ReadFile(const std::wstring& Path, std::string& Data);
LIBRARY_EXPORT bool WriteFile(const std::wstring& Path, const CVariant& Data);
LIBRARY_EXPORT bool ReadFile(const std::wstring& Path, CVariant& Data);
LIBRARY_EXPORT bool WriteFile(const std::wstring& Path, uint64 Offset, const CBuffer& Data);
LIBRARY_EXPORT bool ReadFile(const std::wstring& Path, uint64 Offset, CBuffer& Data);
LIBRARY_EXPORT bool ListDir(const std::wstring& Path, std::vector<std::wstring>& Entries, const wchar_t* Filter = NULL);
LIBRARY_EXPORT long GetFileSize(const std::wstring& Path);