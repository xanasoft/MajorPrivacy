/*
 * Copyright 2020-2024 David Xanatos, xanasoft.com
 */

#include <phnt_windows.h>
#include <phnt.h>

#include "ldrdata.h"

#ifdef _WIN64
 // Pointer to 64-bit ProcessHeap is at offset 0x0030 of 64-bit PEB
 #ifdef _M_ARM64
  #define GET_ADDR_OF_PEB (*((ULONG_PTR*)(__getReg(18) + 0x60)))
 #else
  #define GET_ADDR_OF_PEB __readgsqword(0x60)
 #endif
#else ! _WIN64
 // Pointer to 32-bit ProcessHeap is at offset 0x0018 of 32-bit PEB
 #define GET_ADDR_OF_PEB __readfsdword(0x30)
#endif _WIN64

typedef NTSTATUS (*P_LdrLoadDll)(WCHAR *PathString, ULONG *DllFlags, UNICODE_STRING *ModuleName, HANDLE *ModuleHandle);
typedef NTSTATUS (*P_LdrGetProcedureAddress)(HANDLE ModuleHandle, ANSI_STRING *ProcName, ULONG ProcNum, ULONG_PTR *Address);
typedef NTSTATUS (*P_NtProtectVirtualMemory)(HANDLE ProcessHandle, PVOID *BaseAddress, PSIZE_T RegionSize, ULONG NewProtect, PULONG OldProtect);
typedef NTSTATUS (*P_NtRaiseHardError)(NTSTATUS ErrorStatus, ULONG NumberOfParameters, ULONG UnicodeBitMask, ULONG_PTR *Parameters, ULONG ErrorOption, ULONG *ErrorReturn);
typedef NTSTATUS (*P_NtDelayExecution)(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);
typedef NTSTATUS (*P_NtFlushInstructionCache)(HANDLE ProcessHandle, PVOID BaseAddress, ULONG Length);

#define NT_CALL(x) ((P_##x)&pLdrData->x##_code)

#ifndef SECONDS
#define SECONDS(n64)            (((LONGLONG)n64) * 10000000L)
#endif

int InitInject(LDRCODE_DATA* pLdrCode);

UCHAR *FindDllExport(void* DllBase, const UCHAR* pProcName);

#ifdef _M_ARM64
void* Hook_GetFFSTarget(UCHAR* pSourceFunc);

extern LDRCODE_DATA LdrCodeData;
//extern void* DetourCodeARM64;

ULONG_PTR EntrypointC();
#else
ULONG_PTR EntrypointC(LDRCODE_DATA* pLdrData);
#endif

NTSTATUS DebugReportError(LDRCODE_DATA* pLdrData, ULONG uError)
{
    // Note: A normal string like L"text" would not result in position independent code !!!
    // hence we create a string array and fill it byte by byte
    wchar_t Text[] = { 'L','d','r','C','o','d','e',' ','E','r','r','o','r',':',' ','0','x',0,0,0,0,0,0,0,0,0,0};

    // convert ulong to hex string and copy it into the message array
    wchar_t* Ptr = &Text[17]; // point after L"...0x"
    wchar_t Table[] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
    for(int i=28; i >= 0; i-=4)
        *Ptr++ = Table[(uError >> i) & 0xF];

    return 0; // todo
}

#if 0
void WaitForDebugger(LDRCODE_DATA* pLdrData)
{
    volatile UCHAR *pIsDebuggerPresent = (UCHAR *)(GET_ADDR_OF_PEB + 2);

    const P_NtDelayExecution NtDelayExecution = (P_NtDelayExecution) &pLdrData->NtDelayExecution_code;

    LARGE_INTEGER Delay;
    Delay.QuadPart = -SECONDS(3) / 100;

    while (! (*pIsDebuggerPresent))
        NtDelayExecution(FALSE, &Delay);

    __debugbreak();
}
#endif

