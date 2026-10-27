/*
    * DiskCryptor crypto_lib - Serpent bitsliced S-boxes and linear transform
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * The boolean gate sequences below implement the eight Serpent S-boxes and
    * their inverses in bitsliced form, as described in the Serpent AES
    * submission (Anderson, Biham, Knudsen) and in Dag Arne Osvik's
    * "Speeding up Serpent" (AES3, 2000). They are algorithm, not transcription
    * of any particular implementation.
    *
    * This header is deliberately free of any concrete data type. A translation
    * unit defines the operation macros below and includes it once; the same
    * gate sequences then drive the scalar, SSE2, AVX2 and NEON code paths, so
    * all of them are guaranteed to agree by construction.
    *
    *   SB_T        the word type operated on (unsigned long, __m128i, ...)
    *   SB_XOR(a,b) bitwise xor
    *   SB_AND(a,b) bitwise and
    *   SB_OR(a,b)  bitwise or
    *   SB_NOT(a)   bitwise complement
    *   SB_ROL(a,n) rotate each 32-bit lane left by a constant n
    *   SB_SHL(a,n) shift each 32-bit lane left by a constant n
*/
#ifndef _CL_SERPENT_SBOXES_H_
#define _CL_SERPENT_SBOXES_H_

#if !defined(SB_T) || !defined(SB_XOR) || !defined(SB_AND) || !defined(SB_OR) || !defined(SB_NOT)
 #error serpent_sboxes.h requires SB_T / SB_XOR / SB_AND / SB_OR / SB_NOT to be defined
#endif

/* ---- forward S-boxes: sE1..sE8 == S0..S7 ---------------------------------- */

#define sE1(B0, B1, B2, B3) {          \
	SB_T _tt = (B1);                   \
	B3 = SB_XOR(B3, B0);               \
	B1 = SB_AND(B1, B3);               \
	_tt = SB_XOR(_tt, B2);             \
	B1 = SB_XOR(B1, B0);               \
	B0 = SB_OR(B0, B3);                \
	B0 = SB_XOR(B0, _tt);              \
	_tt = SB_XOR(_tt, B3);             \
	B3 = SB_XOR(B3, B2);               \
	B2 = SB_OR(B2, B1);                \
	B2 = SB_XOR(B2, _tt);              \
	_tt = SB_NOT(_tt);                 \
	_tt = SB_OR(_tt, B1);              \
	B1 = SB_XOR(B1, B3);               \
	B1 = SB_XOR(B1, _tt);              \
	B3 = SB_OR(B3, B0);                \
	B1 = SB_XOR(B1, B3);               \
	_tt = SB_XOR(_tt, B3);             \
	B3 = B0;                           \
	B0 = B1;                           \
	B1 = _tt;                          \
}

#define sE2(B0, B1, B2, B3) {          \
	SB_T _tt;                          \
	B0 = SB_NOT(B0);                   \
	B2 = SB_NOT(B2);                   \
	_tt = B0;                          \
	B0 = SB_AND(B0, B1);               \
	B2 = SB_XOR(B2, B0);               \
	B0 = SB_OR(B0, B3);                \
	B3 = SB_XOR(B3, B2);               \
	B1 = SB_XOR(B1, B0);               \
	B0 = SB_XOR(B0, _tt);              \
	_tt = SB_OR(_tt, B1);              \
	B1 = SB_XOR(B1, B3);               \
	B2 = SB_OR(B2, B0);                \
	B2 = SB_AND(B2, _tt);              \
	B0 = SB_XOR(B0, B1);               \
	B1 = SB_AND(B1, B2);               \
	B1 = SB_XOR(B1, B0);               \
	B0 = SB_AND(B0, B2);               \
	_tt = SB_XOR(_tt, B0);             \
	B0 = B2;                           \
	B2 = B3;                           \
	B3 = B1;                           \
	B1 = _tt;                          \
}

#define sE3(B0, B1, B2, B3) {          \
	SB_T _tt = (B0);                   \
	B0 = SB_AND(B0, B2);               \
	B0 = SB_XOR(B0, B3);               \
	B2 = SB_XOR(B2, B1);               \
	B2 = SB_XOR(B2, B0);               \
	B3 = SB_OR(B3, _tt);               \
	B3 = SB_XOR(B3, B1);               \
	_tt = SB_XOR(_tt, B2);             \
	B1 = B3;                           \
	B3 = SB_OR(B3, _tt);               \
	B3 = SB_XOR(B3, B0);               \
	B0 = SB_AND(B0, B1);               \
	_tt = SB_XOR(_tt, B0);             \
	B1 = SB_XOR(B1, B3);               \
	B1 = SB_XOR(B1, _tt);              \
	B0 = B2;                           \
	B2 = B1;                           \
	B1 = B3;                           \
	B3 = SB_NOT(_tt);                  \
}

