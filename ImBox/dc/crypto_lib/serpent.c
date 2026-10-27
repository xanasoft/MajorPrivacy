/*
    * DiskCryptor crypto_lib - Serpent-256, scalar reference/tweak path
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Written from the Serpent specification (Anderson, Biham, Knudsen, "Serpent:
    * A Proposal for the Advanced Encryption Standard") using the bitsliced gate
    * sequences in serpent_sboxes.h. Fully unrolled: the round structure is
    * emitted inline so the compiler keeps the four bitslice words in registers
    * for the whole block.
    *
    * This is the single-block path, used for XTS tweak derivation and as the
    * portable fallback. Bulk data goes through the SSE2/AVX2/NEON paths, which
    * share the same S-box definitions.
*/
#include "cl_platform.h"
#include "serpent.h"

#define PHI 0x9e3779b9UL

#if defined(_MSC_VER)
 #define ROTL32(x, n) _rotl((x), (n))
 /* MSVC declares _rotl/_rotr/_rotr64/_byteswap_uint64 for every target,
    ARM64 included. Excluding it there left them implicitly declared. */
 #include <intrin.h>
#else
 #define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#endif

#define SB_T          unsigned long
#define SB_XOR(a, b)  ((a) ^ (b))
#define SB_AND(a, b)  ((a) & (b))
#define SB_OR(a, b)   ((a) | (b))
#define SB_NOT(a)     (~(a))
#define SB_ROL(a, n)  ROTL32((a), (n))
#define SB_SHL(a, n)  ((a) << (n))

#include "serpent_sboxes.h"

static __forceinline unsigned long load32(const unsigned char *p)
{
	unsigned long v;
	memcpy(&v, p, sizeof(v));
	return v;
}

static __forceinline void store32(unsigned char *p, unsigned long v)
{
	memcpy(p, &v, sizeof(v));
}

/* Round key xor: K_i is expkey[4i .. 4i+3] */
#define KX(x0, x1, x2, x3, k, r) { \
	x0 ^= (k)[4*(r)+0];            \
	x1 ^= (k)[4*(r)+1];            \
	x2 ^= (k)[4*(r)+2];            \
	x3 ^= (k)[4*(r)+3];            \
}

/*
 * Key schedule.
 *
 * The 256-bit key becomes prekey words w[-8..-1]; the recurrence
 *   w[i] = (w[i-8] ^ w[i-5] ^ w[i-3] ^ w[i-1] ^ PHI ^ i) <<< 11
 * produces w[0..131]. Round keys follow by applying the bitsliced S-boxes to
 * consecutive groups of four prekey words, S-box index (3 - group) mod 8.
 */
void _stdcall serpent256_set_key(const unsigned char *key, serpent256_key *skey)
{
	unsigned long  ws[8 + SERPENT_EXPKEY_WORDS];
	unsigned long *k = skey->expkey;
	unsigned long  a, b, c, d;
	int            i;

	for (i = 0; i < 8; i++) {
		ws[i] = load32(key + i * 4);
	}
	for (i = 0; i < SERPENT_EXPKEY_WORDS; i++) {
		ws[i + 8] = ROTL32(ws[i] ^ ws[i + 3] ^ ws[i + 5] ^ ws[i + 7] ^ PHI ^ (unsigned long)i, 11);
	}

	for (i = 0; i < SERPENT_EXPKEY_WORDS / 4; i++)
	{
		a = ws[8 + i*4 + 0];
		b = ws[8 + i*4 + 1];
		c = ws[8 + i*4 + 2];
		d = ws[8 + i*4 + 3];

		switch (i & 7) {
			case 0: sE4(a, b, c, d); break;  /* S3 */
			case 1: sE3(a, b, c, d); break;  /* S2 */
			case 2: sE2(a, b, c, d); break;  /* S1 */
			case 3: sE1(a, b, c, d); break;  /* S0 */
			case 4: sE8(a, b, c, d); break;  /* S7 */
			case 5: sE7(a, b, c, d); break;  /* S6 */
			case 6: sE6(a, b, c, d); break;  /* S5 */
			case 7: sE5(a, b, c, d); break;  /* S4 */
		}
		k[i*4 + 0] = a;
		k[i*4 + 1] = b;
		k[i*4 + 2] = c;
		k[i*4 + 3] = d;
	}

	memset(ws, 0, sizeof(ws));
}

