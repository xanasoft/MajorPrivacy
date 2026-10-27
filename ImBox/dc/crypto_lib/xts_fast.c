/*
    * DiskCryptor crypto_lib - XTS mode and cipher dispatch
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Written from IEEE 1619. Sectors are XTS_SECTOR_SIZE bytes and the tweak
    * for each one is derived by encrypting the sector index with the tweak key,
    * then advanced by multiplication by alpha across the sector.
    *
    * At init time the best available core is selected for AES and for Serpent,
    * weakest tier first, so each later one overwrites the selection only if the
    * hardware and the OS both support it. Twofish has no tiers: its round
    * function is table lookups, which do not vectorise usefully, so the same
    * four-block interleaved core in xts_twofish.c runs on every target.
*/
#include "cl_platform.h"
#include "xts_fast.h"
#include "aes_asm.h"
#include "xts_twofish.h"

#if defined(_M_X64)
 #include <emmintrin.h>
 /* Under EDK2 the AES-NI intrinsics are declared by the host's <emmintrin.h>
    shim; there is no <wmmintrin.h> to include. */
 #if !defined(_UEFI)
  #include <wmmintrin.h>
 #endif
 #if !CL_KERNEL && CL_SEH
  #include <excpt.h>
 #endif
 #include "xts_aes_ni.h"
 #include "xts_serpent_sse2.h"
 #include "xts_serpent_avx.h"
 #include "xts_serpent_avx2.h"
#elif defined(_M_ARM64)
 #include <arm_neon.h>
 #include "xts_aes_ce.h"
 #include "xts_serpent_neon.h"
#endif

static xts_proc aes_selected_encrypt;
static xts_proc aes_selected_decrypt;
static xts_proc serpent_selected_encrypt;
static xts_proc serpent_selected_decrypt;

/* ---- scalar XTS ----------------------------------------------------------- */

/*
 * One sector at a time: derive the tweak, then walk the sector xor-ing it in
 * before and after the block cipher and advancing it by alpha each block.
 */
#define DEF_XTS_PROC(name, tweak_fn, crypt_fn, field)                                          \
static void _stdcall name(const unsigned char *in, unsigned char *out, size_t len,             \
                          unsigned __int64 offset, xts_key *key)                               \
{                                                                                              \
	unsigned __int64 t0, t1, idx[2], tw[2], sector;                                            \
	unsigned __int64 cf;                                                                       \
	unsigned long    i;                                                                        \
                                                                                               \
	sector = offset / XTS_SECTOR_SIZE;                                                         \
	idx[1] = 0;                                                                                \
                                                                                               \
	do {                                                                                       \
		idx[0] = ++sector;                                                                     \
		tweak_fn((const unsigned char*)idx, (unsigned char*)tw, &key->tweak_k.field);          \
		t0 = tw[0]; t1 = tw[1];                                                                \
                                                                                               \
		for (i = 0; i < XTS_BLOCKS_IN_SECTOR; i++)                                             \
		{                                                                                      \
			((unsigned __int64*)out)[0] = ((const unsigned __int64*)in)[0] ^ t0;                \
			((unsigned __int64*)out)[1] = ((const unsigned __int64*)in)[1] ^ t1;                \
                                                                                               \
			crypt_fn(out, out, &key->crypt_k.field);                                           \
                                                                                               \
			((unsigned __int64*)out)[0] ^= t0;                                                 \
			((unsigned __int64*)out)[1] ^= t1;                                                 \
                                                                                               \
			in += XTS_BLOCK_SIZE; out += XTS_BLOCK_SIZE;                                       \
                                                                                               \
			/* t <- t * alpha in GF(2^128) */                                                  \
			cf = (t1 >> 63) * 135;                                                             \
			t1 = (t1 << 1) | (t0 >> 63);                                                       \
			t0 = (t0 << 1) ^ cf;                                                               \
		}                                                                                      \
	} while (len -= XTS_SECTOR_SIZE);                                                          \
}

