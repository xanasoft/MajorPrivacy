#include "injdll.h"

#ifdef USE_WOW64EXT
#include "../../wow64ext/wow64ext.h"
#else

NTSYSCALLAPI NTSTATUS NTAPI NtGetContextThread(
	_In_ HANDLE ThreadHandle,
	_Inout_ PCONTEXT ThreadContext
);

NTSYSCALLAPI NTSTATUS NTAPI NtSetContextThread(
	_In_ HANDLE ThreadHandle,
	_In_ PCONTEXT ThreadContext
);

NTSYSCALLAPI NTSTATUS NTAPI NtAllocateVirtualMemory(
	_In_ HANDLE ProcessHandle,
	_Inout_ PVOID* BaseAddress,
	_In_ ULONG_PTR ZeroBits,
	_Inout_ PSIZE_T RegionSize,
	_In_ ULONG AllocationType,
	_In_ ULONG Protect
);

NTSYSCALLAPI NTSTATUS NTAPI NtReadVirtualMemory(
	_In_ HANDLE ProcessHandle,
	_In_opt_ PVOID BaseAddress,
	_Out_writes_bytes_(BufferSize) PVOID Buffer,
	_In_ SIZE_T BufferSize,
	_Out_opt_ PSIZE_T NumberOfBytesRead
);


NTSYSCALLAPI NTSTATUS NTAPI NtWriteVirtualMemory(
	_In_ HANDLE ProcessHandle,
	_In_opt_ PVOID BaseAddress,
	_In_reads_bytes_(BufferSize) PVOID Buffer,
	_In_ SIZE_T BufferSize,
	_Out_opt_ PSIZE_T NumberOfBytesWritten
);

NTSYSCALLAPI NTSTATUS NTAPI NtProtectVirtualMemory(
	_In_ HANDLE ProcessHandle,
	_Inout_ PVOID* BaseAddress,
	_Inout_ PSIZE_T RegionSize,
	_In_ ULONG NewProtect,
	_Out_ PULONG OldProtect
);

NTSYSCALLAPI NTSTATUS NTAPI NtFlushInstructionCache(
	_In_ HANDLE ProcessHandle,
	_In_opt_ PVOID BaseAddress,
	_In_ SIZE_T Length
);

#define NtGetContextThread64 NtGetContextThread
#define NtSetContextThread64 NtSetContextThread

#define NtAllocateVirtualMemory64(ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect) \
	NtAllocateVirtualMemory(ProcessHandle, (PVOID)(BaseAddress), ZeroBits, RegionSize, AllocationType, Protect)

#define NtReadVirtualMemory64(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesRead) \
	NtReadVirtualMemory(ProcessHandle, (PVOID)(BaseAddress), Buffer, BufferSize, NumberOfBytesRead)

#define NtWriteVirtualMemory64(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesWritten) \
	NtWriteVirtualMemory(ProcessHandle, (PVOID)(BaseAddress), Buffer, BufferSize, NumberOfBytesWritten)

#define NtProtectVirtualMemory64(ProcessHandle, BaseAddress, RegionSize, NewProtect, OldProtect) \
	NtProtectVirtualMemory(ProcessHandle, BaseAddress, RegionSize, NewProtect, OldProtect)

#define NtFlushInstructionCache64(ProcessHandle, BaseAddress, Length) \
	NtFlushInstructionCache(ProcessHandle, BaseAddress, Length)

#endif

#define MY_A64
#include "myasm.h"


/*
typedef struct _UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PVOID Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

typedef struct _ANSI_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PCHAR Buffer;
} ANSI_STRING, * PANSI_STRING;

typedef void(__stdcall* P_LdrLoadDll) (
	IN PWCHAR PathToFile OPTIONAL,
	IN ULONG Flags OPTIONAL,
	IN PUNICODE_STRING ModuleFileName,
	OUT HMODULE* ModuleHandle);

typedef void(__stdcall* P_LdrGetProcedureAddress) (
	IN HMODULE ModuleHandle,
	IN PANSI_STRING FunctionName OPTIONAL,
	IN WORD Ordinal OPTIONAL,
	OUT PVOID* FunctionAddress);

typedef void(__stdcall* P_RtlInitUnicodeString) (
	PUNICODE_STRING DestinationString,
	PCWSTR SourceString);

typedef void(__stdcall* P_RtlInitAnsiString) (
	PANSI_STRING DestinationString,
	PCSTR SourceString);
*/

