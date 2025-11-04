#ifndef MYASM_H
#define MYASM_H
#pragma once

typedef union
{
	PBYTE pB;
	PWORD  pW;
	PDWORD pL;
	PDWORD64 pQ;
} TYPES;

#if defined(MY_X64) || defined(MY_X86)

#define EAX		0x10
#define ECX		0x11
#define EDX		0x12
#define EBX		0x13
#define ESP		0x14
#define EBP		0x15
#define ESI		0x16
#define EDI		0x17

#if defined(MY_X64)

#define R8D		0x18
#define R9D		0x19
#define R10D	0x1A
#define R11D	0x1B
#define R12D	0x1C
#define R13D	0x1D
#define R14D	0x1E
#define R15D	0x1F

#define RAX		0x00
#define RCX		0x01
#define RDX		0x02
#define RBX		0x03
#define RSP		0x04
#define RBP		0x05
#define RSI		0x06
#define RDI		0x07

#define R8		0x08
#define R9		0x09
#define R10		0x0A
#define R11		0x0B
#define R12		0x0C
#define R13		0x0D
#define R14		0x0E
#define R15		0x0F

#define RIP		0x80

#else

#define RIP		0x80
#define RSP		ESP
#define RBP		EBP

#endif



#define IS_32(reg) ((reg & 0x10) != 0) // use only 32 bit's
#define IS_EX(reg) ((reg & 0x08) != 0) // new 64 bit registers

#define IS_EXX(reg) ( IS_32(reg) && !IS_EX(reg))
#define IS_RXD(reg) ( IS_32(reg) &&  IS_EX(reg))
#define IS_RXX(reg) (!IS_32(reg) && !IS_EX(reg))
#define IS_RX(reg)  (!IS_32(reg) &&  IS_EX(reg))


// 32 bit-prefix
#define EXX_EXX		// no prefix
#define RXD_EXX		0x41
#define EXX_RXD		0x44
#define RXD_RXD		0x45

// 64 bit-prefix
#define RXX_RXX		0x48
#define RX_RXX		0x49
#define RXX_RX		0x4C
#define RX_RX		0x4D

// opcodes
#define ADD			0x00	//   00 0000
#define SUB			0x28	//   10 1000
#define CMP			0x38	//   11 1000
#define XOR			0x30	//   11 0000
#define AND			0x20	//   10 0000
#define OR			0x08	//   00 1000

#define MOV			0x88	// 0x89, 0x8A, 0x8B, 0x8C, 0x8E
#define LEA			0x8D

#define PUSHF		0x9C
#define POPF		0x9D

#define PUSHA		0x60
#define POPA		0x61

#define PUSH		0x50
#define POP			0x58


#define NOP			0x90
#define INT3		0xCC
#define RET			0xC3
#define JMP			0x00FF
#define CALL		0x01FF

// offset size
#define FLAG_O8		0x40	// 0100 0000 // offset is 8 bit
#define FLAG_O32	0x80	// 1000 0000 // offset is 32 bit


