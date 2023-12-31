#pragma once
#include "../lib_global.h"
#include "../Status.h"


LIBRARY_EXPORT std::vector<std::wstring> EnumTasks(const std::wstring& path);
LIBRARY_EXPORT bool IsTaskEnabled(const std::wstring& path, const std::wstring& taskName);
LIBRARY_EXPORT bool SetTaskEnabled(const std::wstring& path, const std::wstring& taskName, bool setValue);
LIBRARY_EXPORT DWORD GetTaskStateKnown(const std::wstring& path, const std::wstring& taskName);