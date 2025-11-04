/*
 * Copyright 2020-2024 David Xanatos, xanasoft.com
 */

#include <phnt_windows.h>
#include <phnt.h>
#include <math.h>

#include "../lib_global.h"


LIBRARY_EXPORT ULONG InitInject();
LIBRARY_EXPORT ULONG InjectLdr(HANDLE hProcess, ULONG flags);

#include "../../LdrCode/LdrData.h"
#include "../../Library/Hooking/dllimport.h"
#include "../../Library/Hooking/arm64_asm.h"

typedef struct _MY_TARGETS {
	unsigned long long Entry;
	unsigned long long Data;
	unsigned long long Detour;
} MY_TARGETS; // keep in sync with all entry_*.asm


void* InjectLdr_AllocMemory(HANDLE hProcess, SIZE_T uSize, BOOLEAN bExecutable, BOOLEAN use_arm64ec);

void *InjectLdr_CopyCode(HANDLE hProcess, SIZE_T uCodeSize, const void* pCode, BOOLEAN UseArm64ec);

BOOLEAN InjectLdr_BuildTramp(UCHAR* pTrampCode, ULONG_PTR TrampAddr, int LongDiff, BOOLEAN UseArm64ec);

BOOLEAN InjectLdr_WriteJump(HANDLE hProcess, ULONG_PTR Detour, int LongDiff, BOOLEAN UseArm64ec);

ULONG g_OsBuild = 0;

// 64-bit loader code
void *g_LdrCode_Ptr = NULL;
ULONG g_LdrCode_Size = 0;
ULONG g_LdrCode_Start_Offset = 0;
ULONG g_LdrCode_Data_Offset = 0;
ULONG g_LdrCode_Detour_Offset = 0;

// 32-bit loader code
void *g_LdrCode32_Ptr = NULL;
ULONG g_LdrCode32_Size = 0;
ULONG g_LdrCode32_Detour_Offset = 0;

// extra data template
LDRCODE_EXTRA_DATA* g_LdrCode_ExtraData = NULL;
SIZE_T g_LdrCode_ExtraData_Size = 0;

WCHAR g_NtDllPath[MAX_PATH];

UCHAR g_NtDllSavedCode[NATIVE_FUNCTION_SIZE * NATIVE_FUNCTION_COUNT]; // see NATIVE_FUNCTION_NAMES

ULONG_PTR g_LdrInitializeThunk = 0;
ULONG_PTR g_LdrInitializeThunkEC = 0;

BOOLEAN g_DisableCHPE = TRUE;

BOOLEAN g_IsOnArm64 = FALSE;

typedef BOOL (WINAPI *P_IsWow64Process2)(
	HANDLE hProcess,
	USHORT* pProcessMachine,
	USHORT* pNativeMachine
	);

typedef BOOL (WINAPI *P_GetProcessInformation)(
	HANDLE hProcess,
	PROCESS_INFORMATION_CLASS ProcessInformationClass,
	LPVOID ProcessInformation,
	DWORD ProcessInformationSize
	);
P_GetProcessInformation pGetProcessInformation = NULL;

typedef PVOID (WINAPI *P_VirtualAlloc2)(
	HANDLE Process,
	PVOID BaseAddress,
	SIZE_T Size,
	ULONG AllocationType,
	ULONG PageProtection,
	MEM_EXTENDED_PARAMETER* ExtendedParameters,
	ULONG ParameterCount
);
P_VirtualAlloc2 pVirtualAlloc2 = NULL;

typedef enum _ELdrArch
{
	eLdr_x86,
	eLdr_x64,
	eLdr_a64,
} ELdrArch;

