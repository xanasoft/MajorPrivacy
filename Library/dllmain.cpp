// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "../Framework/Common/Buffer.h"
#include "../Framework/Common/PathTree.h"
#include "Helpers/WinUtil.h"

#include <phnt_windows.h>
#include <phnt.h>

//#include "../NtCRT/NtCRT.h"

extern "C" NTSTATUS NTAPI PhInitializePhLib(PCWSTR ApplicationName, PVOID ImageBaseAddress);

void DllInitialize()
{
    PhInitializePhLib(L"System Informer", NULL);
    InitSysInfo();
}

//extern "C" void InitGeneralCRT(struct _MY_CRT* MyCRT)
//{
//    InitNtCRT(MyCRT);
//
//    PhInitializePhLib(L"System Informer", NULL);
//}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DllInitialize();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

