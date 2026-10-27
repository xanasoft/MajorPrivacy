/*
    * DiskCryptor crypto_lib - Serpent round structure, vector form
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Included by a translation unit that has already defined the SB_* operation
    * macros and included serpent_sboxes.h. Adds the round-key xor and the full
    * 32-round schedules on top of the shared S-box definitions, so the SSE2,
    * AVX2 and NEON cores differ only in their vector primitives.
    *
    * Additionally required from the includer:
    *   SB_SET1(v)  broadcast a 32-bit value to every lane
*/
#ifndef SB_SET1
 #error serpent_simd.inl requires SB_SET1 to be defined
#endif

/* round key xor: K_r occupies expkey[4r .. 4r+3], one word per bitslice */
#define SP_KX(B0, B1, B2, B3, ctx, r) {                     \
	B0 = SB_XOR(B0, SB_SET1((ctx)->expkey[4 * (r) + 0]));   \
	B1 = SB_XOR(B1, SB_SET1((ctx)->expkey[4 * (r) + 1]));   \
	B2 = SB_XOR(B2, SB_SET1((ctx)->expkey[4 * (r) + 2]));   \
	B3 = SB_XOR(B3, SB_SET1((ctx)->expkey[4 * (r) + 3]));   \
}

#define SP_ENC_ROUND(B0, B1, B2, B3, ctx, r, sb) \
	SP_KX(B0, B1, B2, B3, ctx, r) sb(B0, B1, B2, B3) SB_LT(B0, B1, B2, B3)

#define SP_DEC_ROUND(B0, B1, B2, B3, ctx, r, sb) \
	SB_ILT(B0, B1, B2, B3) sb(B0, B1, B2, B3) SP_KX(B0, B1, B2, B3, ctx, r)

#define SERPENT_SIMD_ENCRYPT(B0, B1, B2, B3, ctx) {                  \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx,  0, sE1)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx,  1, sE2)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx,  2, sE3)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx,  3, sE4)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx,  4, sE5)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx,  5, sE6)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx,  6, sE7)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx,  7, sE8)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx,  8, sE1)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx,  9, sE2)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 10, sE3)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 11, sE4)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 12, sE5)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 13, sE6)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 14, sE7)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 15, sE8)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 16, sE1)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 17, sE2)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 18, sE3)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 19, sE4)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 20, sE5)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 21, sE6)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 22, sE7)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 23, sE8)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 24, sE1)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 25, sE2)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 26, sE3)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 27, sE4)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 28, sE5)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 29, sE6)                       \
	SP_ENC_ROUND(B0, B1, B2, B3, ctx, 30, sE7)                       \
	SP_KX(B0, B1, B2, B3, ctx, 31) sE8(B0, B1, B2, B3)               \
	SP_KX(B0, B1, B2, B3, ctx, 32)                                   \
}

#define SERPENT_SIMD_DECRYPT(B0, B1, B2, B3, ctx) {                  \
	SP_KX(B0, B1, B2, B3, ctx, 32) sD8(B0, B1, B2, B3)               \
	SP_KX(B0, B1, B2, B3, ctx, 31)                                   \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 30, sD7)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 29, sD6)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 28, sD5)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 27, sD4)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 26, sD3)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 25, sD2)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 24, sD1)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 23, sD8)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 22, sD7)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 21, sD6)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 20, sD5)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 19, sD4)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 18, sD3)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 17, sD2)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 16, sD1)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 15, sD8)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 14, sD7)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 13, sD6)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 12, sD5)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 11, sD4)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx, 10, sD3)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx,  9, sD2)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx,  8, sD1)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx,  7, sD8)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx,  6, sD7)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx,  5, sD6)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx,  4, sD5)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx,  3, sD4)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx,  2, sD3)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx,  1, sD2)                       \
	SP_DEC_ROUND(B0, B1, B2, B3, ctx,  0, sD1)                       \
}
