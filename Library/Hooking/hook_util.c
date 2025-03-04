/*
 * Copyright 2020-2022 David Xanatos, xanasoft.com
 */

#include <phnt_windows.h>
#include <phnt.h>

#if defined(_M_ARM64) || defined(_M_ARM64EC)
#include "arm64_asm.h"

void* Hook_GetXipTarget(void* ptr, int mode) 
{
    void* addr = ptr;

	ADRP adrp;
	adrp.OP = ((ULONG*)ptr)[0];
	if (!IS_ADRP(adrp) || adrp.Rd != 16) // adrp x16, #0x4c000 
		return ptr;

    LONG delta = (adrp.immHi << 2 | adrp.immLo) << 12;

    if (mode == 0) // default import jump mode
    {
        LDR ldr;
        ldr.OP = ((ULONG*)ptr)[1];
        if (!IS_LDR(ldr) || ldr.Rn != 16 || ldr.Rt != 16) // ldr  x16, [x16, #0xa8]
            return ptr;

        delta += (ldr.imm12 << ldr.size);

        addr = *((void**)(((UINT_PTR)ptr & ~0xFFF) + delta));
    }
    else if (mode == 1)
    {
        ADD add;
        add.OP = ((ULONG*)ptr)[1];
        if(!IS_ADD(add) || add.Rn != 16 || add.Rd != 16)
            return ptr;

        delta += (add.imm12 << add.shift);

        addr = ((void*)(((UINT_PTR)ptr & ~0xFFF) + delta));
    }
    //else if (mode == 2)
    //{
    //    LDUR ldur;
    //    ldur.OP = ((ULONG*)ptr)[1];
    //    if (!IS_LDUR(ldur) || ldur.Rn != 16 || ldur.Rt != 16) // ldur  x16, [x16, #0xa9]
	//        return ptr;
    // 
    //    delta += (ldr.imm12 << ldr.size);
    //
    //    addr = *((void**)(((UINT_PTR)ptr & ~0xFFF) + delta));
    //}

	BR br;
	br.OP = ((ULONG*)ptr)[2];
	if (!IS_BR(br) || br.rn != 16) // br   x16
		return ptr;

	return addr;
}

void* Hook_GetFFSTargetOld(UCHAR* SourceFunc)
{
    //
    // FFS Sequence: Win10 & Win11 RTM
    // 
    //  48 8B FF            mov         rdi,rdi  
    //  55                  push        rbp  
    //  48 8B EC            mov         rbp,rsp  
    //  5D                  pop         rbp  
    //  90                  nop  
    //  E9 02 48 18 00      jmp         #__GSHandlerCheck_SEH_AMD64+138h (07FFB572B8190h) 
    //

    if (*(UCHAR *)SourceFunc == 0x48 && // mov         rdi,rdi  
        *(USHORT *)((UCHAR *)SourceFunc + 1) == 0xFF8B) 
        SourceFunc = (UCHAR *)SourceFunc + 3;
    if (*(UCHAR *)SourceFunc == 0x55)   // push        rbp
        SourceFunc = (UCHAR *)SourceFunc + 1;
    if (*(UCHAR *)SourceFunc == 0x48 && // mov         rbp,rsp 
        *(USHORT *)((UCHAR *)SourceFunc + 1) == 0xEC8B)
        SourceFunc = (UCHAR *)SourceFunc + 3;
    if (*(UCHAR *)SourceFunc == 0x5D)   // pop         rbp 
        SourceFunc = (UCHAR *)SourceFunc + 1;
    if (*(UCHAR *)SourceFunc == 0x90)   // nop
        SourceFunc = (UCHAR *)SourceFunc + 1;
    if (*(UCHAR *)SourceFunc == 0xE9) {  // jmp        07FFB572B8190h

        LONG diff = *(LONG*)(SourceFunc + 1);
        return (UCHAR*)SourceFunc + 5 + diff;
    }

    return NULL;
}

void* Hook_GetFFSTargetNew(UCHAR* SourceFunc)
{
    //
    // FFS Sequence: Win11 build >= 22621.819 (or 22621.382)
    // 
    //  48 8B C4             mov         rax,rsp  
    //  48 89 58 20          mov         qword ptr [rax+20h],rbx  
    //  55                   push        rbp  
    //  5D                   pop         rbp  
    //  E9 E2 9E 17 00       jmp         #LdrLoadDll (07FFECCB748E0h)  
    //

    if (*(UCHAR *)SourceFunc == 0x48 && // mov         rax,rsp
        *(USHORT *)((UCHAR *)SourceFunc + 1) == 0xC48B) 
        SourceFunc = (UCHAR *)SourceFunc + 3;
    if (*(ULONG *)SourceFunc == 0x20588948) // mov     qword ptr [rax+20h],rbx 
        SourceFunc = (UCHAR *)SourceFunc + 4;
    if (*(UCHAR *)SourceFunc == 0x55)   // push        rbp
        SourceFunc = (UCHAR *)SourceFunc + 1;
    if (*(UCHAR *)SourceFunc == 0x5D)   // pop         rbp 
        SourceFunc = (UCHAR *)SourceFunc + 1;
    if (*(UCHAR *)SourceFunc == 0xE9) {  // jmp        07FFB572B8190h

        LONG diff = *(LONG*)(SourceFunc + 1);
        return (UCHAR*)SourceFunc + 5 + diff;
    }

    return NULL;
}