static void x64_prefix1a(TYPES* ip, BYTE reg1) { // 32bit address to register or register to 32 bit address
#if defined(MY_X64)
	if (IS_EXX(reg1))		; // no prefix
	else if (IS_RXD(reg1))	*ip->pB++ = EXX_RXD;
	else if (IS_RXX(reg1))	*ip->pB++ = RXX_RXX;
	else if (IS_RX(reg1))	*ip->pB++ = RXX_RX;
#endif
}
static void x64_prefix1b(TYPES* ip, BYTE reg1) { // value to register
#if defined(MY_X64)
	if (IS_EXX(reg1))		; // no prefix
	else if (IS_RXD(reg1))	*ip->pB++ = RXD_EXX;
	else if (IS_RXX(reg1))	*ip->pB++ = RXX_RXX;
	else if (IS_RX(reg1))	*ip->pB++ = RX_RXX;
#endif
}
static void x64_prefix1c(TYPES* ip, BYTE reg1) { // 32 bit value to pointer in 64 bit register or call
#if defined(MY_X64)
	if (IS_RXX(reg1))		; // no prefix
	else if (IS_RX(reg1))	*ip->pB++ = RXD_EXX;

	else DebugBreak(); // don't use 32 bit registers for addresses
#endif
}
static void x64_prefix2(TYPES* ip, BYTE reg1, BYTE reg2) {
#if defined(MY_X64)
	if (IS_EXX(reg1) && IS_EXX(reg2)); // no prefix
	else if (IS_RXD(reg1) && IS_EXX(reg2))	*ip->pB++ = RXD_EXX;
	else if (IS_EXX(reg1) && IS_RXD(reg2))	*ip->pB++ = EXX_RXD;
	else if (IS_RXD(reg1) && IS_RXD(reg2))	*ip->pB++ = RXD_RXD;

	else if (IS_RXX(reg1) && IS_RXX(reg2))	*ip->pB++ = RXX_RXX;
	else if (IS_RX(reg1) && IS_RXX(reg2))	*ip->pB++ = RX_RXX;
	else if (IS_RXX(reg1) && IS_RX(reg2))	*ip->pB++ = RXX_RX;
	else if (IS_RX(reg1) && IS_RX(reg2))	*ip->pB++ = RX_RX;

	else DebugBreak(); // invalid combination don't mix 32 and 64 bit
#endif
}


static void asm_op(TYPES* ip, BYTE op) { 
	*ip->pB++ = op;
}

static void asm_op_reg(TYPES* ip, WORD op, BYTE reg) {
	x64_prefix1c(ip, reg);
	if (op == PUSH || op == POP) {
		*ip->pB++ = op | (reg & 0x07);
		return;
	}
	*ip->pB++ = op & 0xFF;
	// op reg
	BYTE flag = ((op & 0xFF00) == 0x0100) ? 0xD0 : 0xE0; // ? 1101 0000 : 1110 0000;
	*ip->pB++ = ((reg & 0x07) | flag);
}

static void asm_op_ptr(TYPES* ip, WORD op, BYTE reg, LONG offset) {
	x64_prefix1c(ip, reg);
#if defined(MY_X86)
	if (op == CALL && reg == RIP) { // special case for x86: call rip+offset
		*ip->pB++ = 0xE8;
		*ip->pL++ = offset - 5;
		return;
	}
#endif
	*ip->pB++ = op & 0xFF;
	BYTE flag = ((op & 0xFF00) == 0x0100) ? 0x10 : 0x20;	// ? 0001 0000 : 0010 0000;
	if (reg == RIP) {

		flag |= 5;
		*ip->pB++ = flag;
		*ip->pL++ = offset - 6;
	}
	else { // op QWORD PTR[reg + #offset]

		// todo 0x80 to 0x7f can be 8 bit with FLAG_O8	// 0100 0000
		if (offset != 0) flag |= FLAG_O32;			// 1000 0000
		else if ((op == CALL || op == JMP) && reg == RBP) flag |= FLAG_O8; // at least 8 bit ofset are mandatory
		*ip->pB++ = ((reg & 0x07) | flag);
		if (reg == RSP) *ip->pB++ = 0x24;
		if (flag & FLAG_O8) *ip->pB++ = offset;
		if (flag & FLAG_O32) *ip->pL++ = offset;
	}
}