ULONG InjectLdr_LoadCode(ELdrArch LdrArch, void **ppLdrCode, ULONG *pLdrCodeSize, ULONG *pStartOffset, ULONG* pDataOffset, ULONG* pDetourOffset)
{
	HRSRC res = 0;
	switch (LdrArch)
	{
	case eLdr_x86: res = FindResource(NULL, L"LDRCODE_X86", RT_RCDATA); break;
	case eLdr_x64: res = FindResource(NULL, L"LDRCODE_X64", RT_RCDATA); break;
	case eLdr_a64: res = FindResource(NULL, L"LDRCODE_A64", RT_RCDATA); break;
	}
	if (! res)
        return 0x10;

    ULONG uSize = SizeofResource(NULL, res);
    if (! uSize)
		return 0x11;

    HGLOBAL glob = LoadResource(NULL, res);
    if (! glob)
		return 0x12;

    UCHAR *pData = (UCHAR *)LockResource(glob);
    if (! pData)
		return 0x13;

	IMAGE_DOS_HEADER* dos_hdr = (IMAGE_DOS_HEADER *)pData;

	IMAGE_NT_HEADERS *nt_hdrs = NULL;
	ULONG_PTR ImageBase = 0;
    if (dos_hdr->e_magic == 'MZ' || dos_hdr->e_magic == 'ZM') 
	{
        nt_hdrs = (IMAGE_NT_HEADERS *)((UCHAR *)dos_hdr + dos_hdr->e_lfanew);

		if (nt_hdrs->Signature != IMAGE_NT_SIGNATURE)   // 'PE\0\0'
			return 0x14;
		if (nt_hdrs->OptionalHeader.Magic != ((LdrArch != eLdr_x86) ? IMAGE_NT_OPTIONAL_HDR64_MAGIC : IMAGE_NT_OPTIONAL_HDR32_MAGIC)) 
			return 0x15;

        if (LdrArch == eLdr_x86)
		{
            IMAGE_NT_HEADERS32 *nt_hdrs_32 = (IMAGE_NT_HEADERS32 *)nt_hdrs;
            IMAGE_OPTIONAL_HEADER32 *opt_hdr_32 = &nt_hdrs_32->OptionalHeader;
			ImageBase = opt_hdr_32->ImageBase;
        }
		else 
		{
            IMAGE_NT_HEADERS64 *nt_hdrs_64 = (IMAGE_NT_HEADERS64 *)nt_hdrs;
            IMAGE_OPTIONAL_HEADER64 *opt_hdr_64 = &nt_hdrs_64->OptionalHeader;
			ImageBase = (ULONG_PTR)opt_hdr_64->ImageBase;
        }
    }

	ULONG zzzzz = 1;
	if (LdrArch == eLdr_a64) 
		zzzzz = 3; // ARM64 only (3 with VS2022, 4 with VS2019)
	else if (ImageBase != 0) // x64 or x86
		return 0x16;

	IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt_hdrs);
    if (nt_hdrs->FileHeader.NumberOfSections < 2) return 0x17;
    if (strncmp((char *)section[0].Name, LDRCODE_INJECTION_SECTION, strlen(LDRCODE_INJECTION_SECTION)) ||
        strncmp((char *)section[zzzzz].Name, LDRCODE_SYMBOL_SECTION, strlen(LDRCODE_SYMBOL_SECTION))) {
		return 0x18;
    }

	MY_TARGETS *targets = (MY_TARGETS *)& pData[section[zzzzz].PointerToRawData];
    if(pStartOffset) *pStartOffset= (ULONG)(targets->Entry - ImageBase - section[0].VirtualAddress);
    if(pDataOffset) *pDataOffset= (ULONG)(targets->Data - ImageBase - section[0].VirtualAddress);
	if(pDetourOffset) *pDetourOffset = (ULONG)(targets->Detour - ImageBase - section[0].VirtualAddress);

    *ppLdrCode = pData + section[0].PointerToRawData;
    *pLdrCodeSize = section[0].SizeOfRawData;

	return 0;
}

