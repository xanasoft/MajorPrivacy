#pragma once
#include "../lib_global.h"

LIBRARY_EXPORT void DbgPrint(const wchar_t* format, ...);

LIBRARY_EXPORT uint64 GetUSTickCount();
