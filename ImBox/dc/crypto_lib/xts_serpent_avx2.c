/*
    * DiskCryptor crypto_lib - XTS-Serpent, AVX2 (eight blocks in parallel)
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Same construction as the SSE2 core, widened to 256-bit vectors. Blocks
    * 0..3 occupy the low 128-bit lane of each vector and blocks 4..7 the high
    * lane, which lets the transpose keep using the lane-local unpack
    * instructions - no cross-lane shuffles anywhere in the round schedule.
    *
    * This file is built only when CL_ENABLE_AVX2 is defined, and then needs
    * /arch:AVX2. MSVC rejects /arch:AVX2 together with /kernel, so the driver
    * build leaves it out and runs the SSE2 core instead. Callers must confirm AVX2 is
    * both present and enabled by the OS (xts_serpent_avx2_available), and in
    * kernel mode must bracket calls with KeSaveExtendedProcessorState.
*/
#if defined(_M_X64) && defined(CL_ENABLE_AVX2)

#include "cl_platform.h"
#include "cl_cpu.h"
#include <immintrin.h>
#include "serpent.h"
#include "xts_fast.h"
#include "xts_serpent_avx2.h"

#define SB_T         __m256i
#define SB_XOR(a, b) _mm256_xor_si256(a, b)
#define SB_AND(a, b) _mm256_and_si256(a, b)
#define SB_OR(a, b)  _mm256_or_si256(a, b)
#define SB_NOT(a)    _mm256_xor_si256(a, _mm256_cmpeq_epi32(a, a))
#define SB_ROL(a, n) _mm256_or_si256(_mm256_slli_epi32(a, n), _mm256_srli_epi32(a, 32 - (n)))
#define SB_SHL(a, n) _mm256_slli_epi32(a, n)
#define SB_SET1(v)   _mm256_set1_epi32((int)(v))

#include "serpent_sboxes.h"
#include "serpent_simd.inl"

#define M256(lo, hi) _mm256_inserti128_si256(_mm256_castsi128_si256(lo), hi, 1)

/* lane-local 4x4 transpose of 32-bit words */
#define transpose8(B0, B1, B2, B3) {                  \
	__m256i _t0 = _mm256_unpacklo_epi32(B0, B1);      \
	__m256i _t1 = _mm256_unpacklo_epi32(B2, B3);      \
	__m256i _t2 = _mm256_unpackhi_epi32(B0, B1);      \
	__m256i _t3 = _mm256_unpackhi_epi32(B2, B3);      \
	B0 = _mm256_unpacklo_epi64(_t0, _t1);             \
	B1 = _mm256_unpackhi_epi64(_t0, _t1);             \
	B2 = _mm256_unpacklo_epi64(_t2, _t3);             \
	B3 = _mm256_unpackhi_epi64(_t2, _t3);             \
}

static __forceinline __m128i next_tweak(__m128i t)
{
	const __m128i poly = _mm_set_epi32(0, 0, 0, 0x87);
	__m128i sign, carry, lo, hi;

	sign  = _mm_srai_epi32(_mm_shuffle_epi32(t, _MM_SHUFFLE(3, 3, 3, 3)), 31);
	carry = _mm_and_si128(sign, poly);

	lo = _mm_slli_epi64(t, 1);
	hi = _mm_slli_si128(_mm_srli_epi64(t, 63), 8);

	return _mm_xor_si128(_mm_or_si128(lo, hi), carry);
}

int _stdcall xts_serpent_avx2_available(void)
{
	/*
	 * The probe itself lives in cl_cpu.c, which is compiled without
	 * /arch:AVX2. Asking "can this CPU run AVX2" from a translation unit the
	 * compiler may emit AVX2 into is a trap worth avoiding.
	 */
	return cl_cpu_has_avx2();
}