ULONG InitInject()
{
	RtlGetNtVersionNumbers(NULL, NULL, &g_OsBuild);
	g_OsBuild &= 0x0000FFFF;

	HMODULE KernelBaseDll = GetModuleHandleW(L"kernelbase.dll");

	P_IsWow64Process2 pIsWow64Process2 = (P_IsWow64Process2)GetProcAddress(KernelBaseDll, "IsWow64Process2");
	if (pIsWow64Process2)
	{
		USHORT ProcessMachine = 0xFFFF;
		USHORT NativeMachine = 0xFFFF;
		BOOL ok = pIsWow64Process2(GetCurrentProcess(), &ProcessMachine, &NativeMachine);

		if (NativeMachine == IMAGE_FILE_MACHINE_ARM64)
			g_IsOnArm64 = TRUE;
	}

	pVirtualAlloc2 = (P_VirtualAlloc2)GetProcAddress(KernelBaseDll, "VirtualAlloc2");

	pGetProcessInformation = (P_GetProcessInformation)GetProcAddress(KernelBaseDll, "GetProcessInformation");


	ULONG err;
	if(g_IsOnArm64)
		err = InjectLdr_LoadCode(eLdr_a64, &g_LdrCode_Ptr, &g_LdrCode_Size, &g_LdrCode_Start_Offset, &g_LdrCode_Data_Offset, &g_LdrCode_Detour_Offset);
	else
		err = InjectLdr_LoadCode(eLdr_x64, &g_LdrCode_Ptr, &g_LdrCode_Size, &g_LdrCode_Start_Offset, &g_LdrCode_Data_Offset, &g_LdrCode_Detour_Offset);
	if(!err)
		err = InjectLdr_LoadCode(FALSE, &g_LdrCode32_Ptr, &g_LdrCode32_Size, NULL, NULL, &g_LdrCode32_Detour_Offset);
	if (err)
		return err;

	if (!GetSystemDirectory(g_NtDllPath, MAX_PATH)) // todo improve <<<<<<<<<<<<<<
		wcscpy(g_NtDllPath, L"C:\\Windows\\System32");
	wcscat(g_NtDllPath, L"\\ntdll.dll");

	//
	// Copy needed native ntdll functions
	//

	HMODULE NtDll = GetModuleHandleW(L"ntdll.dll");
	if(!NtDll) return 0x20;
	const char* NtdllExports[] = NATIVE_FUNCTION_NAMES;
	for (ULONG i = 0; i < NATIVE_FUNCTION_COUNT; ++i) 
	{
		ULONG_PTR func_ptr = (ULONG_PTR)NtDll + FindDllExportFromFile(g_NtDllPath, NtdllExports[i]);
		if(!func_ptr) return 0x21 + i;
		memcpy((void*)((ULONG_PTR)g_NtDllSavedCode + (NATIVE_FUNCTION_SIZE * i)), (void*)func_ptr, NATIVE_FUNCTION_SIZE);
	}

    //
    // record information about ntdll and the virtual memory system
    //

	g_LdrInitializeThunk = (ULONG_PTR)NtDll + FindDllExportFromFile(g_NtDllPath, "LdrInitializeThunk");
    if (!g_LdrInitializeThunk)
		return 0x30;

	if (g_IsOnArm64)
	{
		//
		// for x64 on arm64 we need the EC version of LdrInitializeThunk as well as VirtualAlloc2, 
		// if those are missing we will only fail InjectLdr for x64 processes on arm64
		//

		g_LdrInitializeThunkEC = (ULONG_PTR)NtDll + FindDllExportFromFile(g_NtDllPath, "#LdrInitializeThunk");
	}
	else if (g_OsBuild >= 10000) // Win 10
	{
		unsigned char* code;
		code = (unsigned char*)g_LdrInitializeThunk;
		if (*(ULONG*)&code[0] == 0x24048b48 && code[0xa] == 0x48)
			g_LdrInitializeThunk += 0xa;
	}

	//
	// Get the HookDll Location
	//

	const WCHAR *HookDll = L"\\CaptainHook.dll";

	WCHAR home[512];
	GetModuleFileName(NULL, home, 512);
	WCHAR* bs = wcsrchr(home, L'\\');
	if (bs)
		*bs = L'\0';
	//
	// prepare extra data
	//

	LDRCODE_EXTRA_DATA *extra = (LDRCODE_EXTRA_DATA*)HeapAlloc(GetProcessHeap(), 0, 8192);
	if (!extra)
		return 0x40;

	/*struct {
		struct LDRCODE_EXTRA_DATA {
			// ...
		}			extra_data;

		char LdrLoadDll_str[16] = "LdrLoadDll";
		char LdrGetProcAddr_str[28] = "LdrGetProcedureAddress";
		char NtRaiseHardError_str[20] = "NtRaiseHardError";
		char RtlFindActCtx_str[44] = "RtlFindActivationContextSectionString";

		wchar_t KernelDll_str[13] = L"kernel32.dll";

		wchar_t NativeHookDll_str[] = L"...\\CaptainHook.dll";
		wchar_t Arm64ecHookDll_str[] = L"...\\64\\CaptainHook.dll"; // ARM64EC
		wchar_t Wow64HookDll_str[] = L"...\\32\\CaptainHook.dll";

		char HookFunc_str[12] = "InstalHooks";

		struct INJECT_DATA {
			// ...
		}			InjectData;
	}*/
	
	ULONG len;
	ULONG_PTR extra_ptr = ((ULONG_PTR)extra + sizeof(LDRCODE_EXTRA_DATA));

	//
	// add required function name strings
	//
	
#define ADD_C_STRING(var, str) \
	strcpy((char *)extra_ptr, str); \
	extra->var##_Offset = (ULONG)(extra_ptr - (ULONG_PTR)extra); \
	len = (ULONG)strlen(str); \
	extra_ptr += len + 1;

	ADD_C_STRING(LdrLoadDll, "LdrLoadDll");
	ADD_C_STRING(LdrGetProcAddr, "LdrGetProcedureAddress");
	ADD_C_STRING(NtProtectVirtualMemory, "NtProtectVirtualMemory");
	ADD_C_STRING(NtRaiseHardError, "NtRaiseHardError");
	ADD_C_STRING(NtDeviceIoControlFile, "NtDeviceIoControlFile");

	ADD_C_STRING(RtlFindActCtx, "RtlFindActivationContextSectionString");

	ADD_C_STRING(RtlImageOptionsEx, "LdrQueryImageFileExecutionOptionsEx");

	//
	// add required dll unicode paths
	//

#define ADD_UNICODE_STRING(var) \
	len = (ULONG)wcslen((WCHAR*)extra_ptr); \
	extra->var##_Offset = (ULONG)(extra_ptr - (ULONG_PTR)extra); \
	extra->var##_Length = len * sizeof(WCHAR); \
	extra_ptr += sizeof(WCHAR) * (len + 1);

	wcscpy((WCHAR*)extra_ptr, L"kernel32.dll");
	ADD_UNICODE_STRING(KernelDll);
	
	wcscpy((WCHAR*)extra_ptr, home);
	if (g_IsOnArm64)
		wcscat((WCHAR*)extra_ptr, L"\\ARM64");
	else
		wcscat((WCHAR*)extra_ptr, L"\\x64");
		//wcscat((WCHAR*)extra_ptr, L"\\AMD64");
	wcscat((WCHAR*)extra_ptr, HookDll);
	ADD_UNICODE_STRING(NativeHookDll);

	wcscpy((WCHAR*)extra_ptr, home);
	wcscat((WCHAR*)extra_ptr, L"\\ARM64EC");
	wcscat((WCHAR*)extra_ptr, HookDll);
	ADD_UNICODE_STRING(Arm64ecHookDll);

	wcscpy((WCHAR*)extra_ptr, home);
	wcscat((WCHAR*)extra_ptr, L"\\Win32");
	//wcscat((WCHAR*)extra_ptr, L"\\i386");
	wcscat((WCHAR*)extra_ptr, HookDll);
	ADD_UNICODE_STRING(Wow64HookDll);

#define ADD_ANSI_STRING(var) \
	len = (ULONG)strlen((char*)extra_ptr); \
	extra->var##_Offset = (ULONG)(extra_ptr - (ULONG_PTR)extra); \
	extra->var##_Length = len; \
	extra_ptr += (len + 1);

	strcpy((char*)extra_ptr, "InstallHooks");
	ADD_ANSI_STRING(HookFunc);


	//
	// set the offset of the work area INJECT_DATA
	//

	extra->InjectData_Offset = (ULONG)(extra_ptr - (ULONG_PTR)extra);

	//
	// set the initial value of the lock
	//

	extra->InitLock = 0;

	//
	// finish extra data
	//

	g_LdrCode_ExtraData = extra;
	g_LdrCode_ExtraData_Size = extra->InjectData_Offset + sizeof(INJECT_DATA);

	//
	// On ARM64 we requires x86 applications NOT to use the CHPE binaries.
	// 
	// So when ever the service starts it uses the global xtajit config to disable the use of CHPE binaries,
	// for x86 processes and restores the original value on service shutdown.
	//
	// See comment in HookImageOptionsEx core/low/init.c for more details.
	//

	if (g_DisableCHPE) 
	{
		HKEY hkey = NULL;
		LSTATUS rc = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Wow64\\x86\\xtajit",
			0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL);
		if (rc == 0) {
			DWORD value;
			DWORD size = sizeof(value);
			rc = RegQueryValueEx(hkey, L"LoadCHPEBinaries_old", NULL, NULL, (BYTE*)&value, &size);
			if (rc != 0) { // only save the old value when its not already saved
				rc = RegQueryValueEx(hkey, L"LoadCHPEBinaries", NULL, NULL, (BYTE*)&value, &size);
				if (rc == 0)
					RegSetValueEx(hkey, L"LoadCHPEBinaries_old", 0, REG_DWORD, (BYTE*)&value, size);
			}
			value = 0;
			RegSetValueEx(hkey, L"LoadCHPEBinaries", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
			RegCloseKey(hkey);
		}
	}

    return 0;
}