DEF_XTS_PROC(xts_aes_basic_encrypt, aes256_asm_encrypt, aes256_asm_encrypt, aes)
DEF_XTS_PROC(xts_aes_basic_decrypt, aes256_asm_encrypt, aes256_asm_decrypt, aes)

/* Twofish's four-block interleaved core lives in xts_twofish.c and is the only
   path, on every target. */

DEF_XTS_PROC(xts_serpent_basic_encrypt, serpent256_encrypt, serpent256_encrypt, serpent)
DEF_XTS_PROC(xts_serpent_basic_decrypt, serpent256_encrypt, serpent256_decrypt, serpent)

/* ---- AVX2 entry points ---------------------------------------------------- */

/*
 * A driver must ask the kernel to preserve the upper YMM halves around any VEX
 * code - including VEX-128, which zeroes them - while user mode gets that for
 * free. If the request fails, that call falls back to the SSE2 core rather
 * than corrupting another thread's state.
 */
#if defined(_M_X64) && defined(CL_ENABLE_AVX)

static void _stdcall xts_serpent_avx_enc_guarded(const unsigned char *in, unsigned char *out, size_t len,
                                                 unsigned __int64 offset, xts_key *key)
{
	CL_AVX_STATE;

	if (CL_AVX_ACQUIRE()) {
		xts_serpent_avx_encrypt(in, out, len, offset, key);
		CL_AVX_RELEASE();
	} else {
		xts_serpent_sse2_encrypt(in, out, len, offset, key);
	}
}

static void _stdcall xts_serpent_avx_dec_guarded(const unsigned char *in, unsigned char *out, size_t len,
                                                 unsigned __int64 offset, xts_key *key)
{
	CL_AVX_STATE;

	if (CL_AVX_ACQUIRE()) {
		xts_serpent_avx_decrypt(in, out, len, offset, key);
		CL_AVX_RELEASE();
	} else {
		xts_serpent_sse2_decrypt(in, out, len, offset, key);
	}
}

#endif

#if defined(_M_X64) && defined(CL_ENABLE_AVX2)
static void _stdcall xts_serpent_avx2_enc_guarded(const unsigned char *in, unsigned char *out, size_t len,
                                                  unsigned __int64 offset, xts_key *key)
{
	CL_AVX_STATE;

	if (CL_AVX_ACQUIRE()) {
		xts_serpent_avx2_encrypt(in, out, len, offset, key);
		CL_AVX_RELEASE();
	} else {
		xts_serpent_sse2_encrypt(in, out, len, offset, key);
	}
}

static void _stdcall xts_serpent_avx2_dec_guarded(const unsigned char *in, unsigned char *out, size_t len,
                                                  unsigned __int64 offset, xts_key *key)
{
	CL_AVX_STATE;

	if (CL_AVX_ACQUIRE()) {
		xts_serpent_avx2_decrypt(in, out, len, offset, key);
		CL_AVX_RELEASE();
	} else {
		xts_serpent_sse2_decrypt(in, out, len, offset, key);
	}
}

#endif

#define xts_aes_encrypt     aes_selected_encrypt
#define xts_aes_decrypt     aes_selected_decrypt
#define xts_serpent_encrypt serpent_selected_encrypt
#define xts_serpent_decrypt serpent_selected_decrypt

/* ---- cascades ------------------------------------------------------------- */

static void _stdcall xts_aes_twofish_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	xts_twofish_encrypt(in, out, len, offset, key);
	xts_aes_encrypt(out, out, len, offset, key);
}

static void _stdcall xts_aes_twofish_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	xts_aes_decrypt(in, out, len, offset, key);
	xts_twofish_decrypt(out, out, len, offset, key);
}

static void _stdcall xts_twofish_serpent_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	xts_serpent_encrypt(in, out, len, offset, key);
	xts_twofish_encrypt(out, out, len, offset, key);
}

static void _stdcall xts_twofish_serpent_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	xts_twofish_decrypt(in, out, len, offset, key);
	xts_serpent_decrypt(out, out, len, offset, key);
}

