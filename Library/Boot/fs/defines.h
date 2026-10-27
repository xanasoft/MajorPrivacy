#pragma once
/*
 * The part of DiskCryptor's DCrypt\include\defines.h that fat_sup.c, iso_sup.c
 * and iso_fs.h are written against - the fixed width types and a handful of
 * cast, byte order and copy macros.
 *
 * Only what those three files actually use is here. The real header also
 * carries driver, boot loader and interlocked-intrinsic material that has no
 * business in a user mode library, which is the reason for a shim rather than
 * a copy. See dcconst.h beside this file for the wider note.
 */

#include <stdlib.h>		/* _byteswap_* */
#include <stddef.h>		/* ptrdiff_t, size_t */
#include <intrin.h>
#include <string.h>

typedef unsigned __int64	u64;
typedef unsigned long		u32;
typedef unsigned short		u16;
typedef unsigned char		u8;

typedef __int64				s64;
typedef long				s32;
typedef short				s16;
typedef char				s8;

typedef unsigned char		byte;

/* pointer casts */
#define pv(_x)   ((void*)(_x))
#define p8(_x)   ((u8*)(_x))
#define p16(_x)  ((u16*)(_x))
#define p32(_x)  ((u32*)(_x))
#define p64(_x)  ((u64*)(_x))

/* byte order - ISO 9660 stores several fields in both endiannesses */
#define BE16(x)  _byteswap_ushort(x)
#define BE32(x)  _byteswap_ulong(x)
#define BE64(x)  _byteswap_uint64(x)

#ifndef countof
#define countof(_array)       ( sizeof(_array) / sizeof((_array)[0]) )
#endif

#define addof(_a, _o)         ( pv(p8(_a) + (ptrdiff_t)(_o)) )
#define _align(_size, _align) ( ((_size) + ((_align) - 1)) & ~((_align) - 1) )

/*
 * DiskCryptor's size optimised copy. __movsb is x86 only, so on ARM64 it
 * becomes memcpy - the same substitution the real header makes.
 */
#if defined(_M_ARM64)
 #define mincpy(a,b,c) memcpy(pv(a), pv(b), (size_t)(c))
#else
 #define mincpy(a,b,c) __movsb(pv(a), pv(b), (size_t)(c))
#endif
