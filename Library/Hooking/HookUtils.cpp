#include "pch.h"
#include "HookUtils.h"
#include "../Helpers/Scoped.h"

#include "./Detours/detours.h"

#include "./injdll.h"

bool HookFunction(void* pFunction, void* pHook, void** ppOriginal)
{
	DetourTransactionBegin();
	DetourUpdateThread(NtCurrentThread());

	LONG error = DetourAttach((PVOID*)&pFunction, pHook);

	DetourTransactionCommit();

	if (error == NO_ERROR)
	{
		*ppOriginal = pFunction;
		return true;
	}
	return false;
}

typedef BOOL (WINAPI *P_IsWow64Process2)(
	HANDLE hProcess,
	USHORT* pProcessMachine,
	USHORT* pNativeMachine
	);
static P_IsWow64Process2 pIsWow64Process2 = NULL;

ULONG InjectDll(HANDLE hProcess, HANDLE hThread, ULONG flags)
{
	if (!pIsWow64Process2)
	{
		HMODULE KernelBaseDll = GetModuleHandleW(L"kernelbase.dll");

		pIsWow64Process2 = (P_IsWow64Process2)GetProcAddress(KernelBaseDll, "IsWow64Process2");
		if(!pIsWow64Process2)
			pIsWow64Process2 = (P_IsWow64Process2)-1;
	}

	// We only support 64-bit hosts

	USHORT NativeMachine = 0xFFFF;
	USHORT ProcessMachine = 0xFFFF;
	if (pIsWow64Process2 != (P_IsWow64Process2)-1)
	{
		pIsWow64Process2(hProcess, &ProcessMachine, &NativeMachine);
		if (ProcessMachine == IMAGE_FILE_MACHINE_UNKNOWN)
			ProcessMachine = NativeMachine;
	}
	else // older then Windows Server 2016 no ARM64 support, and we dont support 32-bit must be AMD64
	{
		NativeMachine = IMAGE_FILE_MACHINE_AMD64;

		BOOL bWow64 = FALSE;
		IsWow64Process(hProcess, &bWow64);
		if(bWow64)
			ProcessMachine = IMAGE_FILE_MACHINE_I386;
		else
			ProcessMachine = IMAGE_FILE_MACHINE_AMD64;
	}

	const WCHAR *HookDll = L"\\CaptainHook.dll";

	WCHAR dllPath[512];
	GetModuleFileName(NULL, dllPath, 512);
	WCHAR* bs = wcsrchr(dllPath, L'\\');
	if (bs) *bs = L'\0';

	switch (ProcessMachine)
	{
	case IMAGE_FILE_MACHINE_I386: {
			
			wcscat_s(dllPath, 512, L"\\Win32");
			wcscat_s(dllPath, 512, HookDll);

			return hijack_thread_x86(hProcess, hThread, TRUE, dllPath, NULL, NULL, 0) ? 0 : -1;
		}
	case IMAGE_FILE_MACHINE_AMD64: {

			bool bARM64EC = NativeMachine == IMAGE_FILE_MACHINE_ARM64;

			if (bARM64EC)
				wcscat_s(dllPath, 512, L"\\ARM64EC");
			else
				wcscat_s(dllPath, 512, L"\\x64");
			wcscat_s(dllPath, 512, HookDll);

			return hijack_thread_x64(hProcess, hThread, bARM64EC, dllPath, NULL, NULL, 0) ? 0 : -1;
		}
	case IMAGE_FILE_MACHINE_ARM64:
		{
			wcscat_s(dllPath, 512, L"\\ARM64");
			wcscat_s(dllPath, 512, HookDll);

			return hijack_thread_a64(hProcess, hThread, dllPath, NULL, NULL, 0) ? 0 : -1;
		}
	}

	return -1;
}