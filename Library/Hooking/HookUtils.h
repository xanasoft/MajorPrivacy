#pragma once

#include "../lib_global.h"

extern "C" LIBRARY_EXPORT ULONG InitInject();
extern "C" LIBRARY_EXPORT ULONG InjectLdr(HANDLE hProcess, ULONG flags);

LIBRARY_EXPORT bool HookFunction(void* pFunction, void* pHook, void** ppOriginal);

LIBRARY_EXPORT ULONG InjectDll(HANDLE hProcess, HANDLE hThread, ULONG flags);