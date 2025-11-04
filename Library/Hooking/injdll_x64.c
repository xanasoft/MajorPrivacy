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
	NtProtectVirtualMemory(ProcessHandle, (PVOID)(BaseAddress), RegionSize, NewProtect, OldProtect)

#define NtFlushInstructionCache64(ProcessHandle, BaseAddress, Length) \
	NtFlushInstructionCache(ProcessHandle, BaseAddress, Length)

#endif

#define MY_X64
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

DWORD64 write_shellcode_x64(HANDLE hProcess, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size, ULONG options)
{
	DWORD64 ntdllBase = FindDllBase64(hProcess, L"\\system32\\ntdll.dll");
	DWORD64 LLW = FindRemoteDllExport(hProcess, ntdllBase, "LdrLoadDll");
	DWORD64 GPA = FindRemoteDllExport(hProcess, ntdllBase, "LdrGetProcedureAddress");
	if (LLW == 0 || GPA == 0)
		return FALSE;

#define MAX_NAME		64
#define HEADER_SIZE		(8 * 8)
#define CODE_SIZE		(26 * 8)

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
	//     unused
	//     stolen bytes1
	//     stolen bytes2
	//     unused
	//     return code
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
	*ip.pQ++ = 0;				// original rip
	*ip.pQ++ = LLW;				// LoadLibraryW
	*ip.pQ++ = GPA;				// GetProcAddress
	*ip.pQ++ = 0;				// unused

	*ip.pQ++ = 0;				// stolen bytes1
	*ip.pQ++ = 0;				// stolen bytes2
	*ip.pQ++ = 0;				// unused
	//*ip.pQ++ = 0;				// unused

	// we use this instead of original rip to return if we run in a new thread
	asm_op(&ip, RET);			// ret
	asm_op(&ip, INT3);			// int 3
	*ip.pW++ = 0x9090;			// nop nop 
	*ip.pL++ = 0x90909090;		// nop nop nop nop


	/* >>> enter here <<< */
	//asm_op(&ip, INT3);

	// save all registers
	asm_op(&ip, PUSHF);
	for (int i = 0; i <= 0x0F; i++) {
		if (i == RSP) continue;
		asm_op_reg(&ip, PUSH, i);
	}

#define STACK_SIZE (8*7)
#define SLOT(x) (8*(x+4))

	// Stack layout
	//	QWORD ModuleHandle		+30h	// stack slot 2
	//	QWORD FunctionAddress	+28h	// stack slot 1
	//  QWORD					+20h	// stack slot 0
	//	QWORD[4] // scratch space for calls
	
	asm_op_reg_val(&ip, SUB, RSP, STACK_SIZE);						// sub rsp, 38h

	// restore stolen bytes
  if (options & CODE_OP_RESTORE) {
	asm_op_reg_ptr(&ip, MOV, R8, RIP, JMP_OFFSET + BASE);			// mov r8,[rip+...]
	asm_op_reg_ptr(&ip, MOV, R9, RIP, BYTES_OFFSET + BASE);			// mov r8,[rip+...]
	asm_op_ptr_reg(&ip, MOV, R8, 0, R9);							// mov [r8],r9
	asm_op_reg_ptr(&ip, MOV, R9, RIP, BYTES_OFFSET + 8 + BASE);		// mov r8,[rip+...]
	asm_op_ptr_reg(&ip, MOV, R8, 8, R9);							// mov [r8+8],r9
  }

	// Load Library
    asm_op_reg_ptr(&ip, LEA, R9, RSP, SLOT(1));						// lea r9, [rsp+0x30] - &ModuleHandle 
	asm_op_reg_ptr(&ip, LEA, R8, RIP, PATH_OFFSET + BASE);			// lea r8,[rip+...]	- &ModuleFileName
	asm_op_reg_val(&ip, MOV, RDX, 0);								// mov rdx, 0
	asm_op_reg_val(&ip, MOV, RCX, 0);								// mov rcx, 0
	asm_op_ptr(&ip, CALL, RIP, LLD_OFFSET + BASE);					// call [rip-...] - LdrLoadDll(PathToFile OPTIONAL, Flags, &ModuleFileName, &ModuleHandle);

	// todo: test result

  if (funcName) {

	// Get init function address
	asm_op_reg_ptr(&ip, LEA, R9, RSP, SLOT(2));						// lea r9, [rsp+0x28] - &FunctionAddress
	asm_op_reg_val(&ip, MOV, R8, 0);								// mov r8, 0
	asm_op_reg_ptr(&ip, LEA, RDX, RIP, NAME_OFFSET + BASE);			// lea rdx,[rip+...] - FunctionName
	asm_op_reg_ptr(&ip, MOV, RCX, RSP, SLOT(1));					// mov rcs, [rsp+30h] - ModuleHandle
	asm_op_ptr(&ip, CALL, RIP, GPA_OFFSET + BASE);					// call [rip-...] - LdrGetProcedureAddress(ModuleHandle, FunctionName, Oridinal, &FunctionAddress)

	// todo: test result

	// Call init function
	asm_op_reg_ptr(&ip, MOV, RAX, RSP, SLOT(2));					// mov rax, [rsp+28h]
	asm_op_reg_val(&ip, MOV, RDX, size);							// mov rdx, size
	if (param) {
		asm_op_reg_ptr(&ip, LEA, RCX, RIP, PARAM_OFFSET + BASE);	// lea rcx,[rip+...]
	}
	else {
		asm_op_reg_val(&ip, MOV, RCX, 0);							// mov rcx, 0
	}
	asm_op_reg(&ip, CALL, RAX);										// call rax
  }

    asm_op_reg_val(&ip, ADD, RSP, STACK_SIZE);						// add rsp, 38h

	// restore all registers
	for (int i = 0x0F; i >= 0; i--) {
		if (i == RSP) continue;
		asm_op_reg(&ip, POP, i);
	}
	asm_op(&ip, POPF);

	// resume original controll flow
	asm_op_ptr(&ip, JMP, RIP, JMP_OFFSET + BASE);					// jmp [rip-...]
	
	if (ip.pB > code + HEADER_SIZE + CODE_SIZE)
		DebugBreak(); // CODE_SIZE to small !!!!!!!

	DbgPrint("\n\nshell code:\n");
	for (int i = 0; i < ip.pB - code; i++) {
		DbgPrint("%02x ", code[i]);
		if ((i + 1) % 16 == 0) DbgPrint("\n");
	}
	DbgPrint("\n\n");

	//
	// now write the shell code to the target process
	//

	DWORD64 mem = 0;
	DWORD mem_size = PARAM_OFFSET;
	SIZE_T rs = mem_size + size;
	NtAllocateVirtualMemory64(hProcess, &mem, 0, &rs, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!mem)
		return 0;

	if(funcName)
		PrepareAnsiString64(mem, code, NAME_OFFSET, funcName); //RtlInitAnsiString - strcpy(code + NTSTR64_SIZE + NAME_OFFSET, funcName);

	PrepareUnicodeString64(mem, code, PATH_OFFSET, dllpath); // RtlInitUnicodeString - wcscpy(code + NTSTR64_SIZE + PATH_OFFSET, dllpath);
	
	NtWriteVirtualMemory64(hProcess, mem, code, mem_size, NULL); // write code + data
	if (param)
		NtWriteVirtualMemory64(hProcess, mem + mem_size, (LPVOID)param, size, NULL); // append params

	NtFlushInstructionCache64(hProcess, (LPCVOID)mem, mem_size);

	return mem;
}


