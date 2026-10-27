/*
    * DiskCryptor crypto_lib - XTS-Twofish, four blocks interleaved
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Twofish's round function is four table lookups feeding an xor tree, two
    * dependent adds and a rotate, and every round depends on the one before it.
    * Run one block at a time and the core spends most of its cycles waiting on
    * that chain: the loads have latency but plenty of spare throughput.
    *
    * So this interleaves four independent blocks. XTS hands us exactly that -
    * neighbouring blocks in a sector differ only by their tweak - and the four
    * chains fill each other's stalls. It costs more live registers than x86-64
    * has, but the spills land in L1 and cost far less than the stalls they
    * replace: measured 435 MB/s against 242 for the one-at-a-time path, and 290
    * for the hand-written assembler this library had to drop.
    *
    * There is no SIMD form of this. Twofish's key-dependent S-boxes are memory
    * lookups, and gathers are not competitive with scalar loads here, so plain C
    * is the fast path on both amd64 and ARM64.
*/
#include "cl_platform.h"
#include "twofish.h"
#include "xts_fast.h"
#include "xts_twofish.h"

#if defined(_MSC_VER)
 /* also on ARM64: MSVC declares _rotl/_rotr here for every target, and the
    ROL32/ROR32 macros below need them. Excluding it there left them
    implicitly declared - invisible until a build turns C4013 into an
    error, which the kernel Debug configuration does. */
 #include <intrin.h>
 #define ROL32(x, n) _rotl((x), (n))
 #define ROR32(x, n) _rotr((x), (n))
#else
 #define ROL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
 #define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#endif

#define B(x, n)  ((unsigned char)((x) >> (8 * (n))))
#define LD(p)    (*(const unsigned long*)(p))
#define ST(p, v) (*(unsigned long*)(p) = (unsigned long)(v))

#define LO32(v) ((unsigned long)(v))
#define HI32(v) ((unsigned long)((v) >> 32))

/* g(X) and g(X <<< 8) through the key-dependent, MDS-folded tables */
#define G1(x) (s[0][B(x,0)] ^ s[1][B(x,1)] ^ s[2][B(x,2)] ^ s[3][B(x,3)])
#define G2(x) (s[0][B(x,3)] ^ s[1][B(x,0)] ^ s[2][B(x,1)] ^ s[3][B(x,2)])

/* t <- t * alpha in GF(2^128); runs in general-purpose registers */
#define TWEAK_NEXT(t0, t1) {                       \
	unsigned __int64 _cf = ((t1) >> 63) * 135;     \
	(t1) = ((t1) << 1) | ((t0) >> 63);             \
	(t0) = ((t0) << 1) ^ _cf;                      \
}

/* ---- four-way round functions --------------------------------------------- */

#define ER4(n, A0,B0,C0,D0, A1,B1,C1,D1, A2,B2,C2,D2, A3,B3,C3,D3) { \
	x0 = G1(A0); y0 = G2(B0); x1 = G1(A1); y1 = G2(B1);              \
	x2 = G1(A2); y2 = G2(B2); x3 = G1(A3); y3 = G2(B3);              \
	x0 += y0; x1 += y1; x2 += y2; x3 += y3;                          \
	y0 += x0 + k[2*(n)+1]; y1 += x1 + k[2*(n)+1];                    \
	y2 += x2 + k[2*(n)+1]; y3 += x3 + k[2*(n)+1];                    \
	x0 += k[2*(n)]; x1 += k[2*(n)]; x2 += k[2*(n)]; x3 += k[2*(n)];  \
	C0 ^= x0; C0 = ROR32(C0,1); C1 ^= x1; C1 = ROR32(C1,1);          \
	C2 ^= x2; C2 = ROR32(C2,1); C3 ^= x3; C3 = ROR32(C3,1);          \
	D0 = ROL32(D0,1); D0 ^= y0; D1 = ROL32(D1,1); D1 ^= y1;          \
	D2 = ROL32(D2,1); D2 ^= y2; D3 = ROL32(D3,1); D3 ^= y3;          \
}

