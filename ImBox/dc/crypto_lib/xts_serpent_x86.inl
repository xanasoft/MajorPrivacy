/*
    * DiskCryptor crypto_lib - XTS-Serpent, 128-bit x86 core (four blocks)
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Serpent is a bitsliced cipher, so four 128-bit blocks transpose into four
    * 128-bit vectors holding one bitslice word each, and the whole 32-round
    * schedule then runs with no lane-crossing at all. The round structure comes
    * from serpent_simd.inl and the S-boxes from serpent_sboxes.h, shared with
    * the scalar, AVX2 and NEON cores.
    *
    * This body is compiled twice: once as plain SSE2, and once with /arch:AVX so
    * the same intrinsics come out VEX-encoded. VEX's three-operand form removes
    * the register-copy moves that two-operand SSE needs, which is worth a few
    * percent on parts that have AVX but not AVX2. The includer sets:
    *
    *   CL_SP_PREFIX   symbol prefix, e.g. xts_serpent_sse2_
*/
#ifndef CL_SP_PREFIX
 #error xts_serpent_x86.inl requires CL_SP_PREFIX
#endif

#include <emmintrin.h>
#include "serpent.h"
#include "xts_fast.h"

#define CL_CAT2(a, b) a##b
#define CL_CAT(a, b)  CL_CAT2(a, b)
#define SP_FN(n)      CL_CAT(CL_SP_PREFIX, n)

#define SB_T         __m128i
#define SB_XOR(a, b) _mm_xor_si128(a, b)
#define SB_AND(a, b) _mm_and_si128(a, b)
#define SB_OR(a, b)  _mm_or_si128(a, b)
#define SB_NOT(a)    _mm_xor_si128(a, _mm_cmpeq_epi32(a, a))
#define SB_ROL(a, n) _mm_or_si128(_mm_slli_epi32(a, n), _mm_srli_epi32(a, 32 - (n)))
#define SB_SHL(a, n) _mm_slli_epi32(a, n)
#define SB_SET1(v)   _mm_set1_epi32((int)(v))

#include "serpent_sboxes.h"
#include "serpent_simd.inl"

/* 4x4 transpose of 32-bit words; its own inverse for this layout */
#define transpose4(B0, B1, B2, B3) {              \
	__m128i _t0 = _mm_unpacklo_epi32(B0, B1);     \
	__m128i _t1 = _mm_unpacklo_epi32(B2, B3);     \
	__m128i _t2 = _mm_unpackhi_epi32(B0, B1);     \
	__m128i _t3 = _mm_unpackhi_epi32(B2, B3);     \
	B0 = _mm_unpacklo_epi64(_t0, _t1);            \
	B1 = _mm_unpackhi_epi64(_t0, _t1);            \
	B2 = _mm_unpacklo_epi64(_t2, _t3);            \
	B3 = _mm_unpackhi_epi64(_t2, _t3);            \
}

/* T <- T * alpha in GF(2^128) with the XTS reduction polynomial (0x87) */
static __forceinline __m128i SP_FN(next_tweak)(__m128i t)
{
	const __m128i poly = _mm_set_epi32(0, 0, 0, 0x87);
	__m128i sign, carry, lo, hi;

	/* all-ones in every lane when bit 127 is set */
	sign  = _mm_srai_epi32(_mm_shuffle_epi32(t, _MM_SHUFFLE(3, 3, 3, 3)), 31);
	carry = _mm_and_si128(sign, poly);

	lo = _mm_slli_epi64(t, 1);
	hi = _mm_slli_si128(_mm_srli_epi64(t, 63), 8);   /* carry out of the low half */

	return _mm_xor_si128(_mm_or_si128(lo, hi), carry);
}

#define next_tweak SP_FN(next_tweak)

#define XTS_X86_BODY(CRYPT)                                                       \
	__m128i t0, t1, t2, t3;                                                       \
	__m128i b0, b1, b2, b3;                                                       \
	__m128i tw, tw_next;                                                          \
	unsigned __int64 idx[2];                                                      \
	unsigned __int64 sector;                                                      \
	int i, more;                                                                  \
                                                                                  \
	sector = offset / XTS_SECTOR_SIZE;                                            \
	idx[1] = 0;                                                                   \
	idx[0] = ++sector;                                                            \
	serpent256_encrypt((const unsigned char*)idx,                                 \
	                   (unsigned char*)&tw, &key->tweak_k.serpent);               \
                                                                                  \
	for (;;) {                                                                    \
		/*                                                                        \
		 * The next sector's tweak is independent of this sector, so start        \
		 * it now - but only when a next sector exists. On the final one the      \
		 * result is discarded, and a full 32-round Serpent thrown away has       \
		 * nothing to amortise against on a single-sector request.                \
		 */                                                                       \
		more = (len -= XTS_SECTOR_SIZE) != 0;                                     \
		if (more) {                                                               \
			idx[0] = sector + 1;                                                  \
			serpent256_encrypt((const unsigned char*)idx,                         \
			                   (unsigned char*)&tw_next, &key->tweak_k.serpent);  \
		}                                                                         \
		sector++;                                                                 \
                                                                                  \
		for (i = 0; i < XTS_BLOCKS_IN_SECTOR / 4; i++)                            \
		{                                                                         \
			t0 = tw;                                                              \
			t1 = next_tweak(t0);                                                  \
			t2 = next_tweak(t1);                                                  \
			t3 = next_tweak(t2);                                                  \
			tw = next_tweak(t3);                                                  \
                                                                                  \
			b0 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in +  0)), t0);   \
			b1 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in + 16)), t1);   \
			b2 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in + 32)), t2);   \
			b3 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in + 48)), t3);   \
                                                                                  \
			transpose4(b0, b1, b2, b3)                                            \
			CRYPT(b0, b1, b2, b3, &key->crypt_k.serpent)                          \
			transpose4(b0, b1, b2, b3)                                            \
                                                                                  \
			_mm_storeu_si128((__m128i*)(out +  0), _mm_xor_si128(b0, t0));        \
			_mm_storeu_si128((__m128i*)(out + 16), _mm_xor_si128(b1, t1));        \
			_mm_storeu_si128((__m128i*)(out + 32), _mm_xor_si128(b2, t2));        \
			_mm_storeu_si128((__m128i*)(out + 48), _mm_xor_si128(b3, t3));        \
                                                                                  \
			in  += XTS_BLOCK_SIZE * 4;                                            \
			out += XTS_BLOCK_SIZE * 4;                                            \
		}                                                                         \
		if (!more) break;                                                         \
		tw = tw_next;                                                             \
	}

void _stdcall SP_FN(encrypt)(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	XTS_X86_BODY(SERPENT_SIMD_ENCRYPT)
}

void _stdcall SP_FN(decrypt)(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	XTS_X86_BODY(SERPENT_SIMD_DECRYPT)
}