#define CODE_OP_RESTORE		1
#define CODE_OP_KERNEL32	2
#define CODE_OP_USE_VA		4
#define CODE_OP_32_BIT		8

DWORD64 write_shellcode_a64(HANDLE hProcess, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size, ULONG options)
{
	DWORD64 LLW;
	DWORD64 GPA;
	DWORD64 FIC;

#ifndef USE_WOW64EXT
	//LLW = (DWORD64)LoadLibraryW;	// kernel32.dll
	//GPA = (DWORD64)GetProcAddress;	// kernel32.dll
	HMODULE hNtdll = GetModuleHandle(L"ntdll.dll");
	// Note: to only invoke an already loaded DLL use LdrGetDllHandle instead of LdrLoadDll, same arguments
	//LLW = (DWORD64)GetProcAddress(hNtdll, "LdrLoadDll");
	//GPA = (DWORD64)GetProcAddress(hNtdll, "LdrGetProcedureAddress");
	//FIC = (DWORD64)GetProcAddress(hNtdll, "NtFlushInstructionCache");

	// # variants
	//LLW = 0x00007FFA5A969920;
	//GPA = 0x00007FFA5A9691C0;
	//FIC = 0x00007ffa5a95b320;

	DWORD64 ntdllBase = FindDllBase64(hProcess, L"\\system32\\ntdll.dll");
	LLW = 0x0000000077882e90;//  FindRemoteDllExport(hProcess, ntdllBase, "#LdrLoadDll");
	GPA = FindRemoteDllExport(hProcess, ntdllBase, "#LdrGetProcedureAddress");
	FIC = FindRemoteDllExport(hProcess, ntdllBase, "#NtFlushInstructionCache");

#else
	LLW = GetProcAddress64(getNTDLL64(), "LdrLoadDll");
	GPA = GetProcAddress64(getNTDLL64(), "LdrGetProcedureAddress");
	FIC = 0;
#endif
	if (LLW == 0 || GPA == 0 || FIC == 0)
		return FALSE;

	DWORD64 EF = 0;
	if (funcName && (options & CODE_OP_USE_VA)) {
		EF = FindDllExportFromFile(dllpath, funcName);
		if (EF == 0)
			return FALSE;
	}

#define MAX_NAME		64
#define HEADER_SIZE		(12 * 8)
#define CODE_SIZE		(40 * 8)

#define JMP_OFFSET		(0 * 8)
#define LLD_OFFSET		(1 * 8)
#define GPA_OFFSET		(2 * 8)
		//unused	 // (3 * 8)
#define BYTES_OFFSET	(4 * 8)

#define NAME_OFFSET		(HEADER_SIZE + CODE_SIZE)
#define PATH_OFFSET		(NAME_OFFSET				+ NTSTR64_SIZE + MAX_NAME)
#define PARAM_OFFSET	(PATH_OFFSET											+ NTSTR64_SIZE + path_size) // variable length

#define path_size (MAX_PATH + 1) * sizeof(wchar_t)
	BYTE code[PARAM_OFFSET] = { 0 };
#undef path_size

	DWORD path_size = (DWORD)((wcslen(dllpath) + 1) * sizeof(wchar_t));
	if (path_size > MAX_PATH * sizeof(wchar_t))
		return 0;

	//
	// Overall shell code structure
	// 
	// Base Address 0x0000 - 0xXXXX
	//   Headder Data 0x0000 - 0x0040 // 8*8
	//     Final Jump Address
	//     Address of LdrLoadDll
	//     Address of LdrGetProcedureAddress
	//     Address of NtFlushInstructionCache
	// 
	//     stolen bytes1
	//     stolen bytes2
	//     stolen bytes3
	//     current process pseudo handle (-1)
	// 
	//     unused
	//     unused
	//     unused
	//     return code
	// 
	//   Entry Point 0x0040 - 0xXXXX
	//		Save All Registers
	//		Restore original function bytes (optional)
	//		Load Kernel32.dll (optional) // todo
	//		Load Library
	//		Get Function Address (optional)
	//		Call Function (optional)
	//		Restore All Registers
	//		Jump to Final Address
	//   Footer Data
	//     Kernel32.dll Name (UnicodeString) = 16 + 32 // todo
	//     Function Name String (AnsiString) = 16 + MAX_NAME
	//     Library Path String (UnicodeString) = 16 + (MAX_PATH + 1) * 2
	//     User Params
	//


	TYPES ip;
	ip.pB = code;
#define BASE (-(ip.pB - code)) // relative offset from ip to base

	/* >>> BASE <<< */
	*ip.pQ++ = 0;			// original pc
	*ip.pQ++ = LLW;			// LoadLibraryW
	*ip.pQ++ = GPA;			// GetProcAddress
	*ip.pQ++ = FIC;			// NtFlushInstructionCache

	*ip.pQ++ = 0;			// stolen bytes1
	*ip.pQ++ = 0;			// stolen bytes2
	*ip.pQ++ = 0;			// stolen bytes3
	*ip.pQ++ = -1;			// current process pseudo handle

	//*ip.pQ++ = 0;			// unused
	*ip.pQ++ = EF;			// entry function VA
	*ip.pQ++ = 0;			// unused
	*ip.pQ++ = 0;			// unused
	//*ip.pQ++ = 0;			// unused

	// we use this instead of original rip to return if we run in a new thread
	RET;					// ret
	BRK;					// brk #0xF000

	/* >>> enter here <<< */
	/**ip.pL++ = 0xd503201f;	// nop
	*ip.pL++ = 0xd503201f;	// nop
	*ip.pL++ = 0xD43E0000;	// brk #0xF000
	*ip.pL++ = 0xd503201f;	// nop
	*ip.pL++ = 0xd503201f;	// nop*/

	// save all registers
	SUB_SP_SP(8 * 22);		// sub sp, sp, #0xA0 (8*20)
	for (int i = 0; i < 18; i++)
		STR_SP(i, i * 8);		// str xi, [sp, #(8 * i)]
	*ip.pL++ = 0xF9004BFE;		// str lr, [sp, 0x90]
	// todo: Floating-point/SIMD registers

#define SBASE (8 * 20)
	
	// restore stolen bytes
	if (options & CODE_OP_RESTORE) {
		LDR_PC(8, JMP_OFFSET + BASE);	// ldr x8, #BASE

		LDR_PC(9, BYTES_OFFSET + BASE);	// ldr x9, #BYTES_OFFSET
		STR_X(9, 8, 0);					// STR x9,[x8,8]

		LDR_PC(9, BYTES_OFFSET + 8 + BASE);
		STR_X(9, 8, 8);

		LDR_PC(9, BYTES_OFFSET + 16 + BASE);
		STR_X(9, 8, 16);

		//*ip.pL++ = 0xD43E0000; // brk #0xF000

		// flush instruction cache -  IMPORTANT!!!
		
		LDR_PC(0, BYTES_OFFSET + 24 + BASE);// ldr x1, #BASE// ProcessHandle
		LDR_PC(1, JMP_OFFSET + BASE);		// ldr x1, #BASE// BaseAddress
		MOV(2, 64);							// mov x2, #0	// NumberOfBytesToFlush 

		LDR_PC(9, BASE + 24);				// ldr x8, #BASE + 8 // NtFlushInstructionCache
		BRL(9);								// brl x8
	}

//#if 0
	// Load Library
	MOV(0, 0);			// mov x0,#0 // PathToFile
	MOV(1, 0);			// mov x1,#0 Flags
	ADR(2, PATH_OFFSET + BASE); // adr x2, #0xXXXX // ModuleFileName
	ADD_SP(3, SBASE + 0);		// add x3,sp,#0x90 // ModuleHandle

	LDR_PC(8, BASE + 8);	// ldr x8, #BASE + 8 // LdrLoadDll
	BRL(8);					// brl x8

	// todo: test result

#if 0

	//*ip.pL++ = 0xD43E0000; // brk #0xF000

  if (EF != 0) {

	  LDR_SP(8, SBASE + 0);		// ldr x8,[sp,#0x90] // ModuleHandle
	  LDR_PC(9, BYTES_OFFSET + 32 + BASE);// ldr x9, #EF_VA

	  *ip.pL++ = 0x8B090108;	// add x8, x8, x9

	  // Call init function
	  if (param) {
		  ADR(0, PARAM_OFFSET + BASE); // adr x0, #0xXXXX // Params
	  }
	  else {
		  MOV(0, 0);		// mov x0,#0
	  }
	  MOV(1, size);			// mov x1,#XXXX // Size - limited to 0xFFFF

	  //*ip.pL++ = 0xD43E0000; // brk #0xF000
	  BRL(8);

  } else
  if (funcName) {

	// Get init function address
	LDR_SP(0, SBASE + 0);		// ldr x0,[sp,#0x90] // ModuleHandle
	ADR(1, NAME_OFFSET + BASE); // adr x2, #0xXXXX // FunctionName 
	MOV(2, 0);					// mov x2,#0 // Oridinal = 0
	ADD_SP(3, SBASE + 8);		// add x3,sp,#0x98 // *FunctionAddress

	LDR_PC(8, BASE + 16);	// ldr x8, #BASE + 16 // LdrGetProcedureAddress
	BRL(8);					// brl x8

	// todo: test result
	

	// Call init function
	if (param) {
		ADR(0, PARAM_OFFSET + BASE); // adr x0, #0xXXXX // Params
	}
	else {
		MOV(0, 0);		// mov x0,#0
	}
	MOV(1, size);			// mov x1,#XXXX // Size - limited to 0xFFFF

	LDR_SP(8, SBASE + 8);		// ldr x8,[sp,#0x98] // FunctionAddress

	//*ip.pL++ = 0xD43E0000; // brk #0xF000
	BRL(8);
  }

#endif

	// restore all registers
	for (int i = 0; i < 18; i++)
	  LDR_SP(i, i * 8);			// ldr xi, [sp, #(8 * i)]
	*ip.pL++ = 0xF9404BFE;		// ldr lr, [sp, 0x90]
	// todo: Floating-point/SIMD registers
	ADD_SP_SP(8 * 22);		// add sp, sp, #0xA0 (8*20)

	//*ip.pL++ = 0xD43E0000; // brk #0xF000
	
	// resume original controll flow
	LDR_PC(17, JMP_OFFSET + BASE);	// ldr x17, #BASE
	BR(17);					// br x17

	//LDR_PC(8, 8);			// ldr pc + 4
	//BR(8);					// br x8
	//*ip.pQ++ = 0x00007ffd4435bf70; // hardcoded value not use register

	ip.pL++;

	if (ip.pB > code + HEADER_SIZE + CODE_SIZE)
		DebugBreak(); // CODESIZE to small !!!!!!!



	DWORD64 mem = 0;
	DWORD mem_size = PARAM_OFFSET;
	SIZE_T rs = mem_size + size;
	NtAllocateVirtualMemory64(hProcess, &mem, 0, &rs, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!mem)
		return 0;

	if (options & CODE_OP_32_BIT)
	{
		if (funcName)
			PrepareAnsiString32(mem, code, NAME_OFFSET, funcName); //RtlInitAnsiString - strcpy(code + NTSTR64_SIZE + NAME_OFFSET, funcName);

		PrepareUnicodeString32(mem, code, PATH_OFFSET, dllpath); // RtlInitUnicodeString - wcscpy(code + NTSTR64_SIZE + PATH_OFFSET, dllpath);
	}
	else
	{
		if (funcName)
			PrepareAnsiString64(mem, code, NAME_OFFSET, funcName); //RtlInitAnsiString - strcpy(code + NTSTR64_SIZE + NAME_OFFSET, funcName);

		PrepareUnicodeString64(mem, code, PATH_OFFSET, dllpath); // RtlInitUnicodeString - wcscpy(code + NTSTR64_SIZE + PATH_OFFSET, dllpath);
	}

	NtWriteVirtualMemory64(hProcess, mem, code, mem_size, NULL); // write code + data
	if (param)
		NtWriteVirtualMemory64(hProcess, mem + mem_size, (LPVOID)param, size, NULL); // append params

	NtFlushInstructionCache64(hProcess, (LPCVOID)mem, mem_size);

	return mem;
}