#define sE4(B0, B1, B2, B3) {          \
	SB_T _tt = (B0);                   \
	B0 = SB_OR(B0, B3);                \
	B3 = SB_XOR(B3, B1);               \
	B1 = SB_AND(B1, _tt);              \
	_tt = SB_XOR(_tt, B2);             \
	B2 = SB_XOR(B2, B3);               \
	B3 = SB_AND(B3, B0);               \
	_tt = SB_OR(_tt, B1);              \
	B3 = SB_XOR(B3, _tt);              \
	B0 = SB_XOR(B0, B1);               \
	_tt = SB_AND(_tt, B0);             \
	B1 = SB_XOR(B1, B3);               \
	_tt = SB_XOR(_tt, B2);             \
	B1 = SB_OR(B1, B0);                \
	B1 = SB_XOR(B1, B2);               \
	B0 = SB_XOR(B0, B3);               \
	B2 = B1;                           \
	B1 = SB_OR(B1, B3);                \
	B0 = SB_XOR(B0, B1);               \
	B1 = B2;                           \
	B2 = B3;                           \
	B3 = _tt;                          \
}

#define sE5(B0, B1, B2, B3) {          \
	SB_T _tt;                          \
	B1 = SB_XOR(B1, B3);               \
	B3 = SB_NOT(B3);                   \
	B2 = SB_XOR(B2, B3);               \
	B3 = SB_XOR(B3, B0);               \
	_tt = B1;                          \
	B1 = SB_AND(B1, B3);               \
	B1 = SB_XOR(B1, B2);               \
	_tt = SB_XOR(_tt, B3);             \
	B0 = SB_XOR(B0, _tt);              \
	B2 = SB_AND(B2, _tt);              \
	B2 = SB_XOR(B2, B0);               \
	B0 = SB_AND(B0, B1);               \
	B3 = SB_XOR(B3, B0);               \
	_tt = SB_OR(_tt, B1);              \
	_tt = SB_XOR(_tt, B0);             \
	B0 = SB_OR(B0, B3);                \
	B0 = SB_XOR(B0, B2);               \
	B2 = SB_AND(B2, B3);               \
	B0 = SB_NOT(B0);                   \
	_tt = SB_XOR(_tt, B2);             \
	B2 = B0;                           \
	B0 = B1;                           \
	B1 = _tt;                          \
}

#define sE6(B0, B1, B2, B3) {          \
	SB_T _tt;                          \
	B0 = SB_XOR(B0, B1);               \
	B1 = SB_XOR(B1, B3);               \
	B3 = SB_NOT(B3);                   \
	_tt = B1;                          \
	B1 = SB_AND(B1, B0);               \
	B2 = SB_XOR(B2, B3);               \
	B1 = SB_XOR(B1, B2);               \
	B2 = SB_OR(B2, _tt);               \
	_tt = SB_XOR(_tt, B3);             \
	B3 = SB_AND(B3, B1);               \
	B3 = SB_XOR(B3, B0);               \
	_tt = SB_XOR(_tt, B1);             \
	_tt = SB_XOR(_tt, B2);             \
	B2 = SB_XOR(B2, B0);               \
	B0 = SB_AND(B0, B3);               \
	B2 = SB_NOT(B2);                   \
	B0 = SB_XOR(B0, _tt);              \
	_tt = SB_OR(_tt, B3);              \
	_tt = SB_XOR(_tt, B2);             \
	B2 = B0;                           \
	B0 = B1;                           \
	B1 = B3;                           \
	B3 = _tt;                          \
}

