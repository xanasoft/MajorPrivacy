/*
 * Copyright 2020-2023 David Xanatos, xanasoft.com
 */

#ifndef _MY_LDRDATA_H
#define _MY_LDRDATA_H


#define NATIVE_FUNCTION_NAMES   { "NtDelayExecution", "NtDeviceIoControlFile", "NtFlushInstructionCache", "NtProtectVirtualMemory" }
#define NATIVE_FUNCTION_COUNT   4
#define NATIVE_FUNCTION_SIZE    32

//
// LDRCODE_DATA symbol is located in the "zzzz" section of the ldrcode
// as defined in the entry_*.asm as LdrCodeData
//
#define LDRCODE_INJECTION_SECTION ".text"
#define LDRCODE_SYMBOL_SECTION     "zzzz"

typedef union _LDRCODE_FLAGS {
	ULONG	uFlags;
	struct {
		ULONG
            IsWoW64		    : 1,
            IsArm64ec		: 1,
            IsXtAjit        : 1,
            IsChpe32        : 1,
			Reservd_1		: 4,

			Reservd_2		: 8,

            Reservd_3		: 8,

			Reservd_4		: 8;
	};
} LDRCODE_FLAGS;

typedef struct _LDRCODE_DATA {
    ULONG64     NtDllBase;
    ULONG64     NativeDetour;

    ULONG64     ExtraData;

    LDRCODE_FLAGS Flags;
    UCHAR       InitDone;
    UCHAR       Reserved[3];

    __declspec(align(16))
        UCHAR   LdrInitializeThunk_Tramp[48];

    __declspec(align(16))
        UCHAR   NtDelayExecution_code[NATIVE_FUNCTION_SIZE];
    __declspec(align(16))
        UCHAR   NtDeviceIoControlFile_code[NATIVE_FUNCTION_SIZE]; 
    __declspec(align(16))
        UCHAR   NtFlushInstructionCache_code[NATIVE_FUNCTION_SIZE];
    __declspec(align(16))
        UCHAR   NtProtectVirtualMemory_code[NATIVE_FUNCTION_SIZE];

    ULONG64     RealNtDeviceIoControlFile;
    ULONG64     NtDeviceIoControlFile; // for ARM64
    ULONG64     NativeNtProtectVirtualMemory;
    ULONG64     NativeNtRaiseHardError;

#ifdef _WIN64
    ULONG64     NtDllWoW64Base;
    ULONG64     WoW64Detour;
#endif

    __declspec(align(16))
        UCHAR   RtlImageOptionsEx_Tramp[48];

} LDRCODE_DATA;

//
// Additional data needed to inject the HookDll are passed in
// the LDRCODE_EXTRA_DATA note that on ARM64 LDRCODE_DATA is alocated
// beyond the 64 bit boundary hence only LDRCODE_EXTRA_DATA
// can be used by the a 32-bit DetourFunc function
//

typedef struct _LDRCODE_EXTRA_DATA {

    ULONG LdrLoadDll_Offset;
    ULONG LdrGetProcAddr_Offset;
    ULONG NtProtectVirtualMemory_Offset;
    ULONG NtRaiseHardError_Offset;
    ULONG NtDeviceIoControlFile_Offset;
    ULONG RtlFindActCtx_Offset;
    ULONG RtlImageOptionsEx_Offset;

    ULONG KernelDll_Offset;
    ULONG KernelDll_Length;

    ULONG NativeHookDll_Offset;
    ULONG NativeHookDll_Length;
    ULONG Arm64ecHookDll_Offset;
    ULONG Arm64ecHookDll_Length;
    ULONG Wow64HookDll_Offset;
    ULONG Wow64HookDll_Length;

    ULONG HookFunc_Offset;
	ULONG HookFunc_Length;

    ULONG InjectData_Offset;

    ULONG_PTR InitLock;

} LDRCODE_EXTRA_DATA;

//
// UNICIDE_STRING compatible with 32 and 64 bit API
//

typedef struct _UNIVERSAL_STRING {
    USHORT  Length;
    USHORT  MaxLen;
    ULONG   Buf32;
    ULONG64 Buf64;
} UNIVERSAL_STRING;


//
// temporary data used by the Detour Code any changed to 
// this structure must be synchronized with all 3 versions of the
// in entry_*.asm !!!
//

typedef struct _INJECT_DATA {

    ULONG64 LdrData;            // 0
    ULONG64 HookTarget;         // 8
    ULONG HookTarget_Protect;
    UCHAR HookTarget_Bytes[20];

    ULONG64 LdrLoadDll;
    ULONG64 LdrGetProcAddr;
    ULONG64 NtProtectVirtualMemory;
    ULONG64 NtRaiseHardError;
    ULONG64 NtDeviceIoControlFile;

    UNIVERSAL_STRING KernelDll;
    UNIVERSAL_STRING HookDll;

    UNIVERSAL_STRING HookFunc;

} INJECT_DATA;


//---------------------------------------------------------------------------


#endif