void WriteMemorySafe(LDRCODE_DATA* pLdrData, void* pAddress, SIZE_T uSize, void* pData)
{
    void *pRegionBase = pAddress;
    SIZE_T uRegionSize = uSize;
    ULONG OldProtect;

    NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);

    // memcopy is not available, lets do our own
    switch (uSize) {
    case 1: *(UCHAR*)pAddress = *(UCHAR*)pData;       break;
    case 2: *(USHORT*)pAddress = *(USHORT*)pData;     break;
    case 4: *(ULONG*)pAddress = *(ULONG*)pData;       break;
    case 8: *(ULONG64*)pAddress = *(ULONG64*)pData;   break;
    default:
        for (SIZE_T i = 0; i < uSize; i++)
            ((UCHAR*)pAddress)[i] = ((UCHAR*)pData)[i];
    }

    NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, OldProtect, &OldProtect);
}

#ifdef _M_ARM64
NTSTATUS MyImageOptionsEx(PUNICODE_STRING pSubKey, PCWSTR pValueName, 
    ULONG uType, PVOID pBuffer, ULONG uBufferSize, PULONG pReturnedLength, BOOLEAN bWow64, LDRCODE_DATA* pLdrData)
{
    // Note: A normal string like L"LoadCHPEBinaries" would not result in position independent code !!!
    wchar_t LoadCHPEBinaries[] = { 'L','o','a','d','C','H','P','E','B','i','n','a','r','i','e','s',0 }; 
    PCWSTR Ptr = pValueName;
    for (PCWSTR Tmp = LoadCHPEBinaries; *Ptr && *Tmp && *Ptr == *Tmp; Ptr++, Tmp++);
	if (*Ptr == L'\0'){ //if (_wcsicmp(ValueName, L"LoadCHPEBinaries") == 0)
		*(ULONG*)pBuffer = 0;
		return 0; // STATUS_SUCCESS
	}
    //return 0xC0000034; // STATUS_OBJECT_NAME_NOT_FOUND

    typedef (*P_ImageOptionsEx)(PUNICODE_STRING, PCWSTR, ULONG, PVOID, ULONG, PULONG, BOOLEAN);
    return ((P_ImageOptionsEx)pLdrData->RtlImageOptionsEx_Tramp)(pSubKey, pValueName, uType, pBuffer, uBufferSize, pReturnedLength, bWow64);
}

void DisableCHPE(LDRCODE_DATA* pLdrData)
{
    //
    // Sandboxie on ARM64 requires x86 applications NOT to use the CHPE (Compiled Hybrid Portable Executable)
    // binaries as when hooking a hybrid binary it is required to hook the internal native functions.
    // 
    // This can be done quite easily for ARM64EC (x64 on ARM64) by compiling HookDll.dll as ARM64EC
    // and resolving the FFS sequence targets, which then can be hooked with the native HookDll.dll functions.
    // 
    // For CHPE MSFT how ever does not provide any public build tool chain, hence it would be required
    // to hand craft native detour targets and then properly transition to x86 code which is not documented.
    // When the use of hybrid binaries for x86 is disabled all loaded DLL's, except the native ntdll.dll
    // are pure x86 binaries and can be easily hooked with HookDll.dll's x86 functions.
    // 
    // To prevent the kernel from loading the CHPE version of ntdll.dll we can pass PsAttributeChpe = 0
    // in the AttributeList of NtCreateUserProcess, however then the created process will still try to 
    // load the rest of the system dll's from the SyChpe32 directory and fail to initialize.
    // There for we have to hook LdrQueryImageFileExecutionOptionsEx and simulate the LoadCHPEBinaries = 0
    // in its "Image File Execution Options" key this way the process will continue with loading
    // the regular x86 binaries from the SysWOW64 directory and initialize properly.
    // 
    // Note: This hook affects only the native function and is only installed on x86 processes
    //          hence we install a similar hook using the HookDll.dll which causes 
    //          CreateProcessInternalW to set PsAttributeChpe = 0 when creating new processes.
    //

    LDRCODE_EXTRA_DATA* pExtraData = (LDRCODE_EXTRA_DATA *) (pLdrData->ExtraData);

    void* RtlImageOptionsEx = FindDllExport((void*)pLdrData->NtDllBase, (UCHAR*)pExtraData + pExtraData->RtlImageOptionsEx_Offset);
    if (!RtlImageOptionsEx)
        return;

    void *pRegionBase;
    SIZE_T uRegionSize;
    ULONG OldProtect;
    ULONG* aCode;

    //
    // backup target & create simple trampoline
    //

    pRegionBase = (void*)pLdrData->RtlImageOptionsEx_Tramp;
    uRegionSize = sizeof(pLdrData->RtlImageOptionsEx_Tramp);
    NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);

    ULONG DetourSize = 28;
    memcpy(pLdrData->RtlImageOptionsEx_Tramp, RtlImageOptionsEx, DetourSize); 

    aCode = (ULONG*)(pLdrData->RtlImageOptionsEx_Tramp + DetourSize); // 28
	aCode[0] = 0x58000048;	// ldr x8, 8 - Rest of RtlImageOptionsEx
	aCode[1] = 0xD61F0100;	// br x8
	*(DWORD64*)&aCode[2] = (DWORD64)RtlImageOptionsEx + DetourSize; 
    // 44

    NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, OldProtect, &OldProtect);

    NT_CALL(NtFlushInstructionCache)(NtCurrentProcess(), pRegionBase, (ULONG)uRegionSize);

    //
    // make target writable & create detour
    //

    pRegionBase = (void*)RtlImageOptionsEx;
    uRegionSize = DetourSize;
    NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);

	aCode = (ULONG*)RtlImageOptionsEx;
    aCode[0] = 0x580000a7;	// ldr x7, 20 - data
	aCode[1] = 0x58000048;	// ldr x8, 8 - MyImageOptionsEx
	aCode[2] = 0xD61F0100;	// br x8
	*(DWORD64*)&aCode[3] = (DWORD64)MyImageOptionsEx; 
    *(DWORD64*)&aCode[5] = (DWORD64)pLdrData;
    //28

    NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, OldProtect, &OldProtect);

    NT_CALL(NtFlushInstructionCache)(NtCurrentProcess(), pRegionBase, (ULONG)uRegionSize);
}
#endif