static void _stdcall xts_serpent_aes_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	xts_aes_encrypt(in, out, len, offset, key);
	xts_serpent_encrypt(out, out, len, offset, key);
}

static void _stdcall xts_serpent_aes_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	xts_serpent_decrypt(in, out, len, offset, key);
	xts_aes_decrypt(out, out, len, offset, key);
}

static void _stdcall xts_aes_twofish_serpent_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	xts_serpent_encrypt(in, out, len, offset, key);
	xts_twofish_encrypt(out, out, len, offset, key);
	xts_aes_encrypt(out, out, len, offset, key);
}

static void _stdcall xts_aes_twofish_serpent_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	xts_aes_decrypt(in, out, len, offset, key);
	xts_twofish_decrypt(out, out, len, offset, key);
	xts_serpent_decrypt(out, out, len, offset, key);
}

/* ---- key setup ------------------------------------------------------------ */

int _stdcall xts_set_key(const unsigned char *key, int alg, xts_key *skey)
{
	switch (alg)
	{
		case CF_AES:
			aes256_asm_set_key(key, &skey->crypt_k.aes);
			aes256_asm_set_key(key + XTS_KEY_SIZE, &skey->tweak_k.aes);

			skey->encrypt = xts_aes_encrypt;
			skey->decrypt = xts_aes_decrypt;
		break;
		case CF_TWOFISH:
			twofish256_set_key(key, &skey->crypt_k.twofish);
			twofish256_set_key(key + XTS_KEY_SIZE, &skey->tweak_k.twofish);

			skey->encrypt = xts_twofish_encrypt;
			skey->decrypt = xts_twofish_decrypt;
		break;
		case CF_SERPENT:
			serpent256_set_key(key, &skey->crypt_k.serpent);
			serpent256_set_key(key + XTS_KEY_SIZE, &skey->tweak_k.serpent);

			skey->encrypt = xts_serpent_encrypt;
			skey->decrypt = xts_serpent_decrypt;
		break;
		case CF_AES_TWOFISH:
			twofish256_set_key(key, &skey->crypt_k.twofish);
			aes256_asm_set_key(key + XTS_KEY_SIZE, &skey->crypt_k.aes);
			twofish256_set_key(key + XTS_KEY_SIZE * 2, &skey->tweak_k.twofish);
			aes256_asm_set_key(key + XTS_KEY_SIZE * 3, &skey->tweak_k.aes);

			skey->encrypt = xts_aes_twofish_encrypt;
			skey->decrypt = xts_aes_twofish_decrypt;
		break;
		case CF_TWOFISH_SERPENT:
			serpent256_set_key(key, &skey->crypt_k.serpent);
			twofish256_set_key(key + XTS_KEY_SIZE, &skey->crypt_k.twofish);
			serpent256_set_key(key + XTS_KEY_SIZE * 2, &skey->tweak_k.serpent);
			twofish256_set_key(key + XTS_KEY_SIZE * 3, &skey->tweak_k.twofish);

			skey->encrypt = xts_twofish_serpent_encrypt;
			skey->decrypt = xts_twofish_serpent_decrypt;
		break;
		case CF_SERPENT_AES:
			aes256_asm_set_key(key, &skey->crypt_k.aes);
			serpent256_set_key(key + XTS_KEY_SIZE, &skey->crypt_k.serpent);
			aes256_asm_set_key(key + XTS_KEY_SIZE * 2, &skey->tweak_k.aes);
			serpent256_set_key(key + XTS_KEY_SIZE * 3, &skey->tweak_k.serpent);

			skey->encrypt = xts_serpent_aes_encrypt;
			skey->decrypt = xts_serpent_aes_decrypt;
		break;
		case CF_AES_TWOFISH_SERPENT:
			serpent256_set_key(key, &skey->crypt_k.serpent);
			twofish256_set_key(key + XTS_KEY_SIZE, &skey->crypt_k.twofish);
			aes256_asm_set_key(key + XTS_KEY_SIZE * 2, &skey->crypt_k.aes);
			serpent256_set_key(key + XTS_KEY_SIZE * 3, &skey->tweak_k.serpent);
			twofish256_set_key(key + XTS_KEY_SIZE * 4, &skey->tweak_k.twofish);
			aes256_asm_set_key(key + XTS_KEY_SIZE * 5, &skey->tweak_k.aes);

			skey->encrypt = xts_aes_twofish_serpent_encrypt;
			skey->decrypt = xts_aes_twofish_serpent_decrypt;
		break;
		default:
			return 0;
	}
	return 1;
}