BOOL hijack_thread_a64(HANDLE hProcess, HANDLE hThread, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size)
{
	NTSTATUS status;
	ARM64_NT_CONTEXT context;
	context.ContextFlags = CONTEXT_ARM64_CONTROL;
//	_CONTEXT64_2 context;
//	context.ContextFlags = CONTEXT64_CONTROL;
//#define Pc Rip

	DWORD64 mem = write_shellcode_a64(hProcess, dllpath, funcName, param, size, 0);
	if (!mem)
		return FALSE;
	DWORD64 entry = mem + HEADER_SIZE;

	status = NtGetContextThread64(hThread, &context);

	status = NtWriteVirtualMemory64(hProcess, mem, &context.Pc, sizeof(DWORD64), NULL);

	context.Pc = entry;
	status = NtSetContextThread64(hThread, &context); 

	// todo: returns STATUS_SET_CONTEXT_DENIED when trying from an x64 process

	return TRUE;
}


typedef DWORD(WINAPI * pRtlCreateUserThread)(IN HANDLE ProcessHandle, IN PSECURITY_DESCRIPTOR SecurityDescriptor,
	IN BOOL CreateSuspended, IN ULONG StackZeroBits, IN OUT PULONG StackReserved, IN OUT PULONG	StackCommit,
	IN LPVOID StartAddress, IN LPVOID StartParameter, OUT HANDLE ThreadHandle, OUT LPVOID ClientID);

