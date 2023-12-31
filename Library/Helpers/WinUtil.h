#pragma once
#include "../lib_global.h"
#include "../Types.h"


// list and detection from PHlib
#ifndef WINDOWS_ANCIENT
#define WINDOWS_ANCIENT 0
#define WINDOWS_XP 51           // August, 2001
#define WINDOWS_SERVER_2003 52  // April, 2003
#define WINDOWS_VISTA 60        // November, 2006
#define WINDOWS_7 61            // July, 2009
#define WINDOWS_8 62            // August, 2012
#define WINDOWS_8_1 63          // August, 2013
#define WINDOWS_10 100          // July, 2015
#define WINDOWS_10_TH2 101      // November, 2015
#define WINDOWS_10_RS1 102      // August, 2016
#define WINDOWS_10_RS2 103      // April, 2017
#define WINDOWS_10_RS3 104      // October, 2017
#define WINDOWS_10_RS4 105      // April, 2018
#define WINDOWS_10_RS5 106      // November, 2018
#define WINDOWS_10_19H1 107     // May, 2019
#define WINDOWS_10_19H2 108     // November, 2019
#define WINDOWS_10_20H1 109     // May, 2020
#define WINDOWS_10_20H2 110     // October, 2020
#define WINDOWS_10_21H1 111     // May, 2021
#define WINDOWS_10_21H2 112     // November, 2021
#define WINDOWS_10_22H2 113     // October, 2022
#define WINDOWS_11 114          // October, 2021
#define WINDOWS_11_22H1 115     // February, 2022
#define WINDOWS_NEW ULONG_MAX
#endif

enum class EWinEd
{
    Any = 0,
    Home = 1,
    Pro = 2,
    Enterprise = 3 // or education or server
};

extern LIBRARY_EXPORT uint32& g_WindowsVersion;
extern LIBRARY_EXPORT RTL_OSVERSIONINFOEXW& g_OsVersion;
extern LIBRARY_EXPORT EWinEd m_OsEdition;

void InitSysInfo();


FORCEINLINE VOID NTAPI PhServiceWorkaroundWindowsServiceTypeBug(_Inout_ LPENUM_SERVICE_STATUS_PROCESS ServiceEntry)
{
    // SERVICE_WIN32 is not a valid ServiceType: https://github.com/winsiderss/systeminformer/issues/120 (dmex)
    if (ServiceEntry->ServiceStatusProcess.dwServiceType == SERVICE_WIN32)
        ServiceEntry->ServiceStatusProcess.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
    if (ServiceEntry->ServiceStatusProcess.dwServiceType == (SERVICE_WIN32 | SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE))
        ServiceEntry->ServiceStatusProcess.dwServiceType = SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE;
}