#pragma once
#include "../lib_global.h"
#include "../Status.h"
#include "../Framework/Common/Buffer.h"

LIBRARY_EXPORT STATUS QueryTokenVariable(HANDLE TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass, CBuffer& Buffer);

LIBRARY_EXPORT bool TokenIsAdmin(HANDLE hToken, bool OnlyFull);

LIBRARY_EXPORT bool IsRunningAsAdmin();

LIBRARY_EXPORT bool IsRunningAsSystem();

LIBRARY_EXPORT bool RunAsSystem(const std::wstring& CommandLine, HANDLE* phProcess = NULL);

LIBRARY_EXPORT bool RunAsSystemTask(const std::wstring& CommandLine, HANDLE* phProcess = NULL);