void* Hook_GetFFSTarget(UCHAR* SourceFunc)
{
    //
    // if we first have a jump to the FFS sequence, follow it
    //

    if (*(UCHAR *)SourceFunc == 0x48 && // rex.W
            *(USHORT *)((UCHAR *)SourceFunc + 1) == 0x25FF) { // jmp QWORD PTR [rip+xx xx xx xx];
        // 48 FF 25 is same as FF 25
        SourceFunc = (UCHAR *)SourceFunc + 1;
    }

    if (*(USHORT *)SourceFunc == 0x25FF) { // jmp QWORD PTR [rip+xx xx xx xx];

        LONG diff = *(LONG *)((ULONG_PTR)SourceFunc + 2);
        ULONG_PTR target = (ULONG_PTR)SourceFunc + 6 + diff;

        SourceFunc = (void *)*(ULONG_PTR *)target;
    }

    //
    // check if the function is a FFS sequence and if so 
    // return the address of the target native function
    //

    void* pTarget = Hook_GetFFSTargetOld(SourceFunc);
    if (!pTarget)
        pTarget = Hook_GetFFSTargetNew(SourceFunc);
    return pTarget;
}

USHORT Hook_GetSysCallIndex(UCHAR* SourceFunc)
{

    //
    // Standard syscall function
    //
    //  4C 8B D1                mov     r10, rcx
    //  B8 19000000             mov     eax, 0x19
    //  F6 04 25 0803FE7F 01    test    byte [0x7ffe0308], 0x1
    //  75 03                   jne     +5
    //
    //  0F 05                   syscall 
    //  C3                      ret
    //
    //  CD 2E                   int     0x2e
    //  C3                      ret    
    //

    USHORT index = -1;

    if (*(UCHAR *)SourceFunc == 0x4C && // mov         r10,rcx  
            *(USHORT *)((UCHAR *)SourceFunc + 1) == 0xD18B) 
        SourceFunc = (UCHAR *)SourceFunc + 3;

    if (*(UCHAR *)SourceFunc == 0xB8) { // mov         eax, 0x55
        
        LONG value = *(LONG*)(SourceFunc + 1);
        if((value & 0xFFFF0000) == 0)
            index = (SHORT)value;
    }

    return index;
}

ULONG Hook_GetSysCallFunc(ULONG* aCode, void** pHandleStubHijack)
{
    //  0: ff4300d1    sub  sp, sp, #0x10
    if (aCode[0] != 0xd10043ff)
        return -1;

    //  1: 10000090    adrp x16, #0xffffffffffffe000       ; data_180165740
    //  2: 10021d91    add  x16, x16, #0x740               ; data_180165740

    //  3: f00300f9    str  x16, [sp]
    if (aCode[3] != 0xf90003f0)
        return -1;

    //  4: e10000d4    svc  #0x07
    SVC svc;
    svc.OP = aCode[4];
    if (!IS_SVC(svc))
        return -1;

    // 5: e90340f9    ldr  x9, [sp]
    if (aCode[5] != 0xf94003e9)
        return -1;

    // 6: 10000090    adrp x16, #0xffffffffffffe000       ; data_180165740
    // 7: 10021d91    add  x16, x16, #0x740               ; data_180165740

    // 8: 100209eb    subs x16, x16, x9
    if (aCode[8] != 0xeb090210)
        return -1;

    // 9: ff430091    add  sp, sp, #0x10
    if (aCode[9] != 0x910043ff)
        return -1;

    //  10: c1eaff54    b.ne #0xffffffffffffe520            ; HandleStubHijack

    B_COND b_cond;
    b_cond.OP = aCode[10];
    if (!IS_B_COND(b_cond) && b_cond.cond != 1)
        return -1;
    if (pHandleStubHijack) {
        LONG offset = b_cond.imm19 << 2;
        if (offset & (1 << 20)) // if this is negative
            offset |= 0xFFF00000; // make it properly negative
        *pHandleStubHijack = (void*)((UINT_PTR)(aCode + 10) + offset);
    }

    //  11: 090080d2    movz x9, #0
    if (aCode[11] != 0xd2800009)
        return -1;

    //  12: c0035fd6    ret  
    if (aCode[12] != 0xd65f03c0)
        return -1;

    return svc.imm16;
}

#endif