/* ---- core selection ------------------------------------------------------- */

#if defined(_M_ARM64)

int _stdcall xts_aes_ni_available()
{
	return xts_aes_ce_available();
}

void _stdcall xts_init(int hw_crypt)
{
	if (xts_serpent_neon_available() != 0) {
		serpent_selected_encrypt = xts_serpent_neon_encrypt;
		serpent_selected_decrypt = xts_serpent_neon_decrypt;
	} else {
		serpent_selected_encrypt = xts_serpent_basic_encrypt;
		serpent_selected_decrypt = xts_serpent_basic_decrypt;
	}

	if (hw_crypt != 0 && xts_aes_ce_available() != 0) {
		aes_selected_encrypt = xts_aes_ce_encrypt;
		aes_selected_decrypt = xts_aes_ce_decrypt;
		return;
	}
	aes_selected_encrypt = xts_aes_basic_encrypt;
	aes_selected_decrypt = xts_aes_basic_decrypt;
}

#else /* amd64 */

int _declspec(noinline) _stdcall xts_aes_ni_available()
{
	int info[4];

	__cpuid(info, 1);
	if (info[2] & 0x02000000) return 1;          /* CPUID.01H:ECX.AES[25] */

	/*
	 * Some Hyper-V configurations mask the AES bit while the instructions
	 * still work. Only trust that on a confirmed Microsoft hypervisor, and
	 * only after the instruction produces the right answer.
	 */
#if CL_SEH
	if ((info[2] & 0x80000000) == 0) return 0;
	__cpuid(info, 0x40000000);
	if (info[1] != 'rciM' || info[2] != 'foso' || info[3] != 'vH t') return 0;

	__try {
		__m128i enc = _mm_aesenc_si128(_mm_set_epi32(0, 1, 2, 3), _mm_set_epi32(4, 5, 6, 7));
		return enc.m128i_u64[0] == 0x5f77774d4b7b7b54 && enc.m128i_u64[1] == 0x63636367427c7c58;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
#else
	/*
	 * No SEH here (EFI), so the probe cannot be made safe: an unsupported
	 * AESENC would fault with nothing to catch it. Without the CPUID bit,
	 * AES-NI is simply not used.
	 */
	return 0;
#endif
}

void _stdcall xts_init(int hw_crypt)
{
	/*
	 * Serpent tiers, weakest first. SSE2 is architectural on amd64 so it is a
	 * floor, not a candidate; each later tier overwrites the selection only if
	 * the hardware and the OS both support it.
	 */
	serpent_selected_encrypt = xts_serpent_sse2_encrypt;
	serpent_selected_decrypt = xts_serpent_sse2_decrypt;

#ifdef CL_ENABLE_AVX
	if (xts_serpent_avx_available() != 0) {
		serpent_selected_encrypt = xts_serpent_avx_enc_guarded;
		serpent_selected_decrypt = xts_serpent_avx_dec_guarded;
	}
#endif
#ifdef CL_ENABLE_AVX2
	if (xts_serpent_avx2_available() != 0) {
		serpent_selected_encrypt = xts_serpent_avx2_enc_guarded;
		serpent_selected_decrypt = xts_serpent_avx2_dec_guarded;
	}
#endif

	if (hw_crypt != 0 && xts_aes_ni_available() != 0) {
		aes_selected_encrypt = xts_aes_ni_encrypt;
		aes_selected_decrypt = xts_aes_ni_decrypt;
		return;
	}
	aes_selected_encrypt = xts_aes_basic_encrypt;
	aes_selected_decrypt = xts_aes_basic_decrypt;
}

#endif