BOOL inject_thread_a64(HANDLE hProcess, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size)
{
	DWORD64 mem = write_shellcode_a64(hProcess, dllpath, funcName, param, size, 0);
	if (!mem)
		return FALSE;
	DWORD64 entry = mem + HEADER_SIZE;
	DWORD64 ret   = mem + (HEADER_SIZE - 8);

	NtWriteVirtualMemory64(hProcess, mem, &ret, sizeof(DWORD64), NULL);

	DWORD64 hRemoteThread = 0;
	pRtlCreateUserThread RtlCreateUserThread = NULL;

#ifndef USE_WOW64EXT
	RtlCreateUserThread = (pRtlCreateUserThread)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "RtlCreateUserThread");
	if (RtlCreateUserThread == NULL)
		return FALSE;
	DWORD status = RtlCreateUserThread(hProcess, NULL, 0, 0, 0, 0, (LPVOID)entry, NULL, (PHANDLE)&hRemoteThread, NULL);
#else
    DWORD64 nrvm = GetProcAddress64(getNTDLL64(), "RtlCreateUserThread");
    if (nrvm == 0)
        return FALSE;
    DWORD64 status = X64Call(nrvm, 10, (DWORD64)hProcess, (DWORD64)NULL, (DWORD64)0, (DWORD64)0, (DWORD64)0, (DWORD64)0, (DWORD64)entry, (DWORD64)NULL, (DWORD64)&hRemoteThread, (DWORD64)NULL);