#define DR4(n, A0,B0,C0,D0, A1,B1,C1,D1, A2,B2,C2,D2, A3,B3,C3,D3) { \
	x0 = G1(A0); y0 = G2(B0); x1 = G1(A1); y1 = G2(B1);              \
	x2 = G1(A2); y2 = G2(B2); x3 = G1(A3); y3 = G2(B3);              \
	x0 += y0; x1 += y1; x2 += y2; x3 += y3;                          \
	y0 += x0 + k[2*(n)+1]; y1 += x1 + k[2*(n)+1];                    \
	y2 += x2 + k[2*(n)+1]; y3 += x3 + k[2*(n)+1];                    \
	x0 += k[2*(n)]; x1 += k[2*(n)]; x2 += k[2*(n)]; x3 += k[2*(n)];  \
	D0 ^= y0; D0 = ROR32(D0,1); D1 ^= y1; D1 = ROR32(D1,1);          \
	D2 ^= y2; D2 = ROR32(D2,1); D3 ^= y3; D3 = ROR32(D3,1);          \
	C0 = ROL32(C0,1); C0 ^= x0; C1 = ROL32(C1,1); C1 ^= x1;          \
	C2 = ROL32(C2,1); C2 ^= x2; C3 = ROL32(C3,1); C3 ^= x3;          \
}

#define ROUNDS_A(M, n) M(n, a0,b0,c0,d0, a1,b1,c1,d1, a2,b2,c2,d2, a3,b3,c3,d3)
#define ROUNDS_B(M, n) M(n, c0,d0,a0,b0, c1,d1,a1,b1, c2,d2,a2,b2, c3,d3,a3,b3)

#define ENCRYPT_16                                             \
	ROUNDS_A(ER4,  0) ROUNDS_B(ER4,  1)                        \
	ROUNDS_A(ER4,  2) ROUNDS_B(ER4,  3)                        \
	ROUNDS_A(ER4,  4) ROUNDS_B(ER4,  5)                        \
	ROUNDS_A(ER4,  6) ROUNDS_B(ER4,  7)                        \
	ROUNDS_A(ER4,  8) ROUNDS_B(ER4,  9)                        \
	ROUNDS_A(ER4, 10) ROUNDS_B(ER4, 11)                        \
	ROUNDS_A(ER4, 12) ROUNDS_B(ER4, 13)                        \
	ROUNDS_A(ER4, 14) ROUNDS_B(ER4, 15)

#define DECRYPT_16                                             \
	ROUNDS_B(DR4, 15) ROUNDS_A(DR4, 14)                        \
	ROUNDS_B(DR4, 13) ROUNDS_A(DR4, 12)                        \
	ROUNDS_B(DR4, 11) ROUNDS_A(DR4, 10)                        \
	ROUNDS_B(DR4,  9) ROUNDS_A(DR4,  8)                        \
	ROUNDS_B(DR4,  7) ROUNDS_A(DR4,  6)                        \
	ROUNDS_B(DR4,  5) ROUNDS_A(DR4,  4)                        \
	ROUNDS_B(DR4,  3) ROUNDS_A(DR4,  2)                        \
	ROUNDS_B(DR4,  1) ROUNDS_A(DR4,  0)

/* ---- shared loop scaffolding ---------------------------------------------- */

#define XTS_TF_PROLOGUE                                                          \
	const unsigned long (*s)[256] = key->crypt_k.twofish.s;                      \
	const unsigned long *k = key->crypt_k.twofish.k;                             \
	const unsigned long *w = key->crypt_k.twofish.w;                             \
	unsigned long a0,b0,c0,d0,x0,y0, a1,b1,c1,d1,x1,y1;                          \
	unsigned long a2,b2,c2,d2,x2,y2, a3,b3,c3,d3,x3,y3;                          \
	unsigned __int64 idx[2], tw[2], nx[2], t0, t1, kt[8], sector;                \
	int i, j, more;                                                              \
                                                                                 \
	sector = offset / XTS_SECTOR_SIZE;                                           \
	idx[1] = 0;                                                                  \
	idx[0] = ++sector;                                                           \
	twofish256_encrypt((const unsigned char*)idx, (unsigned char*)tw,            \
	                   &key->tweak_k.twofish);                                   \
	t0 = tw[0]; t1 = tw[1];