VOID UnInitInject()
{
	if (g_DisableCHPE) 
	{
		HKEY hkey = NULL;
		LSTATUS rc = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Wow64\\x86\\xtajit", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL);
		if (rc == 0) {
			DWORD value;
			DWORD size = sizeof(value);
			rc = RegQueryValueEx(hkey, L"LoadCHPEBinaries_old", NULL, NULL, (BYTE*)&value, &size);
			if (rc == 0) {
				RegSetValueEx(hkey, L"LoadCHPEBinaries", 0, REG_DWORD, (BYTE*)&value, size);
				RegDeleteValue(hkey, L"LoadCHPEBinaries_old");
			} else
				RegDeleteValue(hkey, L"LoadCHPEBinaries");
			RegCloseKey(hkey);
		}
	}
}

ULONG InjectLdr(HANDLE hProcess, ULONG uFlags)
{
	SIZE_T ret = 0;
	BOOL ok = FALSE;
	ULONG OldProtect;

	LDRCODE_DATA LdrData;
	memset(&LdrData, 0, sizeof(LdrData));
	LdrData.Flags.uFlags = uFlags;

	if (g_IsOnArm64)
	{
		USHORT ProcessMachine = 0xFFFF;
		//if (OsBuild >= 22000) { // Win 11
		PROCESS_MACHINE_INFORMATION info;
		if (GetProcessInformation(hProcess, (PROCESS_INFORMATION_CLASS)ProcessMachineTypeInfo, &info, sizeof(info)))
			ProcessMachine = info.ProcessMachine;
		//} else {  // Win 10 on arm - not supoorted
		//	IsWow64Process2(hProcess, &ProcessMachine, NULL);
		//}

		if (ProcessMachine == 0xFFFF)
			return 0x50;

		//
		// Currently 32-bit ARM processes are not supported,
		// there doesn't seam to be a significant amount of these out there anyways
		//

		if (ProcessMachine == IMAGE_FILE_MACHINE_ARMNT) {
			LdrData.Flags.IsWoW64 = 1;
			return 0x51; // not supported
		}
		else if (ProcessMachine == IMAGE_FILE_MACHINE_AMD64) {
			LdrData.Flags.IsArm64ec = 1;
			LdrData.Flags.IsXtAjit = 1;
		}
		else if (ProcessMachine == IMAGE_FILE_MACHINE_I386) {
			LdrData.Flags.IsWoW64 = 1;
			LdrData.Flags.IsXtAjit = 1;
		}
	}
	else
	{
		BOOL isTargetWow64Process = FALSE;
		IsWow64Process(hProcess, &isTargetWow64Process);
		LdrData.Flags.IsWoW64 = isTargetWow64Process;
	}

	//
	// check if we have initialized all needed data successfully
	//

	if ((!g_LdrCode_Ptr) || (!g_LdrCode_ExtraData) || (LdrData.Flags.IsArm64ec && ((!g_LdrInitializeThunkEC) || (!pVirtualAlloc2))))
		return 0x52;
	
	//
	// prepare the LdrData parameters
	//

	LdrData.NtDllBase = (ULONG64)(ULONG_PTR)GetModuleHandleW(L"ntdll.dll");

	if (LdrData.Flags.IsWoW64) {
		LdrData.NtDllWoW64Base = FindDllBase64(hProcess, L"\\syswow64\\ntdll.dll");
		if (g_IsOnArm64 && !LdrData.NtDllWoW64Base) {
			//
			// We requires CHPE to be disabled for x86 applications
			// for simplicity and compatibility with forced processes we disable CHPE globally using the Wow64\x86\xtajit key
			// alternatively we can hook LdrQueryImageFileExecutionOptionsEx from LowLevel.dll
			// and return 0 for the L"LoadCHPEBinaries" option
			// 
			// HKLM\SOFTWARE\Microsoft\Wow64\x86\xtajit -> LoadCHPEBinaries = 0
			// HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{image_name} -> LoadCHPEBinaries = 0
			// HKCU\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers -> {full_image_path} = "~ ARM64CHPEDISABLED"
			//  

			//LdrData.ntdll_wow64_base = FindDllBase64(hProcess, L"\\SyChpe32\\ntdll.dll");
			//if (LdrData.ntdll_wow64_base) 
			//	LdrData.flags.is_chpe32 = 1;
			//else {
				return 0x53; // chpe32
			//}
		}
	}

	LdrData.RealNtDeviceIoControlFile = (ULONG64)LdrData.NtDllBase + FindDllExportFromFile(g_NtDllPath, "NtDeviceIoControlFile");
	LdrData.NativeNtProtectVirtualMemory = (ULONG64)LdrData.NtDllBase + FindDllExportFromFile(g_NtDllPath, "NtProtectVirtualMemory");
	LdrData.NativeNtRaiseHardError = (ULONG64)LdrData.NtDllBase + FindDllExportFromFile(g_NtDllPath, "NtRaiseHardError");
	
	//
	// Copy the Native LdrCode into the target process
	//

	void* pLdrCodeAddr = InjectLdr_CopyCode(hProcess, g_LdrCode_Size, g_LdrCode_Ptr, (BOOLEAN)LdrData.Flags.IsArm64ec);
	if (!pLdrCodeAddr)
		return 0x54;

	LdrData.NativeDetour = (ULONG64)((UCHAR*)pLdrCodeAddr + g_LdrCode_Detour_Offset);
	
	//
	// Determine detour jump type
	//

	int LongDiff;
	if (g_IsOnArm64)
		LongDiff = 2; // on ARM64 always 64-bit absolute jump
	else
	{
		LongDiff = 1; // default 32-bit absolute jump
		if (llabs((long long)g_LdrInitializeThunk - (long long)pLdrCodeAddr) < 0x80000000LL)
			LongDiff = 0; // withing a 32-bit realtive jump reach
		else if (((ULONG_PTR)pLdrCodeAddr & 0xffffffff00000000) != 0)
			LongDiff = 2; // 64-bit absolute jump
	}

	//
	// Backup LdrInitializeThunk code for the trampoline
	//

	void* pLdrInitializeThunk = (void*)g_LdrInitializeThunk;
	if (LdrData.Flags.IsArm64ec) 
		pLdrInitializeThunk = (void*)g_LdrInitializeThunkEC;
	ok = ReadProcessMemory(hProcess, pLdrInitializeThunk, LdrData.LdrInitializeThunk_Tramp, sizeof(LdrData.LdrInitializeThunk_Tramp),&ret);
	if (!ok || sizeof(LdrData.LdrInitializeThunk_Tramp) != ret)
		return 0x55;
 
	if (LdrData.Flags.IsWoW64) 
	{
		//
		// If this is a 32 bit process running under WoW64, we need to inject also some 32 bit code
		//

		void *pLdrCode32Addr = InjectLdr_CopyCode(hProcess, g_LdrCode32_Size, g_LdrCode32_Ptr, FALSE);
		if (!pLdrCode32Addr)
			return 0x56;

		//
		// set LdrCode32 protection
		//

		ok = VirtualProtectEx(hProcess, pLdrCode32Addr, g_LdrCode32_Size, PAGE_EXECUTE_READ, &OldProtect);
		if (!ok)
			return 0x57;

		LdrData.WoW64Detour = (ULONG64)((UCHAR*)pLdrCode32Addr + g_LdrCode32_Detour_Offset);
	}

	memcpy(LdrData.NtDelayExecution_code, g_NtDllSavedCode, (NATIVE_FUNCTION_SIZE * NATIVE_FUNCTION_COUNT));
	// LdrData.NtDelayExecution_code
	// LdrData.NtDeviceIoControlFile_code
	// LdrData.NtFlushInstructionCache_code
	// LdrData.NtProtectVirtualMemory_code

	//
	// build trampoline to LdrInitializeThunk
	//

	ULONG_PTR TrampAddr = (ULONG_PTR)pLdrCodeAddr + g_LdrCode_Data_Offset + FIELD_OFFSET(LDRCODE_DATA, LdrInitializeThunk_Tramp);
	if (!InjectLdr_BuildTramp(LdrData.LdrInitializeThunk_Tramp, TrampAddr, LongDiff, (BOOLEAN)LdrData.Flags.IsArm64ec))
		return 0x58;

	//
	// copy the ExtraData  into the target process
	//

	void *pExtraDataAddress = InjectLdr_AllocMemory(hProcess, g_LdrCode_ExtraData_Size, FALSE, FALSE);
	if (!pExtraDataAddress)
		return 0x59;
	
	ok = WriteProcessMemory(hProcess, pExtraDataAddress, g_LdrCode_ExtraData, g_LdrCode_ExtraData_Size, &ret);
	if (!ok || g_LdrCode_ExtraData_Size != ret)
		return 0x5A;
	LdrData.ExtraData = (ULONG64)(ULONG_PTR)pExtraDataAddress;	

	//
	// update LdrData inside the target process
	//

	void* pLdrDataAddr = (void *)((ULONG_PTR)pLdrCodeAddr + g_LdrCode_Data_Offset);
	ok = WriteProcessMemory(hProcess, pLdrDataAddr, &LdrData, sizeof(LDRCODE_DATA), &ret);
	if(!ok || sizeof(LDRCODE_DATA) != ret)
		return 0x5B;

	//
	// set LdrCode protection
	//

	ok = VirtualProtectEx(hProcess, pLdrCodeAddr, g_LdrCode_Size, PAGE_EXECUTE_READ, &OldProtect);
	if (!ok)
		return 0x5C;

	//
	// overwrite the begin of LdrInitializeThunk with the detour to ldrcode entry
	// 

	if (!InjectLdr_WriteJump(hProcess, (ULONG_PTR)((UCHAR*)pLdrCodeAddr + g_LdrCode_Start_Offset), LongDiff, (BOOLEAN)LdrData.Flags.IsArm64ec))
		return 0x5D;

	return 0;
}