#ifdef _M_ARM64
ULONG_PTR EntrypointC() {
    LDRCODE_DATA* pLdrData = &LdrCodeData;
#else
ULONG_PTR EntrypointC(LDRCODE_DATA* pLdrData) {
#endif

    //
    // We use a field in our data structure to synchronize the initialization,
    // the first thread will do the setup the other threads will wait.
    //

    if (!pLdrData->InitDone)
    {
        LDRCODE_EXTRA_DATA* pExtraData = (LDRCODE_EXTRA_DATA*)pLdrData->ExtraData;

        volatile ULONG_PTR* Init_Lock = &pExtraData->InitLock;

#ifdef _WIN64
        ULONG_PTR OldInit_Lock = _InterlockedCompareExchange64(Init_Lock, -1, 0);
#else ! _WIN64
        ULONG_PTR OldInit_Lock = _InterlockedCompareExchange(Init_Lock, -1, 0);
#endif _WIN64

        if (OldInit_Lock == 0) {

            int err = InitInject(pLdrData);
            if (err)
            {
                NTSTATUS status = 0xC000007AL; // = STATUS_PROCEDURE_NOT_FOUND
                ULONG ErrorReturn;
                ((P_NtRaiseHardError)pLdrData->NativeNtRaiseHardError)(
                    status | 0x10000000, // | FORCE_ERROR_MESSAGE_BOX
                    0, 0, NULL, 1, &ErrorReturn);
            }

#ifdef _WIN64
            if (pLdrData->Flags.IsWoW64) 
            {
#ifdef _M_ARM64
                if (!pLdrData->Flags.IsChpe32)
                    DisableCHPE(pLdrData);
#endif
            }
#endif

            UCHAR InitDone = 1;
            WriteMemorySafe(pLdrData, &pLdrData->InitDone, sizeof(UCHAR), &InitDone);

#ifdef _WIN64
            _InterlockedExchange64(Init_Lock, 1);
#else ! _WIN64
            _InterlockedExchange(Init_Lock, 1);
#endif _WIN64

        }
        else if (OldInit_Lock == -1) 
        {
            LARGE_INTEGER delay;
            delay.QuadPart = -SECONDS(3) / 100;

            const P_NtDelayExecution NtDelayExecution = (P_NtDelayExecution)&pLdrData->NtDelayExecution_code;

            while (*Init_Lock == -1)    
                NtDelayExecution(FALSE, &delay);
        }
    }

    return (ULONG_PTR)&pLdrData->LdrInitializeThunk_Tramp;
}

// Inject

ULONG my_strlen(const UCHAR* str)
{
    ULONG i = 0;
    for (; str[i]; i++);
    return i;
}

ULONG my_strcmp(const UCHAR* strL, const UCHAR* strR)
{
	ULONG i = 0;
	for (; strL[i] && strL[i] == strR[i]; i++);
	return strL[i] - strR[i];
}

UCHAR *FindDllExport2(void *DllBase, IMAGE_DATA_DIRECTORY *dir0, const UCHAR* pProcName)
{
    void *proc = NULL;
    if (dir0->VirtualAddress && dir0->Size) 
    {
        IMAGE_EXPORT_DIRECTORY *exports = (IMAGE_EXPORT_DIRECTORY *)((UCHAR *)DllBase + dir0->VirtualAddress);
        ULONG *names = (ULONG *)((UCHAR *)DllBase + exports->AddressOfNames);

        for (ULONG i = 0; i < exports->NumberOfNames; i++) 
        {
            UCHAR *name = (UCHAR *)DllBase + names[i];
            if (my_strcmp(name, pProcName) == 0) 
            {
                USHORT *ordinals = (USHORT *)((UCHAR *)DllBase + exports->AddressOfNameOrdinals);
                if (ordinals[i] < exports->NumberOfFunctions) 
                {
                    ULONG *functions = (ULONG *)((UCHAR *)DllBase + exports->AddressOfFunctions);
                    proc = (UCHAR *)DllBase + functions[ordinals[i]];
                    break;
                }
            }
        }

        if (proc && (ULONG_PTR)proc >= (ULONG_PTR)exports && (ULONG_PTR)proc < (ULONG_PTR)exports + dir0->Size) 
            proc = NULL;
    }
    return proc;
}

UCHAR *FindDllExport(void *DllBase, const UCHAR* pProcName)
{
    IMAGE_DOS_HEADER *dos_hdr = (void *)DllBase;
    if (dos_hdr->e_magic != 'MZ' && dos_hdr->e_magic != 'ZM')
        return NULL;
    
    IMAGE_NT_HEADERS *nt_hdrs = (IMAGE_NT_HEADERS *)((UCHAR *)dos_hdr + dos_hdr->e_lfanew);
    if (nt_hdrs->Signature != IMAGE_NT_SIGNATURE)
        return NULL;

    UCHAR* func_ptr = NULL;
    if (nt_hdrs->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) 
    {
        IMAGE_NT_HEADERS32 *nt_hdrs_32 = (IMAGE_NT_HEADERS32 *)nt_hdrs;
        IMAGE_OPTIONAL_HEADER32 *opt_hdr_32 = &nt_hdrs_32->OptionalHeader;

        if (opt_hdr_32->NumberOfRvaAndSizes) 
        {
            IMAGE_DATA_DIRECTORY *dir0 = &opt_hdr_32->DataDirectory[0];
            func_ptr = FindDllExport2(DllBase, dir0, pProcName);
        }
    }
#ifdef _WIN64
    else if (nt_hdrs->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) 
    {
        IMAGE_NT_HEADERS64 *nt_hdrs_64 = (IMAGE_NT_HEADERS64 *)nt_hdrs;
        IMAGE_OPTIONAL_HEADER64 *opt_hdr_64 = &nt_hdrs_64->OptionalHeader;

        if (opt_hdr_64->NumberOfRvaAndSizes) 
        {
            IMAGE_DATA_DIRECTORY *dir0 = &opt_hdr_64->DataDirectory[0];
            func_ptr = FindDllExport2(DllBase, dir0, pProcName);
        }
    }
#endif _WIN64
    return func_ptr;
}

int InitInject(LDRCODE_DATA* pLdrData)
{
    LDRCODE_EXTRA_DATA* pExtraData = (LDRCODE_EXTRA_DATA *) (pLdrData->ExtraData);
    INJECT_DATA* pInjectData = (INJECT_DATA *) ((UCHAR *)pExtraData + pExtraData->InjectData_Offset);
    pInjectData->LdrData = (ULONG64)pLdrData;

    void *NtDllBase;

    UCHAR *pHookTarget;
    UCHAR* pDetourCode;

    void *pRegionBase;
    SIZE_T uRegionSize;
    ULONG OldProtect;

#ifdef _WIN64
    //
    // in a 32-bit program on 64-bit Windows, we must hook the 32-bit ntdll (ntdll32)
    //

    if (pLdrData->Flags.IsWoW64)
        NtDllBase = (void *)pLdrData->NtDllWoW64Base;
    else
#endif _WIN64
        NtDllBase = (void *)pLdrData->NtDllBase;
    if(!NtDllBase)
		return 0x10;

    //
    // find the addresses of some required functions
    //

    pInjectData->LdrLoadDll = (ULONG_PTR)FindDllExport(NtDllBase, (UCHAR *)pExtraData + pExtraData->LdrLoadDll_Offset);
#ifdef _M_ARM64
    if (pInjectData->LdrLoadDll && pLdrData->Flags.IsArm64ec)
        pInjectData->LdrLoadDll = (ULONG_PTR)Hook_GetFFSTarget((UCHAR*)pInjectData->LdrLoadDll);
#endif
    if (!pInjectData->LdrLoadDll) 
        return 0x11;

    pInjectData->LdrGetProcAddr = (ULONG_PTR)FindDllExport(NtDllBase, (UCHAR *)pExtraData + pExtraData->LdrGetProcAddr_Offset);
#ifdef _M_ARM64
    if (pInjectData->LdrGetProcAddr && pLdrData->Flags.IsArm64ec)
        pInjectData->LdrGetProcAddr = (ULONG_PTR)Hook_GetFFSTarget((UCHAR*)pInjectData->LdrGetProcAddr);
#endif
    if (!pInjectData->LdrGetProcAddr)
        return 0x12;

#ifdef _WIN64
    if (pLdrData->Flags.IsWoW64)
    {
        pInjectData->NtProtectVirtualMemory = (ULONG_PTR)FindDllExport(NtDllBase, (UCHAR*)pExtraData + pExtraData->NtProtectVirtualMemory_Offset);
        if (!pInjectData->NtProtectVirtualMemory)
            return 0x13;

        pInjectData->NtRaiseHardError = (ULONG_PTR)FindDllExport(NtDllBase, (UCHAR*)pExtraData + pExtraData->NtRaiseHardError_Offset);
        if (!pInjectData->NtRaiseHardError)
            return 0x14;

        pInjectData->NtDeviceIoControlFile = (ULONG_PTR)FindDllExport(NtDllBase, (UCHAR*)pExtraData + pExtraData->NtDeviceIoControlFile_Offset);
        if (!pInjectData->NtDeviceIoControlFile)
            return 0x15;
    }
    else
#endif
    {
        //
        // for ARM64EC we need native functions, FindDllExport can manage FFS
        // however this does not work for syscalls, hence we use the native function directly
        //

        pInjectData->NtProtectVirtualMemory = pLdrData->NativeNtProtectVirtualMemory;
        pInjectData->NtRaiseHardError = pLdrData->NativeNtRaiseHardError;
        pInjectData->NtDeviceIoControlFile = pLdrData->NtDeviceIoControlFile;
    }

    //
    // prepare unicode strings
    //

#define SET_NT_STRING(pStr, pBuf, uLen) \
    pStr.Length = (USHORT)uLen; \
    pStr.MaxLen = (USHORT)(uLen + sizeof(WCHAR)); \
    pStr.Buf32 = (ULONG)pBuf; \
    pStr.Buf64 = (ULONG64)pBuf;

    SET_NT_STRING(pInjectData->KernelDll, (ULONG_PTR)pExtraData + pExtraData->KernelDll_Offset, pExtraData->KernelDll_Length);

    //
    // select the right version of the HookDll.dll
    //

#ifdef _M_ARM64
    if (pLdrData->Flags.IsArm64ec)
    {
        SET_NT_STRING(pInjectData->HookDll, (ULONG_PTR)pExtraData + pExtraData->Arm64ecHookDll_Offset, pExtraData->Arm64ecHookDll_Length);
    }
    else
#endif
#ifdef _WIN64
    if (pLdrData->Flags.IsWoW64) 
    {
        SET_NT_STRING(pInjectData->HookDll, (ULONG_PTR)pExtraData + pExtraData->Wow64HookDll_Offset, pExtraData->Wow64HookDll_Length);
    }
    else
#endif
    {
        SET_NT_STRING(pInjectData->HookDll, (ULONG_PTR)pExtraData + pExtraData->NativeHookDll_Offset, pExtraData->NativeHookDll_Length);
    }

    SET_NT_STRING(pInjectData->HookFunc, (ULONG_PTR)pExtraData + pExtraData->HookFunc_Offset, pExtraData->HookFunc_Length);

    //
    // modify the detour code to include a hard coded pointer to the inject data area.
    //

#ifdef _WIN64
    if (!pLdrData->Flags.IsWoW64) 
    {
        pDetourCode = (void*)pLdrData->NativeDetour;

        pRegionBase = (void*)(pDetourCode - 8);
        uRegionSize = sizeof(ULONG_PTR);
        NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);

		// set the pointer to the inject data area
        *(ULONG_PTR*)(pDetourCode - 8) = (ULONG_PTR)pInjectData;

        NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, OldProtect, &OldProtect);
    }
    else
    {
        pDetourCode = (void*)pLdrData->WoW64Detour;
#else // 32 bit
    {
        pDetourCode = (void*)pLdrData->NativeDetour;
#endif
        pRegionBase = (void*)(pDetourCode + 1);
        uRegionSize = sizeof(ULONG_PTR);
        NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);

        // set the pointer to the inject data area
        *(ULONG*)(pDetourCode + 1) = (ULONG)(ULONG_PTR)pInjectData;

        NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, OldProtect, &OldProtect);
    }

    //
    // Hook the target function
    //

    /*
#ifdef _M_ARM64
    //
    // when hooking on arm64, go for LdrLoadDll 
    // instead of RtlFindActivationContextSectionString
    // for ARM64 both work, but for ARM64EC hooking RtlFindActCtx fails
    //

    if (!pLdrData->Flags.IsWoW64)
        pHookTarget = (UCHAR*)pInjectData->LdrLoadDll;
    else
#endif
    {
        pHookTarget = FindDllExport(NtDllBase, (UCHAR *)pExtraData + pExtraData->RtlFindActCtx_Offset);
        if (!pHookTarget)
            return 0x16;
    }
    */
    pHookTarget = (UCHAR*)pInjectData->LdrLoadDll;
    pInjectData->HookTarget = (ULONG_PTR)pHookTarget;

