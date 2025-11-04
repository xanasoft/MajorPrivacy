#include "injdll.h"

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

#define MY_X86
#include "myasm.h"

#ifndef USE_WOW64EXT
#	define GetThreadContext Wow64GetThreadContext
#	define SetThreadContext Wow64SetThreadContext
#endif

#define CODE_OP_RESTORE		1
#define CODE_OP_KERNEL32	2

DWORD write_shellcode_x86(HANDLE hProcess, BOOL bWoW64, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size, ULONG options)
{
	DWORD64 ntdllBase = FindDllBase64(hProcess, bWoW64 ? L"\\syswow64\\ntdll.dll" : L"\\system32\\ntdll.dll");
	//DWORD64 ntdllBase = FindDllBase(hProcess, L"\\SyChpe32\\ntdll.dll");
	DWORD64 LLW = FindRemoteDllExport(hProcess, ntdllBase, "LdrLoadDll");
	DWORD64 GPA = FindRemoteDllExport(hProcess, ntdllBase, "LdrGetProcedureAddress");
	DWORD64 FIC = FindRemoteDllExport(hProcess, ntdllBase, "NtFlushInstructionCache");
	if (LLW == 0 || GPA == 0 || FIC == 0)
		return FALSE;

#define MAX_NAME		64
#define HEADER_SIZE		(8 * 8)
#define CODE_SIZE		(26 * 8)

#define JMP_OFFSET		(0 * 8)
#define LLD_OFFSET		(1 * 8)
#define GPA_OFFSET		(2 * 8)
#define FIC_OFFSET		(3 * 8)
#define BYTES_OFFSET	(4 * 8)

#define GET_EIP_OFFSET	(7 * 8)

#define NAME_OFFSET		(HEADER_SIZE + CODE_SIZE)
#define KERNEL_OFFSET	(NAME_OFFSET				+ NTSTR32_SIZE + MAX_NAME)
#define PATH_OFFSET		(KERNEL_OFFSET						+ NTSTR32_SIZE + MAX_NAME)
#define PARAM_OFFSET	(PATH_OFFSET											+ NTSTR32_SIZE + path_size) // variable length

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
	*ip.pL++ = 0;				// original rip
	*ip.pL++ = 0;				// unused
	*ip.pL++ = LLW;				// LoadLibraryW
	*ip.pL++ = 0;				// unused
	*ip.pL++ = GPA;				// GetProcAddress
	*ip.pL++ = 0;				// unused
	*ip.pL++ = FIC;				// unused
	*ip.pL++ = 0;				// unused

	*ip.pQ++ = 0;				// stolen bytes
	*ip.pQ++ = 0;				// unused
	*ip.pQ++ = 0;				// unused
	//*ip.pQ++ = 0;				// unused

	// get the value of the EIP register
	// GET_EIP_OFFSET
	asm_op_reg_ptr(&ip, MOV, EAX, ESP, 0); // mov eax, [esp]
	asm_op(&ip, RET);			// ret

	*ip.pL++ = 0x90909090;		// nop nop nop nop 


	/* >>> enter here <<< */
	*ip.pB++ = 0x90;			// if we hijacked a thread push eip will be writen here
	*ip.pL++ = 0x90909090;
	//asm_op(&ip, INT3);

	// save all registers
	asm_op(&ip, PUSHF);
	asm_op(&ip, PUSHA);

	// store the base address in EBP
	asm_op_ptr(&ip, CALL, RIP, GET_EIP_OFFSET + BASE);	// call get_eip
	asm_op_reg_reg(&ip, MOV, EBP, EAX);					// mov ebp, eax
	asm_op_reg_val(&ip, SUB, EBP, (ip.pB - code) - 2);	// sub ebp, XXh

#define STACK_SIZE (4*3)
#define SLOT(x) (4*x)

	// Stack layout
	//	DWORD ModuleHandle		slot 2
	//	DWORD FunctionAddress	slot 1
	//  DWORD					slot 0

	asm_op_reg_val(&ip, SUB, ESP, STACK_SIZE);						// sub esp, 38h

	// restore stolen bytes
	if (options & CODE_OP_RESTORE) {
		asm_op_reg_ptr(&ip, MOV, ECX, EBP, JMP_OFFSET);				// mov r8,[rip+...]
		asm_op_reg_ptr(&ip, MOV, EAX, EBP, BYTES_OFFSET);			// mov r8,[rip+...]
		asm_op_ptr_reg(&ip, MOV, ECX, 0, EAX);						// mov [r8],r9
		asm_op_reg_ptr(&ip, MOV, EAX, EBP, BYTES_OFFSET + 4);		// mov r8,[rip+...]
		asm_op_ptr_reg(&ip, MOV, ECX, 4, EAX);						// mov [r8+8],r9

		// flush instruction cache

#if 0
		asm_op_reg_val(&ip, MOV, EAX, 8);							// mov eax, 8 - NumberOfBytesToFlush 
		asm_op_reg(&ip, PUSH, EAX);									// push eax
		asm_op_reg(&ip, PUSH, ECX);									// push ecx - BaseAddress
		asm_op_reg_val(&ip, MOV, EAX, -1);							// mov eax, -1 - ProcessHandle
		asm_op_reg(&ip, PUSH, EAX);									// push eax
		asm_op_ptr(&ip, CALL, EBP, FIC_OFFSET);						// call [ebp+...]
		// this __stdcall function cleans the stack itself
#endif
	}

	if (options & CODE_OP_KERNEL32) {
		// Load kernel32.dll
		asm_op_reg_ptr(&ip, LEA, EAX, ESP, SLOT(1));				// lea eax, [rsp+0x30] - &ModuleHandle 
		asm_op_reg_ptr(&ip, LEA, ECX, EBP, KERNEL_OFFSET);			// lea eax,[ebp+...]	- &ModuleFileName
		asm_op_reg_val(&ip, MOV, EDX, 0);							// mov eax, 0
		asm_op_reg_val(&ip, MOV, EBX, 0);							// mov eax, 0
		asm_op_reg(&ip, PUSH, EAX);									// push eax
		asm_op_reg(&ip, PUSH, ECX);									// push eax
		asm_op_reg(&ip, PUSH, EDX);									// push eax
		asm_op_reg(&ip, PUSH, EBX);									// push eax
		asm_op_ptr(&ip, CALL, EBP, LLD_OFFSET);						// call [ebp+...] - LdrLoadDll(PathToFile OPTIONAL, Flags, &ModuleFileName, &ModuleHandle);
		// this __stdcall function cleans the stack itself
	}

#if 1

	// Load Library
	asm_op_reg_ptr(&ip, LEA, EAX, ESP, SLOT(1));					// lea eax, [rsp+0x30] - &ModuleHandle 
	asm_op_reg_ptr(&ip, LEA, ECX, EBP, PATH_OFFSET);				// lea eax,[ebp+...]	- &ModuleFileName
	asm_op_reg_val(&ip, MOV, EDX, 0);								// mov eax, 0
	asm_op_reg_val(&ip, MOV, EBX, 0);								// mov eax, 0
	asm_op_reg(&ip, PUSH, EAX);										// push eax
	asm_op_reg(&ip, PUSH, ECX);										// push eax
	asm_op_reg(&ip, PUSH, EDX);										// push eax
	asm_op_reg(&ip, PUSH, EBX);										// push eax
	asm_op_ptr(&ip, CALL, EBP, LLD_OFFSET);							// call [ebp+...] - LdrLoadDll(PathToFile OPTIONAL, Flags, &ModuleFileName, &ModuleHandle);
	// this __stdcall function cleans the stack itself
	
	// todo: test result
	
	if (funcName) {

		// Get init function address
		asm_op_reg_ptr(&ip, LEA, EAX, ESP, SLOT(2));				// lea eax, [rsp+0x28] - &FunctionAddress
		asm_op_reg_val(&ip, MOV, ECX, 0);							// mov ecx, 0
		asm_op_reg_ptr(&ip, LEA, EDX, EBP, NAME_OFFSET);			// lea edx,[rip+...] - FunctionName
		asm_op_reg_ptr(&ip, MOV, EBX, ESP, SLOT(1));				// mov ebx, [rsp+30h] - ModuleHandle

		asm_op_reg(&ip, PUSH, EAX);									// push eax
		asm_op_reg(&ip, PUSH, ECX);									// push ecx
		asm_op_reg(&ip, PUSH, EDX);									// push edx
		asm_op_reg(&ip, PUSH, EBX);									// push ebx
		asm_op_ptr(&ip, CALL, EBP, GPA_OFFSET);						// call [ebp+...] - LdrGetProcedureAddress(ModuleHandle, FunctionName, Oridinal, &FunctionAddress)
		// this __stdcall function cleans the stack itself
		
		// todo: test result
		
		// Call init function
		asm_op_reg_ptr(&ip, MOV, EAX, ESP, SLOT(2));				// mov ecx, [rsp+28h]
		asm_op_reg_val(&ip, MOV, ECX, size);						// mov eax, size
		if (param) {
			asm_op_reg_ptr(&ip, LEA, EDX, EBP, PARAM_OFFSET);		// lea eax,[rip+...]
		}
		else {
			asm_op_reg_val(&ip, MOV, EDX, 0);						// mov rcx, 0
		}
		asm_op_reg(&ip, PUSH, ECX);									// push eax		// rsp +=4
		asm_op_reg(&ip, PUSH, EDX);									// push eax		// rsp +=4
		asm_op_reg(&ip, CALL, EAX);									// call ecx
		asm_op_reg_val(&ip, ADD, ESP, 0x08);						// add rsp, 8h	// rsp -=8
	}

#endif

	asm_op_reg_val(&ip, ADD, ESP, STACK_SIZE);						// add esp, 38h

	// restore all registers
	asm_op(&ip, POPA);
	asm_op(&ip, POPF);

	// resume original controll flow
	//asm_op(&ip, INT3);
	//asm_op_ptr(&ip, JMP, EBP, JMP_OFFSET);						// jmp [ebp]
	asm_op(&ip, RET);												// ret
	//asm_op_reg(&ip, POP, EAX);
	//asm_op_reg(&ip, JMP, EAX);

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
	NtAllocateVirtualMemory(hProcess, &mem, 0, &rs, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!mem)
		return 0;

	if (funcName)
		PrepareAnsiString32(mem, code, NAME_OFFSET, funcName); //RtlInitAnsiString - strcpy(code + NTSTR32_SIZE + NAME_OFFSET, funcName);

	PrepareUnicodeString32(mem, code, KERNEL_OFFSET, L"kernel32.dll");

	PrepareUnicodeString32(mem, code, PATH_OFFSET, dllpath); // RtlInitUnicodeString - wcscpy(code + NTSTR32_SIZE + PATH_OFFSET, dllpath);

	NtWriteVirtualMemory(hProcess, mem, code, mem_size, NULL); // write code + data
	if (param)
		NtWriteVirtualMemory(hProcess, mem + mem_size, (LPVOID)param, size, NULL); // append params

	NtFlushInstructionCache(hProcess, (LPCVOID)mem, mem_size);

	return mem;

}