#define sE7(B0, B1, B2, B3) {          \
	SB_T _tt;                          \
	B2 = SB_NOT(B2);                   \
	_tt = B3;                          \
	B3 = SB_AND(B3, B0);               \
	B0 = SB_XOR(B0, _tt);              \
	B3 = SB_XOR(B3, B2);               \
	B2 = SB_OR(B2, _tt);               \
	B1 = SB_XOR(B1, B3);               \
	B2 = SB_XOR(B2, B0);               \
	B0 = SB_OR(B0, B1);                \
	B2 = SB_XOR(B2, B1);               \
	_tt = SB_XOR(_tt, B0);             \
	B0 = SB_OR(B0, B3);                \
	B0 = SB_XOR(B0, B2);               \
	_tt = SB_XOR(_tt, B3);             \
	_tt = SB_XOR(_tt, B0);             \
	B3 = SB_NOT(B3);                   \
	B2 = SB_AND(B2, _tt);              \
	B3 = SB_XOR(B3, B2);               \
	B2 = _tt;                          \
}

#define sE8(B0, B1, B2, B3) {          \
	SB_T _tt = (B1);                   \
	B1 = SB_OR(B1, B2);                \
	B1 = SB_XOR(B1, B3);               \
	_tt = SB_XOR(_tt, B2);             \
	B2 = SB_XOR(B2, B1);               \
	B3 = SB_OR(B3, _tt);               \
	B3 = SB_AND(B3, B0);               \
	_tt = SB_XOR(_tt, B2);             \
	B3 = SB_XOR(B3, B1);               \
	B1 = SB_OR(B1, _tt);               \
	B1 = SB_XOR(B1, B0);               \
	B0 = SB_OR(B0, _tt);               \
	B0 = SB_XOR(B0, B2);               \
	B1 = SB_XOR(B1, _tt);              \
	B2 = SB_XOR(B2, B1);               \
	B1 = SB_AND(B1, B0);               \
	B1 = SB_XOR(B1, _tt);              \
	B2 = SB_NOT(B2);                   \
	B2 = SB_OR(B2, B0);                \
	_tt = SB_XOR(_tt, B2);             \
	B2 = B1;                           \
	B1 = B3;                           \
	B3 = B0;                           \
	B0 = _tt;                          \
}

/* ---- inverse S-boxes: sD1..sD8 == S0^-1..S7^-1 ---------------------------- */

#define sD1(B0, B1, B2, B3) {          \
	SB_T _tt = (B1);                   \
	B2 = SB_NOT(B2);                   \
	B1 = SB_OR(B1, B0);                \
	_tt = SB_NOT(_tt);                 \
	B1 = SB_XOR(B1, B2);               \
	B2 = SB_OR(B2, _tt);               \
	B1 = SB_XOR(B1, B3);               \
	B0 = SB_XOR(B0, _tt);              \
	B2 = SB_XOR(B2, B0);               \
	B0 = SB_AND(B0, B3);               \
	_tt = SB_XOR(_tt, B0);             \
	B0 = SB_OR(B0, B1);                \
	B0 = SB_XOR(B0, B2);               \
	B3 = SB_XOR(B3, _tt);              \
	B2 = SB_XOR(B2, B1);               \
	B3 = SB_XOR(B3, B0);               \
	B3 = SB_XOR(B3, B1);               \
	B2 = SB_AND(B2, B3);               \
	_tt = SB_XOR(_tt, B2);             \
	B2 = B1;                           \
	B1 = _tt;                          \
}

#define sD2(B0, B1, B2, B3) {          \
	SB_T _tt = (B1);                   \
	B1 = SB_XOR(B1, B3);               \
	B3 = SB_AND(B3, B1);               \
	_tt = SB_XOR(_tt, B2);             \
	B3 = SB_XOR(B3, B0);               \
	B0 = SB_OR(B0, B1);                \
	B2 = SB_XOR(B2, B3);               \
	B0 = SB_XOR(B0, _tt);              \
	B0 = SB_OR(B0, B2);                \
	B1 = SB_XOR(B1, B3);               \
	B0 = SB_XOR(B0, B1);               \
	B1 = SB_OR(B1, B3);                \
	B1 = SB_XOR(B1, B0);               \
	_tt = SB_NOT(_tt);                 \
	_tt = SB_XOR(_tt, B1);             \
	B1 = SB_OR(B1, B0);                \
	B1 = SB_XOR(B1, B0);               \
	B1 = SB_OR(B1, _tt);               \
	B3 = SB_XOR(B3, B1);               \
	B1 = B0;                           \
	B0 = _tt;                          \
	_tt = B2;                          \
	B2 = B3;                           \
	B3 = _tt;                          \
}