static void asm_op_reg_reg(TYPES* ip, WORD op, BYTE dest, BYTE src) {
	x64_prefix2(ip, dest, src);
	*ip->pB++ = op | 0x01;	
	*ip->pB++ = (((src << 3) | (dest & 0x07)) | 0xC0);
}
static void asm_op_reg_val(TYPES* ip, WORD op, BYTE dest, LONG val) {
	x64_prefix1b(ip, dest);
	switch (op) {
	case MOV:
		if (IS_RXX(dest) || IS_RX(dest)) {
			*ip->pB++ = 0xC7;
			*ip->pB++ = ((dest & 0x07) | 0xC0);
		} 
		else
			*ip->pB++ = ((dest & 0x07) | 0xB8);
		break;
	default:
		*ip->pB++ = 0x81;
		*ip->pB++ = ((dest & 0x07) | 0xC0 | op);
	}
	*ip->pL++ = val;
}
static void asm_mov_reg_val64(TYPES* ip, BYTE dest, LONGLONG val) {
	x64_prefix1b(ip, dest);
	*ip->pB++ = ((dest & 0x07) | 0xB8);
	*ip->pQ++ = val;
}
static void asm_op_reg_addr(TYPES* ip, WORD op, BYTE dest, BYTE addr32) {
	x64_prefix1a(ip, dest);
	switch (op) {
	case LEA:
		*ip->pB++ = op;
		break;
	default:
		*ip->pB++ = op | 0x03;
	}
	*ip->pB++ = (((dest & 0x07) << 3) | 0x04);
	*ip->pB++ = 0x25;	
	*ip->pL++ = addr32;
}
static void asm_op_reg_ptr(TYPES* ip, WORD op, BYTE dest, BYTE psrc, LONG offset) {
	x64_prefix2(ip, psrc, dest);
	if(op == LEA)
		*ip->pB++ = op;
	else
		*ip->pB++ = op | 0x03;
	if ((op == MOV || op == LEA) && psrc == RIP) {
		*ip->pB++ = (((dest & 0x07) << 3) | 0x05);
		*ip->pL++ = offset - 7;
		return;
	}
	BYTE flag = 0;
	// todo 0x80 to 0x7f can be 8 bit with FLAG_O8	// 0100 0000
	if (offset != 0) flag |= FLAG_O32;			// 1000 0000
	else if (/*(op == CALL || op == JMP) &&*/ psrc == RBP) flag |= FLAG_O8; // at least 8 bit ofset are mandatory
	*ip->pB++ = (((dest & 0x07) << 3) | (psrc & 0x07) | flag);
	if (psrc == RSP) *ip->pB++ = 0x24;
	if (flag & FLAG_O8) *ip->pB++ = offset;
	if (flag & FLAG_O32) *ip->pL++ = offset;
}
static void asm_op_addr_reg(TYPES* ip, WORD op, BYTE addr32, BYTE src) {
	x64_prefix1a(ip, src);
	*ip->pB++ = op | 0x01;	
	*ip->pB++ = (((src & 0x07) << 3) | 0x04);
	*ip->pB++ = 0x25;	
	*ip->pL++ = addr32;
}
static void asm_op_ptr_reg(TYPES* ip, WORD op, BYTE pdest, LONG offset, BYTE src) {
	x64_prefix2(ip, pdest, src);
	*ip->pB++ = op | 0x01;
	if (op == MOV && pdest == RIP) {
		*ip->pB++ = ((src << 3) | 0x05);
		*ip->pL++ = offset - 6;
		return;
	}
	BYTE flag = 0;
	// todo 0x80 to 0x7f can be 8 bit with FLAG_O8	// 0100 0000
	if (offset != 0) flag |= FLAG_O32;			// 1000 0000
	else if (/*(op == CALL || op == JMP) &&*/ pdest == RBP) flag |= FLAG_O8; // at least 8 bit ofset are mandatory
	*ip->pB++ = (((src & 0x07) << 3) | (pdest & 0x07) | flag);
	if (pdest == RSP) *ip->pB++ = 0x24;
	if (flag & FLAG_O8) *ip->pB++ = offset;
	if (flag & FLAG_O32) *ip->pL++ = offset;
}
static void asm_op_addr_val(TYPES* ip, WORD op, BYTE addr32, LONG val) {
	switch (op) {
	case MOV:
		*ip->pB++ = 0xC7;
		*ip->pB++ = (0x04);
		break;
	default:
		*ip->pB++ = 0x81;
		*ip->pB++ = (op | 0x04);
	}
	*ip->pB++ = 0x25; 
	*ip->pL++ = addr32;
	*ip->pL++ = val;
}
static void asm_op_ptr_val(TYPES* ip, WORD op, BYTE pdest, LONG offset, LONG val) {
	x64_prefix1c(ip, pdest);
	BYTE flag = 0;
	// todo 0x80 to 0x7f can be 8 bit with FLAG_O8	// 0100 0000
	if (offset != 0) flag |= FLAG_O32;			// 1000 0000
	switch (op) {
	case MOV:
		*ip->pB++ = 0xC7;
		*ip->pB++ = ((pdest & 0x07) | flag);
		break;
	default:
		*ip->pB++ = 0x81;
		*ip->pB++ = ((pdest & 0x07) | op | flag);
	}
	if (flag & FLAG_O8) *ip->pB++ = offset;
	if (flag & FLAG_O32) *ip->pL++ = offset;
	*ip->pL++ = val;
}