BOOL hijack_thread_x86(HANDLE hProcess, HANDLE hThread, BOOL bWoW64, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size)
{
	WOW64_CONTEXT context;
	DWORD mem;

	mem = write_shellcode_x86(hProcess, bWoW64, dllpath, funcName, param, size, 0);
	if (!mem)
		return FALSE;
	DWORD64 entry = mem + HEADER_SIZE;

	context.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hThread, (LPCONTEXT)&context);

	UCHAR tmp[5];
	tmp[0] = 0x68;			// push eip
	*((LONG*)&tmp[1]) = context.Eip;
	NtWriteVirtualMemory(hProcess, (LPVOID)(entry), &tmp, 5, NULL);

	//NtWriteVirtualMemory64(hProcess, mem, &context.Rip, sizeof(DWORD64), NULL);

	context.Eip = entry;
	SetThreadContext(hThread, (LPCONTEXT)&context);
	return TRUE;
}

typedef DWORD(WINAPI* pRtlCreateUserThread)(IN HANDLE ProcessHandle, IN PSECURITY_DESCRIPTOR SecurityDescriptor,
	IN BOOL CreateSuspended, IN ULONG StackZeroBits, IN OUT PULONG StackReserved, IN OUT PULONG	StackCommit,
	IN LPVOID StartAddress, IN LPVOID StartParameter, OUT HANDLE ThreadHandle, OUT LPVOID ClientID);