void _stdcall serpent256_encrypt(const unsigned char *in, unsigned char *out, serpent256_key *key)
{
	const unsigned long *k = key->expkey;
	unsigned long x0, x1, x2, x3;

	x0 = load32(in +  0);
	x1 = load32(in +  4);
	x2 = load32(in +  8);
	x3 = load32(in + 12);

	#define ENC_ROUND(r, sb) \
		KX(x0, x1, x2, x3, k, r); sb(x0, x1, x2, x3); SB_LT(x0, x1, x2, x3);

	ENC_ROUND( 0, sE1) ENC_ROUND( 1, sE2) ENC_ROUND( 2, sE3) ENC_ROUND( 3, sE4)
	ENC_ROUND( 4, sE5) ENC_ROUND( 5, sE6) ENC_ROUND( 6, sE7) ENC_ROUND( 7, sE8)
	ENC_ROUND( 8, sE1) ENC_ROUND( 9, sE2) ENC_ROUND(10, sE3) ENC_ROUND(11, sE4)
	ENC_ROUND(12, sE5) ENC_ROUND(13, sE6) ENC_ROUND(14, sE7) ENC_ROUND(15, sE8)
	ENC_ROUND(16, sE1) ENC_ROUND(17, sE2) ENC_ROUND(18, sE3) ENC_ROUND(19, sE4)
	ENC_ROUND(20, sE5) ENC_ROUND(21, sE6) ENC_ROUND(22, sE7) ENC_ROUND(23, sE8)
	ENC_ROUND(24, sE1) ENC_ROUND(25, sE2) ENC_ROUND(26, sE3) ENC_ROUND(27, sE4)
	ENC_ROUND(28, sE5) ENC_ROUND(29, sE6) ENC_ROUND(30, sE7)

	/* final round: no linear transform, extra key xor instead */
	KX(x0, x1, x2, x3, k, 31);
	sE8(x0, x1, x2, x3);
	KX(x0, x1, x2, x3, k, 32);

	#undef ENC_ROUND

	store32(out +  0, x0);
	store32(out +  4, x1);
	store32(out +  8, x2);
	store32(out + 12, x3);
}

void _stdcall serpent256_decrypt(const unsigned char *in, unsigned char *out, serpent256_key *key)
{
	const unsigned long *k = key->expkey;
	unsigned long x0, x1, x2, x3;

	x0 = load32(in +  0);
	x1 = load32(in +  4);
	x2 = load32(in +  8);
	x3 = load32(in + 12);

	KX(x0, x1, x2, x3, k, 32);
	sD8(x0, x1, x2, x3);
	KX(x0, x1, x2, x3, k, 31);

	#define DEC_ROUND(r, sb) \
		SB_ILT(x0, x1, x2, x3); sb(x0, x1, x2, x3); KX(x0, x1, x2, x3, k, r);

	DEC_ROUND(30, sD7) DEC_ROUND(29, sD6) DEC_ROUND(28, sD5) DEC_ROUND(27, sD4)
	DEC_ROUND(26, sD3) DEC_ROUND(25, sD2) DEC_ROUND(24, sD1) DEC_ROUND(23, sD8)
	DEC_ROUND(22, sD7) DEC_ROUND(21, sD6) DEC_ROUND(20, sD5) DEC_ROUND(19, sD4)
	DEC_ROUND(18, sD3) DEC_ROUND(17, sD2) DEC_ROUND(16, sD1) DEC_ROUND(15, sD8)
	DEC_ROUND(14, sD7) DEC_ROUND(13, sD6) DEC_ROUND(12, sD5) DEC_ROUND(11, sD4)
	DEC_ROUND(10, sD3) DEC_ROUND( 9, sD2) DEC_ROUND( 8, sD1) DEC_ROUND( 7, sD8)
	DEC_ROUND( 6, sD7) DEC_ROUND( 5, sD6) DEC_ROUND( 4, sD5) DEC_ROUND( 3, sD4)
	DEC_ROUND( 2, sD3) DEC_ROUND( 1, sD2) DEC_ROUND( 0, sD1)

	#undef DEC_ROUND

	store32(out +  0, x0);
	store32(out +  4, x1);
	store32(out +  8, x2);
	store32(out + 12, x3);
}