BOOL hijack_thread_x64(HANDLE hProcess, HANDLE hThread, BOOL bARM64EC, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size)
{
	NTSTATUS status;
#if defined(_M_ARM64)
	CONTEXT context;
	context.ContextFlags = CONTEXT_CONTROL;

	#define Rip Pc
#elif defined(_M_X64)
	CONTEXT context;
	context.ContextFlags = CONTEXT_CONTROL;
#else
	_CONTEXT64_2 context;
	context.ContextFlags = CONTEXT64_CONTROL;
#endif

	DWORD64 mem = write_shellcode_x64(hProcess, dllpath, funcName, param, size, 0);
	if (!mem)
		return FALSE;
	DWORD64 entry = mem + HEADER_SIZE;

	status = NtGetContextThread64(hThread, &context);

	status = NtWriteVirtualMemory64(hProcess, mem, &context.Rip, sizeof(DWORD64), NULL);

	context.Rip = entry;
	status = NtSetContextThread64(hThread, &context);

#if defined(_M_ARM64)
	#undef Rip
#endif

	return TRUE;
}


typedef DWORD(WINAPI * pRtlCreateUserThread)(IN HANDLE ProcessHandle, IN PSECURITY_DESCRIPTOR SecurityDescriptor,
	IN BOOL CreateSuspended, IN ULONG StackZeroBits, IN OUT PULONG StackReserved, IN OUT PULONG	StackCommit,
	IN LPVOID StartAddress, IN LPVOID StartParameter, OUT HANDLE ThreadHandle, OUT LPVOID ClientID);

BOOL inject_thread_x64(HANDLE hProcess, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size)
{
	DWORD64 mem = write_shellcode_x64(hProcess, dllpath, funcName, param, size, 0);
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

BOOL hijack_function_x64(HANDLE hProcess, DWORD64 pTargetFunc, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size)
{
	NTSTATUS status;
	UCHAR code[16];

	DWORD64 mem = write_shellcode_x64(hProcess, dllpath, funcName, param, size, CODE_OP_RESTORE);
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

	//asm_op(&ip, INT3);
	asm_mov_reg_val64(&ip, RAX, entry);	// movabs rax, entry
	asm_op_reg(&ip, JMP, RAX);			// jmp rax

	if (ip.pB > code + sizeof(code))
		DebugBreak(); // CODESIZE to small !!!!!!!

	DbgPrint("\n\ndetour:\n");
	for (int i = 0; i < ip.pB - code; i++) {
		DbgPrint("%02x ", code[i]);
		if ((i + 1) % 16 == 0) DbgPrint("\n");
	}
	DbgPrint("\n\n");

	status = NtWriteVirtualMemory64(hProcess, pTargetFunc, code, ip.pB - code, NULL);

	NtFlushInstructionCache64(hProcess, pTargetFunc, ip.pB - code);

	return TRUE;
}