void* InjectLdr_AllocMemory(HANDLE hProcess, SIZE_T uSize, BOOLEAN bExecutable, BOOLEAN UseArm64ec) 
{
	SIZE_T uAllocSize = uSize;
	void* pAllocAddr = NULL;

	if (UseArm64ec && bExecutable) {

		MEM_EXTENDED_PARAMETER Parameter = { 0 };
		Parameter.Type = MemExtendedParameterAttributeFlags;
		Parameter.ULong64 = MEM_EXTENDED_PARAMETER_EC_CODE;

		for (UINT_PTR BaseAddr = 0x10000; !pAllocAddr; BaseAddr <<= 1) 
		{
			pAllocAddr = pVirtualAlloc2(hProcess, (void*)BaseAddr, uAllocSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE, &Parameter, 1);
		}
	} 
	else
	{
		//
		// Allocate virtual memory within the process. 
		// To ensure the address falls within the low 24 bits of the address space, 
		// NtAllocateVirtualMemory must be used with ZeroBits set to 8 (32 - 8 = 24).
		//

		//for (int i = 8; !pAllocAddr && i > 2; i--) {
		for (int i = 8; !pAllocAddr && i >= 0; i--) 
		{
			if (!NT_SUCCESS(NtAllocateVirtualMemory(hProcess, &pAllocAddr, i, &uAllocSize, MEM_COMMIT | MEM_RESERVE, bExecutable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE))) {
				pAllocAddr = NULL;
				uAllocSize = uSize;
			}
		}
	}
	return pAllocAddr;
}