#define sD3(B0, B1, B2, B3) {          \
	SB_T _tt;                          \
	B2 = SB_XOR(B2, B3);               \
	B3 = SB_XOR(B3, B0);               \
	_tt = B3;                          \
	B3 = SB_AND(B3, B2);               \
	B3 = SB_XOR(B3, B1);               \
	B1 = SB_OR(B1, B2);                \
	B1 = SB_XOR(B1, _tt);              \
	_tt = SB_AND(_tt, B3);             \
	B2 = SB_XOR(B2, B3);               \
	_tt = SB_AND(_tt, B0);             \
	_tt = SB_XOR(_tt, B2);             \
	B2 = SB_AND(B2, B1);               \
	B2 = SB_OR(B2, B0);                \
	B3 = SB_NOT(B3);                   \
	B2 = SB_XOR(B2, B3);               \
	B0 = SB_XOR(B0, B3);               \
	B0 = SB_AND(B0, B1);               \
	B3 = SB_XOR(B3, _tt);              \
	B3 = SB_XOR(B3, B0);               \
	B0 = B1;                           \
	B1 = _tt;                          \
}

#define sD4(B0, B1, B2, B3) {          \
	SB_T _tt = (B2);                   \
	B2 = SB_XOR(B2, B1);               \
	B0 = SB_XOR(B0, B2);               \
	_tt = SB_AND(_tt, B2);             \
	_tt = SB_XOR(_tt, B0);             \
	B0 = SB_AND(B0, B1);               \
	B1 = SB_XOR(B1, B3);               \
	B3 = SB_OR(B3, _tt);               \
	B2 = SB_XOR(B2, B3);               \
	B0 = SB_XOR(B0, B3);               \
	B1 = SB_XOR(B1, _tt);              \
	B3 = SB_AND(B3, B2);               \
	B3 = SB_XOR(B3, B1);               \
	B1 = SB_XOR(B1, B0);               \
	B1 = SB_OR(B1, B2);                \
	B0 = SB_XOR(B0, B3);               \
	B1 = SB_XOR(B1, _tt);              \
	B0 = SB_XOR(B0, B1);               \
	_tt = B0;                          \
	B0 = B2;                           \
	B2 = B3;                           \
	B3 = _tt;                          \
}

#define sD5(B0, B1, B2, B3) {          \
	SB_T _tt = (B2);                   \
	B2 = SB_AND(B2, B3);               \
	B2 = SB_XOR(B2, B1);               \
	B1 = SB_OR(B1, B3);                \
	B1 = SB_AND(B1, B0);               \
	_tt = SB_XOR(_tt, B2);             \
	_tt = SB_XOR(_tt, B1);             \
	B1 = SB_AND(B1, B2);               \
	B0 = SB_NOT(B0);                   \
	B3 = SB_XOR(B3, _tt);              \
	B1 = SB_XOR(B1, B3);               \
	B3 = SB_AND(B3, B0);               \
	B3 = SB_XOR(B3, B2);               \
	B0 = SB_XOR(B0, B1);               \
	B2 = SB_AND(B2, B0);               \
	B3 = SB_XOR(B3, B0);               \
	B2 = SB_XOR(B2, _tt);              \
	B2 = SB_OR(B2, B3);                \
	B3 = SB_XOR(B3, B0);               \
	B2 = SB_XOR(B2, B1);               \
	B1 = B3;                           \
	B3 = _tt;                          \
}

#define sD6(B0, B1, B2, B3) {          \
	SB_T _tt = (B3);                   \
	B1 = SB_NOT(B1);                   \
	B2 = SB_XOR(B2, B1);               \
	B3 = SB_OR(B3, B0);                \
	B3 = SB_XOR(B3, B2);               \
	B2 = SB_OR(B2, B1);                \
	B2 = SB_AND(B2, B0);               \
	_tt = SB_XOR(_tt, B3);             \
	B2 = SB_XOR(B2, _tt);              \
	_tt = SB_OR(_tt, B0);              \
	_tt = SB_XOR(_tt, B1);             \
	B1 = SB_AND(B1, B2);               \
	B1 = SB_XOR(B1, B3);               \
	_tt = SB_XOR(_tt, B2);             \
	B3 = SB_AND(B3, _tt);              \
	_tt = SB_XOR(_tt, B1);             \
	B3 = SB_XOR(B3, _tt);              \
	_tt = SB_NOT(_tt);                 \
	B3 = SB_XOR(B3, B0);               \
	B0 = B1;                           \
	B1 = _tt;                          \
	_tt = B3;                          \
	B3 = B2;                           \
	B2 = _tt;                          \
}

