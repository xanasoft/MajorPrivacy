/* SPDX-License-Identifier: CC0-1.0 OR Apache-2.0 */
/*
 * Argon2 reference source code package - reference C implementations
 *
 * Copyright 2015
 * Daniel Dinu, Dmitry Khovratovich, Jean-Philippe Aumasson, and Samuel Neves
 *
 * You may use this work under the terms of a Creative Commons CC0 1.0
 * License/Waiver or the Apache Public License 2.0, at your option. The terms of
 * these licenses can be found at:
 *
 * - CC0 1.0 Universal : https://creativecommons.org/publicdomain/zero/1.0
 * - Apache 2.0        : https://www.apache.org/licenses/LICENSE-2.0
 *
 * You should have received a copy of both of these licenses along with this
 * software. If not, they may be obtained at the above URLs.
 */

 /* ARM64 NEON port of the SSE2 round in blamka-round-opt.h, for DiskCryptor - 2026 */

#ifndef BLAKE_ROUND_MKA_NEON_H
#define BLAKE_ROUND_MKA_NEON_H

#include <arm_neon.h>
#include "blake2-impl.h"

/*
 * Same layout as the SSE2 macros: each uint64x2_t holds two 64-bit words, so
 * one BLAKE2_ROUND runs two G functions per vector. blamka-round-ref.h has the
 * scalar semantics this must reproduce bit for bit.
 *
 * Only ARMv7-era NEON intrinsics are used here. The WDK kernel arm_neon.h lacks
 * some of the AArch64-only additions (vdupq_laneq_s8, for one), and a missing
 * declaration there is only a C4013 warning until the driver fails to link.
 */

/* rotr64 by 0 < c < 64: x << (64 - c), with x >> c inserted below it */
#define rotr64_neon(x, c) vsriq_n_u64(vshlq_n_u64((x), 64 - (c)), (x), (c))

/* x + y + 2 * lo32(x) * lo32(y) */
static BLAKE2_INLINE uint64x2_t fBlaMka(uint64x2_t x, uint64x2_t y) {
    const uint64x2_t z = vmull_u32(vmovn_u64(x), vmovn_u64(y));
    return vaddq_u64(vaddq_u64(x, y), vaddq_u64(z, z));
}

#define G1(A0, B0, C0, D0, A1, B1, C1, D1)                                     \
    do {                                                                       \
        A0 = fBlaMka(A0, B0);                                                  \
        A1 = fBlaMka(A1, B1);                                                  \
                                                                               \
        D0 = veorq_u64(D0, A0);                                                \
        D1 = veorq_u64(D1, A1);                                                \
                                                                               \
        D0 = rotr64_neon(D0, 32);                                              \
        D1 = rotr64_neon(D1, 32);                                              \
                                                                               \
        C0 = fBlaMka(C0, D0);                                                  \
        C1 = fBlaMka(C1, D1);                                                  \
                                                                               \
        B0 = veorq_u64(B0, C0);                                                \
        B1 = veorq_u64(B1, C1);                                                \
                                                                               \
        B0 = rotr64_neon(B0, 24);                                              \
        B1 = rotr64_neon(B1, 24);                                              \
    } while ((void)0, 0)

#define G2(A0, B0, C0, D0, A1, B1, C1, D1)                                     \
    do {                                                                       \
        A0 = fBlaMka(A0, B0);                                                  \
        A1 = fBlaMka(A1, B1);                                                  \
                                                                               \
        D0 = veorq_u64(D0, A0);                                                \
        D1 = veorq_u64(D1, A1);                                                \
                                                                               \
        D0 = rotr64_neon(D0, 16);                                              \
        D1 = rotr64_neon(D1, 16);                                              \
                                                                               \
        C0 = fBlaMka(C0, D0);                                                  \
        C1 = fBlaMka(C1, D1);                                                  \
                                                                               \
        B0 = veorq_u64(B0, C0);                                                \
        B1 = veorq_u64(B1, C1);                                                \
                                                                               \
        B0 = rotr64_neon(B0, 63);                                              \
        B1 = rotr64_neon(B1, 63);                                              \
    } while ((void)0, 0)

/*
 * With A0..D1 = (v0,v1) (v2,v3) (v4,v5) (v6,v7) (v8,v9) (v10,v11) (v12,v13)
 * (v14,v15), the diagonal step needs B0=(v5,v6) B1=(v7,v4) C0=(v10,v11)
 * C1=(v8,v9) D0=(v15,v12) D1=(v13,v14). vextq_u64(a, b, 1) is (a[1], b[0]).
 */
#define DIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1)                            \
    do {                                                                       \
        uint64x2_t t0 = vextq_u64(B0, B1, 1);                                  \
        uint64x2_t t1 = vextq_u64(B1, B0, 1);                                  \
        B0 = t0;                                                               \
        B1 = t1;                                                               \
                                                                               \
        t0 = C0;                                                               \
        C0 = C1;                                                               \
        C1 = t0;                                                               \
                                                                               \
        t0 = vextq_u64(D1, D0, 1);                                             \
        t1 = vextq_u64(D0, D1, 1);                                             \
        D0 = t0;                                                               \
        D1 = t1;                                                               \
    } while ((void)0, 0)

#define UNDIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1)                          \
    do {                                                                       \
        uint64x2_t t0 = vextq_u64(B1, B0, 1);                                  \
        uint64x2_t t1 = vextq_u64(B0, B1, 1);                                  \
        B0 = t0;                                                               \
        B1 = t1;                                                               \
                                                                               \
        t0 = C0;                                                               \
        C0 = C1;                                                               \
        C1 = t0;                                                               \
                                                                               \
        t0 = vextq_u64(D0, D1, 1);                                             \
        t1 = vextq_u64(D1, D0, 1);                                             \
        D0 = t0;                                                               \
        D1 = t1;                                                               \
    } while ((void)0, 0)

#define BLAKE2_ROUND(A0, A1, B0, B1, C0, C1, D0, D1)                           \
    do {                                                                       \
        G1(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
        G2(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
                                                                               \
        DIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1);                           \
                                                                               \
        G1(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
        G2(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
                                                                               \
        UNDIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1);                         \
    } while ((void)0, 0)

#endif /* BLAKE_ROUND_MKA_NEON_H */
