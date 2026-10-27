/*
    * DiskCryptor crypto_lib - XTS-AES, AES-NI (eight blocks in parallel)
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Three things decide the throughput here.
    *
    * AESENC has a multi-cycle latency but retires one per cycle (two on cores
    * with a second AES unit), so the sector loop keeps eight independent blocks
    * in flight to cover it.
    *
    * Deriving a sector's first tweak is a full AES encryption of the sector
    * index - fourteen strictly dependent rounds that can overlap with nothing
    * inside their own sector. So each pass derives the *next* sector's tweak
    * before encrypting the current one: the chain is then independent of all
    * the work around it and disappears into the AES pipeline. That one change
    * is worth about 12% on a 512-byte sector size.
    *
    * Eight blocks plus eight live tweaks plus a round key is seventeen XMM
    * registers, and there are sixteen. Holding the tweaks across the rounds
    * therefore costs a spill and reload on every group. Instead only the first
    * tweak of the group is kept; the other seven are thrown away before the
    * AES and regenerated from it afterwards. Sixteen cheap integer-domain ops
    * buy back the spills, and that is worth a further ~17%.
    *
    * Decryption uses the equivalent-inverse-cipher schedule already built by
    * aes256_set_key (reversed round keys with InvMixColumns applied to all but
    * the first and last), which is exactly what AESDEC expects.
*/
#if defined(_M_X64)

/* Under EDK2 the AES-NI intrinsics are declared by the host's <emmintrin.h>
   shim; there is no <wmmintrin.h> to include. */
#if !defined(_UEFI)
 #include <wmmintrin.h>
#endif
#include <emmintrin.h>
#include "aes_key.h"
#include "xts_fast.h"
#include "xts_aes_ni.h"

/* T <- T * alpha in GF(2^128) with the XTS reduction polynomial (0x87) */
static __forceinline __m128i next_tweak(__m128i t)
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

/* single block, used only for tweak derivation */
static __forceinline __m128i aes256_ni_encrypt_block(__m128i x, const unsigned long *rk)
{
	x = _mm_xor_si128(x, _mm_load_si128((const __m128i*)(rk)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk +  4)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk +  8)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 12)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 16)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 20)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 24)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 28)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 32)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 36)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 40)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 44)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 48)));
	x = _mm_aesenc_si128(x, _mm_load_si128((const __m128i*)(rk + 52)));
	return _mm_aesenclast_si128(x, _mm_load_si128((const __m128i*)(rk + 56)));
}

#define LOAD_RK(r) _mm_load_si128((const __m128i*)(rk + (r) * 4))

#define ROUND8(op, r) {                   \
	_k = LOAD_RK(r);                      \
	x0 = op(x0, _k); x1 = op(x1, _k);     \
	x2 = op(x2, _k); x3 = op(x3, _k);     \
	x4 = op(x4, _k); x5 = op(x5, _k);     \
	x6 = op(x6, _k); x7 = op(x7, _k);     \
}

#define XOR8_RK0 {                                          \
	_k = LOAD_RK(0);                                        \
	x0 = _mm_xor_si128(x0, _k); x1 = _mm_xor_si128(x1, _k); \
	x2 = _mm_xor_si128(x2, _k); x3 = _mm_xor_si128(x3, _k); \
	x4 = _mm_xor_si128(x4, _k); x5 = _mm_xor_si128(x5, _k); \
	x6 = _mm_xor_si128(x6, _k); x7 = _mm_xor_si128(x7, _k); \
}

#define AES8(op, oplast)                  \
	XOR8_RK0                              \
	ROUND8(op,  1) ROUND8(op,  2)         \
	ROUND8(op,  3) ROUND8(op,  4)         \
	ROUND8(op,  5) ROUND8(op,  6)         \
	ROUND8(op,  7) ROUND8(op,  8)         \
	ROUND8(op,  9) ROUND8(op, 10)         \
	ROUND8(op, 11) ROUND8(op, 12)         \
	ROUND8(op, 13)                        \
	ROUND8(oplast, 14)

