#pragma once

#include <phnt_windows.h>
#include <phnt.h>


//#if !defined(_AMD64_) && !defined(_ARM64_) && !defined(_ARM_)
//#define USE_WOW64EXT
//#endif

#ifdef __cplusplus
extern "C" {
#endif

DWORD64 FindDllBase64(HANDLE ProcessHandle, const WCHAR* dll);
BYTE* MapRemoteDll(HANDLE hProcess, DWORD64 DllBase);
DWORD64 FindDllExport(DWORD64 DllBase, const char* ProcName);
DWORD64 FindRemoteDllExport(HANDLE hProcess, DWORD64 DllBase, const char* ProcName);
DWORD64 FindDllExportFromFile(const WCHAR* dll, const char* ProcName);

BOOL hijack_thread_x86(HANDLE hProcess, HANDLE hThread, BOOL bWoW64, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);
BOOL inject_thread_x86(HANDLE hProcess, BOOL bWoW64, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);
BOOL hijack_function_x86(HANDLE hProcess, DWORD pTargetFunc, BOOL bWoW64, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);

BOOL hijack_thread_x64(HANDLE hProcess, HANDLE hThread, BOOL bARM64EC, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);
BOOL inject_thread_x64(HANDLE hProcess, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);
BOOL hijack_function_x64(HANDLE hProcess, DWORD64 pTargetFunc, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);

BOOL hijack_thread_a64(HANDLE hProcess, HANDLE hThread, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);
BOOL inject_thread_a64(HANDLE hProcess, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);
BOOL hijack_function_a64(HANDLE hProcess, DWORD64 pTargetFunc, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);
//
//BOOL hijack_thread_a32(HANDLE hProcess, HANDLE hThread, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);
//BOOL inject_thread_a32(HANDLE hProcess, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);
//BOOL hijack_function_a32(HANDLE hProcess, DWORD64 pTargetFunc, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size);

#ifdef __cplusplus
}
#endif