void *InjectLdr_CopyCode(HANDLE hProcess, SIZE_T uCodeSize, const void* pCode, BOOLEAN UseArm64ec) 
{
	void* pAllocAddr = InjectLdr_AllocMemory(hProcess, uCodeSize, TRUE, UseArm64ec);
	if (!pAllocAddr) 
		return NULL;
	
	SIZE_T ret = 0;
	BOOL ok = WriteProcessMemory(hProcess, pAllocAddr, pCode, uCodeSize, &ret);
	if (!ok || uCodeSize != ret)
		return NULL;

	return pAllocAddr;
}

BOOLEAN InjectLdr_BuildTramp(UCHAR* pTrampCode, ULONG_PTR TrampAddr, int LongDiff, BOOLEAN UseArm64ec) 
{
	//
	// code contains already a copy of LdrInitializeThunk (48 bytes)
	// here we need to add the jump to the original at the right location
	//

	if (g_IsOnArm64)
	{
		ULONG uToSave = 16; // the length of the jump to detour code // = 20 with debug break

		ULONG* aCode = (ULONG*)(pTrampCode + uToSave);

		//
		// append a jump to the original code
		// on ARM64 its simple as each instruction is always 4 byte long
		//

		*aCode++ = 0x58000048;	// ldr x8, 8
		*aCode++ = 0xD61F0100;	// br x8
		*(DWORD64*)aCode = (UseArm64ec ? g_LdrInitializeThunkEC : g_LdrInitializeThunk) + uToSave;
	}
	else
	{

		//
		// Determin the length of the overwritten block,
		// keep in sync with InjectLdr_WriteJump !!!
		//

		ULONG uToSave;
		if (LongDiff == 0) uToSave = 6;
		else if (LongDiff == 1) uToSave = 7;
		else  uToSave = 12;

		//
		// traverse the copied code and find the right position where to insert the jump
		// 
		// TODO: replace this with a proper dissaselbler from the detours library or soemthing...
		//

		ULONG uOffset = 0;

#define IS_1BYTE(a)     (                 pTrampCode[uOffset + 0] == (a))
#define IS_2BYTE(a,b)   (IS_1BYTE(a)   && pTrampCode[uOffset + 1] == (b))
#define IS_3BYTE(a,b,c) (IS_2BYTE(a,b) && pTrampCode[uOffset + 2] == (c))

		while (uOffset < uToSave) {

			ULONG uInstLen = 0;

			// nop
			if (IS_1BYTE(0x90))
				uInstLen = 1;

			// push ebp
			else if (IS_1BYTE(0x55))
				uInstLen = 1;

			// mov ebp, esp
			else if (IS_2BYTE(0x8B, 0xEC))
				uInstLen = 2;

			// mov edi, edi
			else if (IS_2BYTE(0x8B, 0xFF))
				uInstLen = 2;

			// push ebx
			else if (IS_2BYTE(0xFF, 0xF3))
				uInstLen = 2;

			// push rbx (Windows 8.1)
			else if (IS_2BYTE(0x40, 0x53))
				uInstLen = 2;

			// mov dword ptr [esp+imm8],eax
			else if (IS_3BYTE(0x89, 0x44, 0x24))
				uInstLen = 4;

			// lea eax, esp+imm8
			else if (IS_3BYTE(0x8D, 0x44, 0x24))
				uInstLen = 4;

			// sub rsp, imm8
			else if (IS_3BYTE(0x48, 0x83, 0xEC))
				uInstLen = 4;

			// mov rbx, rcx
			else if (IS_3BYTE(0x48, 0x8B, 0xD9))
				uInstLen = 3;

			// mov rax,qword ptr [???] 
			//else if (IS_3BYTE(0x48, 0x8B, 0x04))
			//	uInstLen = 4;

			if (!uInstLen)
				return FALSE;

			uOffset += uInstLen;
		}

#undef IS_3BYTE
#undef IS_2BYTE
#undef IS_1BYTE

		// append a jump to the original code

		union {
			UCHAR* pB;
			WORD* pW;
			DWORD* pD;
			UINT64* pQ;
		} xCode;

		xCode.pB = &pTrampCode[uOffset];

		if (LongDiff == 0)
		{
			*xCode.pW++ = 0xE948;	// jmp ; relative
			*xCode.pD++ = (ULONG)(g_LdrInitializeThunk + uOffset - (TrampAddr + uOffset + 6)); // jump offset
		}
		else // 64 bit absolute jump without touching data registers
		{
			*xCode.pW++ = 0x25FF;	// jmp qword ptr[rip+0]
			*xCode.pD++ = 0;			// 0
			*xCode.pQ++ = g_LdrInitializeThunk + uOffset; // jump target
		}
	}

	return TRUE;
}