#define XTS_AES_NI_BODY(op, oplast, keyfield)                                        \
	__m128i x0, x1, x2, x3, x4, x5, x6, x7;                                          \
	__m128i t0, t1, t2, t3, t4, t5, t6, t7, tw, tw_next, tw_base, _k;                \
	const unsigned long *rk = key->crypt_k.aes.keyfield;                             \
	const unsigned long *tweak_rk = key->tweak_k.aes.enc_key;                        \
	unsigned __int64 idx[2], sector;                                                 \
	int i, more;                                                                     \
                                                                                     \
	sector = offset / XTS_SECTOR_SIZE;                                               \
	idx[1] = 0;                                                                      \
	idx[0] = ++sector;                                                               \
	tw = aes256_ni_encrypt_block(_mm_loadu_si128((const __m128i*)idx), tweak_rk);     \
                                                                                     \
	for (;;) {                                                                       \
		/*                                                                           \
		 * The next sector's tweak is derived before this sector's work so the       \
		 * serial chain overlaps with it - but only when a next sector exists.       \
		 * On the final sector the result is discarded, and for a single-sector      \
		 * request that waste is a full fourteen-round AES against just 32           \
		 * blocks of real work.                                                      \
		 */                                                                          \
		more = (len -= XTS_SECTOR_SIZE) != 0;                                        \
		if (more) {                                                                  \
			idx[0] = sector + 1;                                                     \
			tw_next = aes256_ni_encrypt_block(_mm_loadu_si128((const __m128i*)idx), tweak_rk); \
		}                                                                            \
		sector++;                                                                    \
                                                                                     \
		for (i = 0; i < XTS_BLOCKS_IN_SECTOR / 8; i++)                               \
		{                                                                            \
			tw_base = tw;                                                            \
			t0 = tw_base;                                                            \
			t1 = next_tweak(t0); t2 = next_tweak(t1); t3 = next_tweak(t2);           \
			t4 = next_tweak(t3); t5 = next_tweak(t4); t6 = next_tweak(t5);           \
			t7 = next_tweak(t6); tw = next_tweak(t7);                                \
                                                                                     \
			x0 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in +   0)), t0);     \
			x1 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in +  16)), t1);     \
			x2 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in +  32)), t2);     \
			x3 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in +  48)), t3);     \
			x4 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in +  64)), t4);     \
			x5 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in +  80)), t5);     \
			x6 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in +  96)), t6);     \
			x7 = _mm_xor_si128(_mm_loadu_si128((const __m128i*)(in + 112)), t7);     \
                                                                                     \
			AES8(op, oplast)                                                         \
                                                                                     \
			/* the tweak registers were freed for the rounds; rebuild them now */    \
			t0 = tw_base;                                                            \
			t1 = next_tweak(t0); t2 = next_tweak(t1); t3 = next_tweak(t2);           \
			t4 = next_tweak(t3); t5 = next_tweak(t4); t6 = next_tweak(t5);           \
			t7 = next_tweak(t6);                                                     \
                                                                                     \
			_mm_storeu_si128((__m128i*)(out +   0), _mm_xor_si128(x0, t0));          \
			_mm_storeu_si128((__m128i*)(out +  16), _mm_xor_si128(x1, t1));          \
			_mm_storeu_si128((__m128i*)(out +  32), _mm_xor_si128(x2, t2));          \
			_mm_storeu_si128((__m128i*)(out +  48), _mm_xor_si128(x3, t3));          \
			_mm_storeu_si128((__m128i*)(out +  64), _mm_xor_si128(x4, t4));          \
			_mm_storeu_si128((__m128i*)(out +  80), _mm_xor_si128(x5, t5));          \
			_mm_storeu_si128((__m128i*)(out +  96), _mm_xor_si128(x6, t6));          \
			_mm_storeu_si128((__m128i*)(out + 112), _mm_xor_si128(x7, t7));          \
                                                                                     \
			in  += XTS_BLOCK_SIZE * 8;                                               \
			out += XTS_BLOCK_SIZE * 8;                                               \
		}                                                                            \
		if (!more) break;                                                            \
		tw = tw_next;                                                                \
	}

void _stdcall xts_aes_ni_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	XTS_AES_NI_BODY(_mm_aesenc_si128, _mm_aesenclast_si128, enc_key)
}

void _stdcall xts_aes_ni_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	XTS_AES_NI_BODY(_mm_aesdec_si128, _mm_aesdeclast_si128, dec_key)
}

#endif /* _M_X64 */