#define XTS_AVX2_BODY(CRYPT)                                                          \
	__m256i t0, t1, t2, t3;                                                           \
	__m256i b0, b1, b2, b3;                                                           \
	__m128i tk[8], tw, tw_next;                                                       \
	unsigned __int64 idx[2];                                                          \
	unsigned __int64 sector;                                                          \
	int i, j, more;                                                                   \
                                                                                      \
	sector = offset / XTS_SECTOR_SIZE;                                                \
	idx[1] = 0;                                                                       \
	idx[0] = ++sector;                                                                \
	serpent256_encrypt((const unsigned char*)idx,                                     \
	                   (unsigned char*)&tw, &key->tweak_k.serpent);                   \
                                                                                      \
	for (;;) {                                                                        \
		/*                                                                            \
		 * The next sector's tweak is independent of this sector, so start            \
		 * it now - but only when a next sector exists. On the final one the          \
		 * result is discarded, and a full 32-round Serpent thrown away has           \
		 * nothing to amortise against on a single-sector request.                    \
		 */                                                                           \
		more = (len -= XTS_SECTOR_SIZE) != 0;                                         \
		if (more) {                                                                   \
			idx[0] = sector + 1;                                                      \
			serpent256_encrypt((const unsigned char*)idx,                             \
			                   (unsigned char*)&tw_next, &key->tweak_k.serpent);      \
		}                                                                             \
		sector++;                                                                     \
                                                                                      \
		for (i = 0; i < XTS_BLOCKS_IN_SECTOR / 8; i++)                                \
		{                                                                             \
			tk[0] = tw;                                                               \
			for (j = 1; j < 8; j++) tk[j] = next_tweak(tk[j - 1]);                    \
			tw = next_tweak(tk[7]);                                                   \
                                                                                      \
			t0 = M256(tk[0], tk[4]);                                                  \
			t1 = M256(tk[1], tk[5]);                                                  \
			t2 = M256(tk[2], tk[6]);                                                  \
			t3 = M256(tk[3], tk[7]);                                                  \
                                                                                      \
			b0 = _mm256_xor_si256(M256(_mm_loadu_si128((const __m128i*)(in +  0)),     \
			                           _mm_loadu_si128((const __m128i*)(in + 64))), t0); \
			b1 = _mm256_xor_si256(M256(_mm_loadu_si128((const __m128i*)(in + 16)),     \
			                           _mm_loadu_si128((const __m128i*)(in + 80))), t1); \
			b2 = _mm256_xor_si256(M256(_mm_loadu_si128((const __m128i*)(in + 32)),     \
			                           _mm_loadu_si128((const __m128i*)(in + 96))), t2); \
			b3 = _mm256_xor_si256(M256(_mm_loadu_si128((const __m128i*)(in + 48)),     \
			                           _mm_loadu_si128((const __m128i*)(in + 112))), t3); \
                                                                                      \
			transpose8(b0, b1, b2, b3)                                                \
			CRYPT(b0, b1, b2, b3, &key->crypt_k.serpent)                              \
			transpose8(b0, b1, b2, b3)                                                \
                                                                                      \
			b0 = _mm256_xor_si256(b0, t0);                                            \
			b1 = _mm256_xor_si256(b1, t1);                                            \
			b2 = _mm256_xor_si256(b2, t2);                                            \
			b3 = _mm256_xor_si256(b3, t3);                                            \
                                                                                      \
			_mm_storeu_si128((__m128i*)(out +   0), _mm256_castsi256_si128(b0));      \
			_mm_storeu_si128((__m128i*)(out +  16), _mm256_castsi256_si128(b1));      \
			_mm_storeu_si128((__m128i*)(out +  32), _mm256_castsi256_si128(b2));      \
			_mm_storeu_si128((__m128i*)(out +  48), _mm256_castsi256_si128(b3));      \
			_mm_storeu_si128((__m128i*)(out +  64), _mm256_extracti128_si256(b0, 1)); \
			_mm_storeu_si128((__m128i*)(out +  80), _mm256_extracti128_si256(b1, 1)); \
			_mm_storeu_si128((__m128i*)(out +  96), _mm256_extracti128_si256(b2, 1)); \
			_mm_storeu_si128((__m128i*)(out + 112), _mm256_extracti128_si256(b3, 1)); \
                                                                                      \
			in  += XTS_BLOCK_SIZE * 8;                                                \
			out += XTS_BLOCK_SIZE * 8;                                                \
		}                                                                             \
		if (!more) break;                                                             \
		tw = tw_next;                                                                 \
	}                                                                                 \
	_mm256_zeroupper();

void _stdcall xts_serpent_avx2_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	XTS_AVX2_BODY(SERPENT_SIMD_ENCRYPT)
}

void _stdcall xts_serpent_avx2_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	XTS_AVX2_BODY(SERPENT_SIMD_DECRYPT)
}

#endif /* _M_X64 && CL_ENABLE_AVX2 */

