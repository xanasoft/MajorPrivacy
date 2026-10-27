/*
    * DiskCryptor crypto_lib - XTS-Serpent, ARM64 NEON (four blocks in parallel)
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Structurally identical to the SSE2 core: four blocks transpose into four
    * vectors of bitslice words, and the round schedule from serpent_simd.inl
    * runs entirely lane-local. NEON is architectural on ARM64, so there is no
    * runtime check to make and no fallback to select.
*/
#if defined(_M_ARM64)

#include <arm_neon.h>
#include "serpent.h"
#include "xts_fast.h"
#include "xts_serpent_neon.h"

#define SB_T         uint32x4_t
#define SB_XOR(a, b) veorq_u32(a, b)
#define SB_AND(a, b) vandq_u32(a, b)
#define SB_OR(a, b)  vorrq_u32(a, b)
#define SB_NOT(a)    vmvnq_u32(a)
#define SB_ROL(a, n) vorrq_u32(vshlq_n_u32(a, n), vshrq_n_u32(a, 32 - (n)))
#define SB_SHL(a, n) vshlq_n_u32(a, n)
#define SB_SET1(v)   vdupq_n_u32((unsigned int)(v))

#include "serpent_sboxes.h"
#include "serpent_simd.inl"

/* 4x4 transpose of 32-bit words; its own inverse for this layout */
#define transpose4(B0, B1, B2, B3) {                                                   \
	uint32x4x2_t _t01 = vtrnq_u32(B0, B1);                                             \
	uint32x4x2_t _t23 = vtrnq_u32(B2, B3);                                             \
	uint64x2_t   _u0 = vreinterpretq_u64_u32(_t01.val[0]);                             \
	uint64x2_t   _u1 = vreinterpretq_u64_u32(_t01.val[1]);                             \
	uint64x2_t   _u2 = vreinterpretq_u64_u32(_t23.val[0]);                             \
	uint64x2_t   _u3 = vreinterpretq_u64_u32(_t23.val[1]);                             \
	B0 = vreinterpretq_u32_u64(vcombine_u64(vget_low_u64(_u0),  vget_low_u64(_u2)));   \
	B1 = vreinterpretq_u32_u64(vcombine_u64(vget_low_u64(_u1),  vget_low_u64(_u3)));   \
	B2 = vreinterpretq_u32_u64(vcombine_u64(vget_high_u64(_u0), vget_high_u64(_u2)));  \
	B3 = vreinterpretq_u32_u64(vcombine_u64(vget_high_u64(_u1), vget_high_u64(_u3)));  \
}

int _stdcall xts_serpent_neon_available(void)
{
	return 1;   /* NEON is architectural on ARM64 */
}

/*
 * T <- T * alpha in GF(2^128) with the XTS reduction polynomial (0x87), on the
 * little-endian halves lo = bytes 0-7 and hi = bytes 8-15.
 *
 * On general-purpose registers rather than NEON, as in xts_aes_ce.c: the tweak
 * chain is strictly serial, and a NEON version needs the carry out of bit 127
 * as a scalar - a NEON -> GPR -> NEON round trip per block inside that chain.
 * Serpent's own rounds dominate here, so the gain is small, but it is free.
 */
#define XTS_MUL_X(lo, hi) {                                                         \
	unsigned __int64 _c = (unsigned __int64)((__int64)(hi) >> 63) & 0x87;           \
	(hi) = ((hi) << 1) | ((lo) >> 63);                                              \
	(lo) = ((lo) << 1) ^ _c;                                                        \
}
#define XTS_TWEAK_VEC(lo, hi)                                                       \
	vreinterpretq_u32_u64(vcombine_u64(vcreate_u64(lo), vcreate_u64(hi)))

#define XTS_NEON_BODY(CRYPT)                                                        \
	uint32x4_t t0, t1, t2, t3;                                                      \
	uint32x4_t b0, b1, b2, b3;                                                      \
	unsigned __int64 idx[2], tw[2], tw_next[2];                                     \
	unsigned __int64 lo, hi;                                                        \
	unsigned __int64 sector;                                                        \
	int i, more;                                                                    \
                                                                                    \
	sector = offset / XTS_SECTOR_SIZE;                                              \
	idx[1] = 0;                                                                     \
	idx[0] = ++sector;                                                              \
	serpent256_encrypt((const unsigned char*)idx,                                   \
	                   (unsigned char*)tw, &key->tweak_k.serpent);                  \
	lo = tw[0];                                                                     \
	hi = tw[1];                                                                     \
                                                                                    \
	for (;;) {                                                                      \
		/*                                                                          \
		 * Deriving a sector's tweak is a full 32-round Serpent with a              \
		 * strictly serial dependency chain and nothing inside its own              \
		 * sector to overlap with, so derive the next sector's here, ahead          \
		 * of this sector's bulk work - but only when a next sector exists,         \
		 * since on the last one the result is simply discarded.                    \
		 */                                                                         \
		more = (len -= XTS_SECTOR_SIZE) != 0;                                       \
		if (more) {                                                                 \
			idx[0] = sector + 1;                                                    \
			serpent256_encrypt((const unsigned char*)idx,                           \
			                   (unsigned char*)tw_next, &key->tweak_k.serpent);     \
		}                                                                           \
		sector++;                                                                   \
                                                                                    \
		for (i = 0; i < XTS_BLOCKS_IN_SECTOR / 4; i++)                              \
		{                                                                           \
			t0 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);                          \
			t1 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);                          \
			t2 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);                          \
			t3 = XTS_TWEAK_VEC(lo, hi); XTS_MUL_X(lo, hi);                          \
                                                                                    \
			b0 = veorq_u32(vld1q_u32((const unsigned int*)(in +  0)), t0);          \
			b1 = veorq_u32(vld1q_u32((const unsigned int*)(in + 16)), t1);          \
			b2 = veorq_u32(vld1q_u32((const unsigned int*)(in + 32)), t2);          \
			b3 = veorq_u32(vld1q_u32((const unsigned int*)(in + 48)), t3);          \
                                                                                    \
			transpose4(b0, b1, b2, b3)                                              \
			CRYPT(b0, b1, b2, b3, &key->crypt_k.serpent)                            \
			transpose4(b0, b1, b2, b3)                                              \
                                                                                    \
			vst1q_u32((unsigned int*)(out +  0), veorq_u32(b0, t0));                \
			vst1q_u32((unsigned int*)(out + 16), veorq_u32(b1, t1));                \
			vst1q_u32((unsigned int*)(out + 32), veorq_u32(b2, t2));                \
			vst1q_u32((unsigned int*)(out + 48), veorq_u32(b3, t3));                \
                                                                                    \
			in  += XTS_BLOCK_SIZE * 4;                                              \
			out += XTS_BLOCK_SIZE * 4;                                              \
		}                                                                           \
		if (!more) break;                                                           \
		lo = tw_next[0];                                                            \
		hi = tw_next[1];                                                            \
	}

void _stdcall xts_serpent_neon_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	XTS_NEON_BODY(SERPENT_SIMD_ENCRYPT)
}

void _stdcall xts_serpent_neon_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	XTS_NEON_BODY(SERPENT_SIMD_DECRYPT)
}

#endif /* _M_ARM64 */