#define sD7(B0, B1, B2, B3) {          \
	SB_T _tt = (B2);                   \
	B0 = SB_XOR(B0, B2);               \
	B2 = SB_AND(B2, B0);               \
	_tt = SB_XOR(_tt, B3);             \
	B2 = SB_NOT(B2);                   \
	B3 = SB_XOR(B3, B1);               \
	B2 = SB_XOR(B2, B3);               \
	_tt = SB_OR(_tt, B0);              \
	B0 = SB_XOR(B0, B2);               \
	B3 = SB_XOR(B3, _tt);              \
	_tt = SB_XOR(_tt, B1);             \
	B1 = SB_AND(B1, B3);               \
	B1 = SB_XOR(B1, B0);               \
	B0 = SB_XOR(B0, B3);               \
	B0 = SB_OR(B0, B2);                \
	B3 = SB_XOR(B3, B1);               \
	_tt = SB_XOR(_tt, B0);             \
	B0 = B1;                           \
	B1 = B2;                           \
	B2 = _tt;                          \
}

#define sD8(B0, B1, B2, B3) {          \
	SB_T _tt = (B2);                   \
	B2 = SB_XOR(B2, B0);               \
	B0 = SB_AND(B0, B3);               \
	_tt = SB_OR(_tt, B3);              \
	B2 = SB_NOT(B2);                   \
	B3 = SB_XOR(B3, B1);               \
	B1 = SB_OR(B1, B0);                \
	B0 = SB_XOR(B0, B2);               \
	B2 = SB_AND(B2, _tt);              \
	B3 = SB_AND(B3, _tt);              \
	B1 = SB_XOR(B1, B2);               \
	B2 = SB_XOR(B2, B0);               \
	B0 = SB_OR(B0, B2);                \
	_tt = SB_XOR(_tt, B1);             \
	B0 = SB_XOR(B0, B3);               \
	B3 = SB_XOR(B3, _tt);              \
	_tt = SB_OR(_tt, B0);              \
	B3 = SB_XOR(B3, B2);               \
	_tt = SB_XOR(_tt, B2);             \
	B2 = B1;                           \
	B1 = B0;                           \
	B0 = B3;                           \
	B3 = _tt;                          \
}

/* ---- linear transform ----------------------------------------------------- */

#if defined(SB_ROL) && defined(SB_SHL)

#define SB_LT(B0, B1, B2, B3) {        \
	B0 = SB_ROL(B0, 13);               \
	B2 = SB_ROL(B2, 3);                \
	B1 = SB_XOR(B1, B0);               \
	B1 = SB_XOR(B1, B2);               \
	B3 = SB_XOR(B3, B2);               \
	B3 = SB_XOR(B3, SB_SHL(B0, 3));    \
	B1 = SB_ROL(B1, 1);                \
	B3 = SB_ROL(B3, 7);                \
	B0 = SB_XOR(B0, B1);               \
	B0 = SB_XOR(B0, B3);               \
	B2 = SB_XOR(B2, B3);               \
	B2 = SB_XOR(B2, SB_SHL(B1, 7));    \
	B0 = SB_ROL(B0, 5);                \
	B2 = SB_ROL(B2, 22);               \
}

#define SB_ILT(B0, B1, B2, B3) {       \
	B2 = SB_ROL(B2, 10);               \
	B0 = SB_ROL(B0, 27);               \
	B2 = SB_XOR(B2, B3);               \
	B2 = SB_XOR(B2, SB_SHL(B1, 7));    \
	B0 = SB_XOR(B0, B1);               \
	B0 = SB_XOR(B0, B3);               \
	B3 = SB_ROL(B3, 25);               \
	B1 = SB_ROL(B1, 31);               \
	B3 = SB_XOR(B3, B2);               \
	B3 = SB_XOR(B3, SB_SHL(B0, 3));    \
	B1 = SB_XOR(B1, B0);               \
	B1 = SB_XOR(B1, B2);               \
	B2 = SB_ROL(B2, 29);               \
	B0 = SB_ROL(B0, 19);               \
}

#endif /* SB_ROL && SB_SHL */

#endif /* _CL_SERPENT_SBOXES_H_ */