BOOLEAN InjectLdr_WriteJump(HANDLE hProcess, ULONG_PTR Detour, int LongDiff, BOOLEAN UseArm64ec)
{
	UCHAR JumpCode[20];
	SIZE_T CodeSize = 0;
	UCHAR* Func = (UCHAR *)((ULONG_PTR)g_LdrInitializeThunk);

	//
	// prepare a jump code to the injected ldrcode entry
	//

	if (g_IsOnArm64)
	{

		if (UseArm64ec)
			Func = (UCHAR*)((ULONG_PTR)g_LdrInitializeThunkEC);

		ULONG* aCode = (ULONG*)JumpCode;
		//*aCode++ = 0xD43E0000;	// brk #0xF000
		*aCode++ = 0x58000048;	// ldr x8, 8
		*aCode++ = 0xD61F0100;	// br x8
		*(DWORD64*)aCode = Detour; aCode += 2;

		CodeSize = (UCHAR*)aCode - JumpCode;
	}
	else
	{

		union {
			UCHAR* pB;
			WORD* pW;
			DWORD* pD;
			UINT64* pQ;
		} xCode;

		xCode.pB = JumpCode;

		if (LongDiff == 0) // this no longer happens as ntdll usually resides past the 4gb boundary while we inject lrdcode in the lower 4 GB
		{
			*xCode.pW++ = 0xE948; // jmp ; relative
			*xCode.pD++ = (ULONG)(Detour - (g_LdrInitializeThunk + 6));
		} // 6
		else if (LongDiff == 1)
		{
			*xCode.pB++ = 0xB8; // mov rax, addr
			*xCode.pD++ = (ULONG)Detour;

			*xCode.pW++ = 0xE0FF; // jmp rax
		} // 7
		else // if (LongDiff == 2)
		{
			*xCode.pW++ = 0xb848; // movabs rax, addr
			*xCode.pQ++ = Detour;

			*xCode.pW++ = 0xE0FF; // jmp rax
		} // 12

		CodeSize = (UCHAR*)xCode.pB - JumpCode;
	}

	//
	// Override LdrInitializeThunk prolog with the detour code
	//

	ULONG OldProtect;
	BOOL ok = VirtualProtectEx(hProcess, Func, CodeSize, PAGE_READWRITE, &OldProtect);
	if (ok) {
		SIZE_T ret = 0;
		ok = WriteProcessMemory(hProcess, Func, JumpCode, CodeSize, &ret);
		if (ok && CodeSize == ret) {
			ok = VirtualProtectEx(hProcess, Func, CodeSize, OldProtect, &OldProtect);
			if (ok)
				return TRUE;
		}
	}
	return FALSE;
}
