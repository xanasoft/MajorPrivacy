#include "pch.h"
#include "DbgHelp.h"

void DbgPrint(const wchar_t* format, ...)
{
	va_list argptr; 
	va_start(argptr, format);

	const size_t bufferSize = 10241;
	wchar_t bufferline[bufferSize];
#ifndef WIN32
	if (vswprintf_l(bufferline, bufferSize, sLine, argptr) == -1)
#else
	if (vswprintf(bufferline, bufferSize, format, argptr) == -1)
#endif
		bufferline[bufferSize - 1] = L'\0';

	va_end(argptr);

	OutputDebugStringW(bufferline);
}

uint64 GetUSTickCount()
{
	uint64 freq, now;
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
	QueryPerformanceCounter((LARGE_INTEGER*)&now);
	uint64 dwNow = ((now * 1000000) / freq) & 0xffffffff;
	return dwNow; // returns time since system start in us
}