#endif
	if(status < 0)
		return FALSE;

	WaitForSingleObject((HANDLE)hRemoteThread, INFINITE);
	
	// Note: sonce we wait we could read the param back for bidirectional communication

	CloseHandle((HANDLE)hRemoteThread);

	return TRUE;
}

BOOL hijack_function_a64(HANDLE hProcess, DWORD64 pTargetFunc, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size)
{
	NTSTATUS status;
	UCHAR code[24];

	//wcscpy(dllpath, L"\\\\hexi\\HackerLib\\Build\\ARM64\\Debug\\HackLib.dll");
	//wcscpy(dllpath, L"\\\\hexi\\HackerLib\\Build\\ARM64EC\\Debug\\HackLib.dll");
	//wcscpy(dllpath, L"\\\\hexi\\HackerLib\\Build\\x64\\Debug\\HackLib.dll");
	//wcscpy(dllpath, L"\\\\hexi\\HackerLib\\Build\\win32\\Debug\\HackLib.dll");
	//wcscpy(dllpath, L"\\\\hexi\\HackerLib\\Build\\arm\\Debug\\HackLib.dll");

	/// test begin

	//ntdllBase 0x00007ffa5a7d0000
	//0x00007ffa5a811050 {ntdll.dll!LdrLoadDll(void)}
	//0x00007ffa5a7d1890 {ntdll.dll!EXP+#LdrLoadDll}
	//0x00007ffa5a969920 {ntdll.dll!#LdrLoadDll}

	//HMODULE hNtdll = GetModuleHandle(L"ntdll.dll");
	//DWORD64 LLW1 = GetProcAddress(hNtdll, "LdrLoadDll");
	//DWORD64 LLW1 = FindRemoteDllExport(GetCurrentProcess(), (DWORD64)hNtdll, "LdrLoadDll");

//	DWORD64 ntdllBase = FindDllBase(hProcess, L"\\system32\\ntdll.dll");
	//DWORD64 LLW2 = FindRemoteDllExport(hProcess, ntdllBase, "LdrLoadDll");
	//DWORD64 LLW3 = FindRemoteDllExport(hProcess, ntdllBase, "#LdrLoadDll");

	//pTargetFunc = 0x00007FFC8AD85610; //#LdrInitializeThunk
	//pTargetFunc = 0x00007FFA5A969920; //#LdrLoadDll

//	pTargetFunc = FindRemoteDllExport(hProcess, ntdllBase, "#LdrLoadDll");

	/////

	//DWORD64 LLW4 = (DWORD64)hNtdll + FindDllExportFromFile(L"C:\\Windows\\system32\\ntdll.dll", "LdrLoadDll");
	//DWORD64 LLW5 = (DWORD64)hNtdll + FindDllExportFromFile(L"C:\\Windows\\system32\\ntdll.dll", "#LdrLoadDll");


	/*DWORD64 hackBase = GetModuleHandle(L"hackLib.dll");
	DWORD64 LLW6 = FindRemoteDllExport(GetCurrentProcess(), hackBase, "HackApi_Install");
	DWORD64 LLW7 = FindRemoteDllExport(GetCurrentProcess(), hackBase, "#HackApi_Install");*/

	/// test end


	DWORD64 mem = write_shellcode_a64(hProcess, dllpath, funcName, param, size, CODE_OP_RESTORE | CODE_OP_32_BIT);
	if (!mem)
		return FALSE;
	DWORD64 entry = mem + HEADER_SIZE;

	// write the target function address to the final jump target location
	status = NtWriteVirtualMemory64(hProcess, mem, &pTargetFunc, sizeof(DWORD64), NULL);


	// save target function prolog
	DWORD64 backup = mem + BYTES_OFFSET;
	NtReadVirtualMemory64(hProcess, pTargetFunc, code, sizeof(code), NULL);
	NtWriteVirtualMemory64(hProcess, backup, code, sizeof(code), NULL);

	// memcpy(&inject->RtlFindActCtx_Bytes, LdrCode, 12);

	// make target writable
	void* RegionBase = (void*)pTargetFunc;
	SIZE_T RegionSize = sizeof(code);
	ULONG OldProtect;
	status = NtProtectVirtualMemory64(hProcess, &RegionBase, &RegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);

	// write detour to target function
	TYPES ip;
	ip.pB = code;

	//*ip.pL++ = 0xD43E0000;	// brk #0xF000
	//*ip.pL++ = 0xd503201f;	// nop
	LDR_PC(17, 8);				// ldr x17, [pc + 4]
	BR(17);						// br x17
	*ip.pQ++ = entry;

	if (ip.pB > code + sizeof(code))
		DebugBreak(); // CODESIZE to small !!!!!!!

	status = NtWriteVirtualMemory64(hProcess, pTargetFunc, code, ip.pB - code, NULL);

	NtFlushInstructionCache64(hProcess, pTargetFunc, ip.pB - code);

	return TRUE;
}