#endif


#ifdef MY_A64

#define BRK					*ip.pL++ = 0xD43E0000
#define RET					*ip.pL++ = 0xD65F03C0
#define NOP					*ip.pL++ = 0xD503201F

#define SUB_SP_SP(value)	*ip.pL++ = 0xD10003FF | ((((value) >> 2) & 0x00000FFF) << 12)
#define ADD_SP_SP(value)	*ip.pL++ = 0x910003FF | ((((value) >> 2) & 0x00000FFF) << 12)

#define STR_SP(reg, offset)	*ip.pL++ = 0xF90003E0 | (reg & 0x0000001F) | ((((offset) >> 3) & 0x00000FFF) << 10)
#define LDR_SP(reg, offset)	*ip.pL++ = 0xF94003E0 | (reg & 0x0000001F) | ((((offset) >> 3) & 0x00000FFF) << 10)

//#define STR_X(reg, regA)	*ip.pL++ = 0xF9000000 | (reg & 0x0000001F) | ((regA & 0x0000001F) << 5)
#define STR_X(reg, regA, offset) *ip.pL++ = 0xF9000000 | (reg & 0x0000001F) | ((regA & 0x0000001F) << 5) | ((((offset) >> 3) & 0x00000FFF) << 10)

#define MOV(reg, value)		*ip.pL++ = 0xD2800000 | (reg & 0x0000001F) | (((value) & 0x0000FFFF) << 5)

#define ADR(reg, offset)	*ip.pL++ = 0x10000000 | (reg & 0x0000001F) | ((((offset) >> 2) & 0x0007FFFF) << 5) | (((offset) & 0x00000003) << 29) // WTF why?!

#define ADD_SP(reg, value)	*ip.pL++ = 0x910003E0 | (reg & 0x0000001F) | (((value) & 0x00000FFF) << 10)

#define LDR_PC(reg, offset)	*ip.pL++ = 0x58000000 | (reg & 0x0000001F) | ((((offset) >> 1) & 0x000FFFFF) << 4)

#define BRL(reg)			*ip.pL++ = 0xD63F0000 | ((reg & 0x0000001F) << 5)
#define BR(reg)				*ip.pL++ = 0xD61F0000 | ((reg & 0x0000001F) << 5)

#endif

#ifdef MY_A32

// R0-R12
#define SP	13
#define LR	14
#define PC	15

#define TRAP						*ip.pW++ = 0xDEFE
#define NOP							*ip.pW++ = 0xBF00

#define BX(reg)						*ip.pW++ = 0x4700 | ((reg & 0xf) << 3)
#define BLX(reg)					*ip.pW++ = 0x4780 | ((reg & 0xf) << 3)

#define ALIGN_PC(x)					if ((x) & 0x03) NOP;