/*
 * Deriving a sector's tweak is a full 16-round Twofish with nothing inside its
 * own sector to overlap with, so start the next sector's before doing this
 * one's bulk work - the same trick the AES and Serpent cores use.
 *
 * Only when a next sector exists, though. On the final sector the derivation
 * is discarded, and on a single-sector request that waste has nothing at all
 * to amortise against.
 */
#define XTS_TF_SECTOR_HEAD                                                       \
	more = (len -= XTS_SECTOR_SIZE) != 0;                                        \
	if (more) {                                                                  \
		idx[0] = sector + 1;                                                     \
		twofish256_encrypt((const unsigned char*)idx, (unsigned char*)nx,        \
		                   &key->tweak_k.twofish);                               \
	}                                                                            \
	sector++;

#define XTS_TF_LOAD_TWEAKS                                                       \
	for (j = 0; j < 4; j++) { kt[j*2] = t0; kt[j*2+1] = t1; TWEAK_NEXT(t0, t1) }

void _stdcall xts_twofish_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	XTS_TF_PROLOGUE

	for (;;) {
		XTS_TF_SECTOR_HEAD

		for (i = 0; i < XTS_BLOCKS_IN_SECTOR / 4; i++)
		{
			XTS_TF_LOAD_TWEAKS

			a0 = LD(in +  0) ^ LO32(kt[0]) ^ w[0];  b0 = LD(in +  4) ^ HI32(kt[0]) ^ w[1];
			c0 = LD(in +  8) ^ LO32(kt[1]) ^ w[2];  d0 = LD(in + 12) ^ HI32(kt[1]) ^ w[3];
			a1 = LD(in + 16) ^ LO32(kt[2]) ^ w[0];  b1 = LD(in + 20) ^ HI32(kt[2]) ^ w[1];
			c1 = LD(in + 24) ^ LO32(kt[3]) ^ w[2];  d1 = LD(in + 28) ^ HI32(kt[3]) ^ w[3];
			a2 = LD(in + 32) ^ LO32(kt[4]) ^ w[0];  b2 = LD(in + 36) ^ HI32(kt[4]) ^ w[1];
			c2 = LD(in + 40) ^ LO32(kt[5]) ^ w[2];  d2 = LD(in + 44) ^ HI32(kt[5]) ^ w[3];
			a3 = LD(in + 48) ^ LO32(kt[6]) ^ w[0];  b3 = LD(in + 52) ^ HI32(kt[6]) ^ w[1];
			c3 = LD(in + 56) ^ LO32(kt[7]) ^ w[2];  d3 = LD(in + 60) ^ HI32(kt[7]) ^ w[3];

			ENCRYPT_16

			ST(out +  0, c0 ^ w[4] ^ LO32(kt[0]));  ST(out +  4, d0 ^ w[5] ^ HI32(kt[0]));
			ST(out +  8, a0 ^ w[6] ^ LO32(kt[1]));  ST(out + 12, b0 ^ w[7] ^ HI32(kt[1]));
			ST(out + 16, c1 ^ w[4] ^ LO32(kt[2]));  ST(out + 20, d1 ^ w[5] ^ HI32(kt[2]));
			ST(out + 24, a1 ^ w[6] ^ LO32(kt[3]));  ST(out + 28, b1 ^ w[7] ^ HI32(kt[3]));
			ST(out + 32, c2 ^ w[4] ^ LO32(kt[4]));  ST(out + 36, d2 ^ w[5] ^ HI32(kt[4]));
			ST(out + 40, a2 ^ w[6] ^ LO32(kt[5]));  ST(out + 44, b2 ^ w[7] ^ HI32(kt[5]));
			ST(out + 48, c3 ^ w[4] ^ LO32(kt[6]));  ST(out + 52, d3 ^ w[5] ^ HI32(kt[6]));
			ST(out + 56, a3 ^ w[6] ^ LO32(kt[7]));  ST(out + 60, b3 ^ w[7] ^ HI32(kt[7]));

			in  += XTS_BLOCK_SIZE * 4;
			out += XTS_BLOCK_SIZE * 4;
		}
		if (!more) break;
		t0 = nx[0]; t1 = nx[1];
	}
}