BOOL inject_thread_x86(HANDLE hProcess, BOOL bWoW64, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size)
{
	DWORD mem;

	mem = write_shellcode_x86(hProcess, bWoW64, dllpath, funcName, param, size, 0);
	if (!mem)
		return FALSE;
	DWORD entry = (DWORD64)mem + 4;

	HANDLE hRemoteThread = NULL;
	pRtlCreateUserThread RtlCreateUserThread = NULL;

	RtlCreateUserThread = (pRtlCreateUserThread)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "RtlCreateUserThread");
	if (RtlCreateUserThread == NULL)
		return FALSE;
	DWORD status = RtlCreateUserThread(hProcess, NULL, 0, 0, 0, 0, (LPVOID)entry, NULL, (PHANDLE)&hRemoteThread, NULL);
	if (status < 0)
		return FALSE;

	WaitForSingleObject(hRemoteThread, INFINITE);

	// Note: sonce we wait we could read the param back for bidirectional communication

	CloseHandle(hRemoteThread);

	return TRUE;
}

BOOL hijack_function_x86(HANDLE hProcess, DWORD pTargetFunc, BOOL bWoW64, LPCWSTR dllpath, LPCSTR funcName, const void* param, size_t size)
{
	NTSTATUS status;
	UCHAR code[8];

//	DWORD64 ntdllBase = FindDllBase(hProcess, L"\\SyChpe32\\ntdll.dll");
	//DWORD64 LLW = FindRemoteDllExport(hProcess, ntdllBase, "LdrInitializeThunk");
//	DWORD64 LLW = FindRemoteDllExport(hProcess, ntdllBase, "LdrLoadDll");

	//pTargetFunc = LLW;

	DWORD64 mem = write_shellcode_x86(hProcess, bWoW64, dllpath, funcName, param, size, CODE_OP_RESTORE);
	if (!mem)
		return FALSE;
	DWORD64 entry = mem + HEADER_SIZE;

	// write the target function address to the final jump target location
	status = NtWriteVirtualMemory(hProcess, mem, &pTargetFunc, sizeof(DWORD), NULL);

	UCHAR tmp[5];
	tmp[0] = 0x68;			// push pTargetFunc
	//*((LONG*)&tmp[1]) = pTargetFunc-2;
	*((LONG*)&tmp[1]) = pTargetFunc;
	NtWriteVirtualMemory(hProcess, (LPVOID)(entry), &tmp, 5, NULL);

	// save target function prolog
	DWORD64 backup = mem + BYTES_OFFSET;
	NtReadVirtualMemory(hProcess, pTargetFunc, code, sizeof(code), NULL);
	NtWriteVirtualMemory(hProcess, backup, code, sizeof(code), NULL);

	// memcpy(&inject->RtlFindActCtx_Bytes, LdrCode, 12);

	// make target writable
	void* RegionBase = (void*)pTargetFunc;
	SIZE_T RegionSize = sizeof(code);
	ULONG OldProtect;
	status = NtProtectVirtualMemory(hProcess, &RegionBase, &RegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);

	// write detour to target function
	TYPES ip;
	ip.pB = code;

	//asm_op(&ip, INT3);
	asm_op_reg_val(&ip, MOV, EAX, entry);	// mov eax, entry
	asm_op_reg(&ip, JMP, EAX);			// jmp eax
	//*ip.pB++ = 0xE9;
	//*ip.pL++ = (ULONG)(entry - (pTargetFunc + 5));

	if (ip.pB > code + sizeof(code))
		DebugBreak(); // CODESIZE to small !!!!!!!

	DbgPrint("\n\ndetour:\n");
	for (int i = 0; i < ip.pB - code; i++) {
		DbgPrint("%02x ", code[i]);
		if ((i + 1) % 16 == 0) DbgPrint("\n");
	}
	DbgPrint("\n\n");

	status = NtWriteVirtualMemory(hProcess, pTargetFunc, code, ip.pB - code, NULL);

	NtFlushInstructionCache(hProcess, pTargetFunc, ip.pB - code);

	return TRUE;
}