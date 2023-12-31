#include "pch.h"
#include "MiscHelpers.h"

unsigned __int64 GetTime()
{
	__int64 iTime = 0;
	_time64(&iTime);
	return (unsigned)iTime;
}

unsigned __int64 GetCurTick()
{
	return ::GetTickCount64();
}

unsigned __int64 GetCurTimeStamp()
{
	FILETIME ftNow;
	LARGE_INTEGER liNow;
	__int64 now;

	GetSystemTimeAsFileTime(&ftNow);
	liNow.LowPart = ftNow.dwLowDateTime;
	liNow.HighPart = ftNow.dwHighDateTime;
	now = liNow.QuadPart;

	return (unsigned __int64)now;
}