#ifdef _WIN64
    if (!pLdrData->Flags.IsWoW64) 
    {
#ifdef _M_ARM64
        pRegionBase = (void*)&pHookTarget[0];
        uRegionSize = 16;
        NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, PAGE_EXECUTE_READWRITE, &pInjectData->HookTarget_Protect);

        // backup target
        memcpy(pInjectData->HookTarget_Bytes, pHookTarget, 16);

        // create detour
        ULONG* aCode = (ULONG*)pHookTarget;
        *aCode++ = 0x58000048;	// ldr x8, 8
        *aCode++ = 0xD61F0100;	// br x8
        *(DWORD64*)aCode = (DWORD64)pDetourCode;
        // 16

        NT_CALL(NtFlushInstructionCache)(NtCurrentProcess(), pRegionBase, (ULONG)uRegionSize);
#else
        pRegionBase = (void*)&pHookTarget[0];
        uRegionSize = 12;
        NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, PAGE_EXECUTE_READWRITE, &pInjectData->HookTarget_Protect);

        // backup target
        memcpy(pInjectData->HookTarget_Bytes, pHookTarget, 12);

        union {
            UCHAR* pB;
            WORD* pW;
            DWORD* pD;
            UINT64* pQ;
        } xCode;
        xCode.pB = pHookTarget;

        // create detour
        *xCode.pW++ = 0xB848; // movabs rax, pDetourCode
        *xCode.pQ++ = (UINT64)pDetourCode;
        *xCode.pW++ = 0xE0FF; // jmp rax
        // 16