void _stdcall xts_twofish_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key)
{
	XTS_TF_PROLOGUE

	for (;;) {
		XTS_TF_SECTOR_HEAD

		for (i = 0; i < XTS_BLOCKS_IN_SECTOR / 4; i++)
		{
			XTS_TF_LOAD_TWEAKS

			/* output whitening is undone first, and the halves arrive swapped */
			c0 = LD(in +  0) ^ LO32(kt[0]) ^ w[4];  d0 = LD(in +  4) ^ HI32(kt[0]) ^ w[5];
			a0 = LD(in +  8) ^ LO32(kt[1]) ^ w[6];  b0 = LD(in + 12) ^ HI32(kt[1]) ^ w[7];
			c1 = LD(in + 16) ^ LO32(kt[2]) ^ w[4];  d1 = LD(in + 20) ^ HI32(kt[2]) ^ w[5];
			a1 = LD(in + 24) ^ LO32(kt[3]) ^ w[6];  b1 = LD(in + 28) ^ HI32(kt[3]) ^ w[7];
			c2 = LD(in + 32) ^ LO32(kt[4]) ^ w[4];  d2 = LD(in + 36) ^ HI32(kt[4]) ^ w[5];
			a2 = LD(in + 40) ^ LO32(kt[5]) ^ w[6];  b2 = LD(in + 44) ^ HI32(kt[5]) ^ w[7];
			c3 = LD(in + 48) ^ LO32(kt[6]) ^ w[4];  d3 = LD(in + 52) ^ HI32(kt[6]) ^ w[5];
			a3 = LD(in + 56) ^ LO32(kt[7]) ^ w[6];  b3 = LD(in + 60) ^ HI32(kt[7]) ^ w[7];

			DECRYPT_16

			ST(out +  0, a0 ^ w[0] ^ LO32(kt[0]));  ST(out +  4, b0 ^ w[1] ^ HI32(kt[0]));
			ST(out +  8, c0 ^ w[2] ^ LO32(kt[1]));  ST(out + 12, d0 ^ w[3] ^ HI32(kt[1]));
			ST(out + 16, a1 ^ w[0] ^ LO32(kt[2]));  ST(out + 20, b1 ^ w[1] ^ HI32(kt[2]));
			ST(out + 24, c1 ^ w[2] ^ LO32(kt[3]));  ST(out + 28, d1 ^ w[3] ^ HI32(kt[3]));
			ST(out + 32, a2 ^ w[0] ^ LO32(kt[4]));  ST(out + 36, b2 ^ w[1] ^ HI32(kt[4]));
			ST(out + 40, c2 ^ w[2] ^ LO32(kt[5]));  ST(out + 44, d2 ^ w[3] ^ HI32(kt[5]));
			ST(out + 48, a3 ^ w[0] ^ LO32(kt[6]));  ST(out + 52, b3 ^ w[1] ^ HI32(kt[6]));
			ST(out + 56, c3 ^ w[2] ^ LO32(kt[7]));  ST(out + 60, d3 ^ w[3] ^ HI32(kt[7]));

			in  += XTS_BLOCK_SIZE * 4;
			out += XTS_BLOCK_SIZE * 4;
		}
		if (!more) break;
		t0 = nx[0]; t1 = nx[1];
	}
}
