/*
 * Copyright 2022 David Xanatos, xanasoft.com
 *
 */


#ifndef DLLIMPORT_H
#define DLLIMPORT_H
#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

DWORD64 FindDllBase64(HANDLE hProcess, const WCHAR* dll);
BYTE* MapRemoteDll(HANDLE hProcess, DWORD64 DllBase);
DWORD64 FindDllExport(DWORD64 DllBase, const char* ProcName);
DWORD64 FindRemoteDllExport(HANDLE hProcess, DWORD64 DllBase, const char* ProcName);
DWORD64 FindDllExportFromFile(const WCHAR* dll, const char* ProcName);

#ifdef __cplusplus
}
#endif

#endif /* DLLIMPORT_H */