#endif
    }
    else
#endif // 32 bit
    {
        pRegionBase = (void*)pHookTarget;
        uRegionSize = 5;

        NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, PAGE_EXECUTE_READWRITE, &pInjectData->HookTarget_Protect);

        // backup target
        memcpy(pInjectData->HookTarget_Bytes, pHookTarget, 5);

        union {
            UCHAR* pB;
            WORD* pW;
            DWORD* pD;
            UINT64* pQ;
        } xCode;
        xCode.pB = pHookTarget;

        // create detour
        *xCode.pB++ = 0xE9; // jmp
        *xCode.pD++ = (ULONG)(pDetourCode - (pHookTarget + 5));
        // 5
    }

    return 0;
}



VOID DetourFunc(INJECT_DATA* pInjectData)
{
    //
    // Note: this function is invoked from the detour code, hence when running in WoW64,
    // the used instance of this function will be from the 32 bit version,
    // in which case we are unable to use NT_CALL and need to have a
    // pointer to the appropriate 32 bit function
    // 
    // Furthermore, on ARM64 the LDRCODE_DATA will be allocated past the 4 GB boundary 
    // hence in 32 bit mode we can not access it, only INJECT_DATA is available
    //

    NTSTATUS status;

    UNICODE_STRING* pDllPath;
    HANDLE ModuleHandle;
    
    ANSI_STRING* pHookFunc;
    typedef VOID(*P_HookFunc)(INJECT_DATA* inject);
    P_HookFunc HookFunc;

    void* pRegionBase;
    SIZE_T uRegionSize;
    ULONG OldProtect;

#ifdef _WIN64
    LDRCODE_DATA* pLdrData = (LDRCODE_DATA*)pInjectData->LdrData;
#endif

    //
    // restore original function
    //

    pRegionBase = (void*)pInjectData->HookTarget;
#ifdef _WIN64
#ifdef _M_ARM64
    uRegionSize = 16;
    memcpy((void*)pInjectData->HookTarget, pInjectData->HookTarget_Bytes, 16);

    NT_CALL(NtFlushInstructionCache)(NtCurrentProcess(), (void*)pInjectData->HookTarget, 16);
#else
    uRegionSize = 12;
    memcpy((void*)pInjectData->HookTarget, pInjectData->HookTarget_Bytes, 12);
#endif

    NT_CALL(NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, pInjectData->HookTarget_Protect, &OldProtect);
#else
    uRegionSize = 5;
    memcpy((void*)pInjectData->HookTarget, pInjectData->HookTarget_Bytes, 5);

    ((P_NtProtectVirtualMemory)pInjectData->NtProtectVirtualMemory)(NtCurrentProcess(), &pRegionBase, &uRegionSize, pInjectData->HookTarget_Protect, &OldProtect);
#endif

    //
    // load kernel32.dll
    //

    pDllPath = (UNICODE_STRING*)&pInjectData->KernelDll;

    status = ((P_LdrLoadDll)pInjectData->LdrLoadDll)(NULL, 0, pDllPath, &ModuleHandle);

    //
    // load HookDll.dll
    //

    if (status == 0) 
    {
        pDllPath = (UNICODE_STRING*)&pInjectData->HookDll;

        status = ((P_LdrLoadDll)pInjectData->LdrLoadDll)(NULL, 0, pDllPath, &ModuleHandle);
    }

    //
    // get HookFunc from HookDll
    //

    if (status == 0) 
    {
		pHookFunc = (ANSI_STRING*)&pInjectData->HookFunc;

        status = ((P_LdrGetProcedureAddress)pInjectData->LdrGetProcAddr)(ModuleHandle, pHookFunc, 0, (ULONG_PTR*)&HookFunc);
#ifdef _M_ARM64
        //
        // on ARM64EC we hook the native code, hence we need to obtain the address of the native ordinal 1 from our HookDll.dll
        // instead of the FFS sequence as given by NtGetProcedureAddress when in ARM64EC mode
        //

        if (pLdrData->Flags.IsArm64ec && status >= 0) {
            HookFunc = (P_HookFunc)Hook_GetFFSTarget((UCHAR*)HookFunc);
            //if (!HookFunc)
            //    status = STATUS_ENTRYPOINT_NOT_FOUND;
        }
#endif
    }

    //
    // or report error if one occurred instead
    //

    if (status != 0)
    {
        status = 0xC0000142; // = STATUS_DLL_INIT_FAILED
        ULONG_PTR Parameters[1] = { (ULONG_PTR)pDllPath };
        ULONG ErrorReturn;
        ((P_NtRaiseHardError)pInjectData->NtRaiseHardError)(
            status | 0x10000000, // | FORCE_ERROR_MESSAGE_BOX
            1, 1, Parameters, 1, &ErrorReturn);

        return;
    }

    //
    // call HookFunc from HookDll.dll
    // 
    // Note: HookFunc may free ExtraData
    //

    HookFunc(pInjectData);
}