#define STR(src, dest, offset)		*ip.pL++ = 0x0000F840 | ((src & 0xf) << 28) | (dest & 0xf) | ((offset) >= 0 ? 0x80 | (((offset) & 0xFFF) << 16) : ((-(offset) & 0xFFF) << 16))	// str.w r0, [r0, #0]
#define LDR(dest, src, offset)		*ip.pL++ = 0x0000F850 | ((dest & 0xF) << 28) | (src & 0xf) | ((offset) >= 0 ? 0x80 | (((offset) & 0xFFF) << 16) : ((-(offset) & 0xFFF) << 16))	// ldr.w r0, [r0, #0]

#define SUB(dest, src, value)		*ip.pL++ = 0x0000F1A0 | ((dest & 0xf) << 24) | (src & 0xf) | (((value) & 0x3FF) << 16)
#define ADD(dest, src, value)		*ip.pL++ = 0x0000F100 | ((dest & 0xf) << 24) | (src & 0xf) | (((value) & 0x3FF) << 16)	// add.w dest, src, value

#define ADR(dest, value)			*ip.pL++ = 0x0000F20F | ((dest & 0xf) << 24) | (((value) & 0xFF) << 16)	|  ((((value) >> 8) & 0x07) << 28) // adr dest, pc, value

#define MOV(dest, value)			*ip.pL++ = 0x0000F04F | ((dest & 0xf) << 24) | (((value) & 0xFF) << 16)

#endif



#define NTSTR64_SIZE (sizeof(USHORT) + sizeof(USHORT) + sizeof(ULONG) + sizeof(DWORD64)) // USHORT Length; USHORT MaximumLength; ULOND Padding; DWORD64 Buffer; // 16
#define NTSTR32_SIZE (sizeof(USHORT) + sizeof(USHORT) + sizeof(DWORD)) // USHORT Length; USHORT MaximumLength; DWORD Buffer; // 8


void static PrepareAnsiString64(DWORD64 mem, BYTE* code, DWORD pos, const char* str) {
	USHORT* ptr = (USHORT*)(code + pos);
	ptr[0] = (SHORT)(strlen(str) * sizeof(char));
	ptr[1] = ptr[0] + sizeof(char);
	DWORD64 offset = (((UCHAR*)&ptr[4]) + sizeof(DWORD64)) - code;
	*((DWORD64*)&ptr[4]) = mem + offset;
	strcpy(code + offset, str);
}

void static PrepareUnicodeString64(DWORD64 mem, BYTE* code, DWORD pos, const WCHAR* str) {
	USHORT* ptr = (USHORT*)(code + pos);
	ptr[0] = (SHORT)(wcslen(str) * sizeof(wchar_t));
	ptr[1] = ptr[0] + sizeof(wchar_t);
	DWORD64 offset = (((UCHAR*)&ptr[4]) + sizeof(DWORD64)) - code;
	*((DWORD64*)&ptr[4]) = mem + offset;
	wcscpy((wchar_t*)(code + offset), str);
}


void static PrepareAnsiString32(DWORD32 mem, BYTE* code, DWORD pos, const char* str) {
	USHORT* ptr = (USHORT*)(code + pos);
	ptr[0] = (SHORT)(strlen(str) * sizeof(char));
	ptr[1] = ptr[0] + sizeof(char);
	DWORD32 offset = (((UCHAR*)&ptr[2]) + sizeof(DWORD)) - code;
	*((DWORD32*)&ptr[2]) = mem + offset;
	strcpy(code + offset, str);
}

void static PrepareUnicodeString32(DWORD32 mem, BYTE* code, DWORD pos, const WCHAR* str) {
	USHORT* ptr = (USHORT*)(code + pos);
	ptr[0] = (SHORT)(wcslen(str) * sizeof(wchar_t));
	ptr[1] = ptr[0] + sizeof(wchar_t);
	DWORD32 offset = (((UCHAR*)&ptr[2]) + sizeof(DWORD)) - code;
	*((DWORD32*)&ptr[2]) = mem + offset;
	wcscpy((wchar_t*)(code + offset), str);
}


#endif /* MYASM_H */
