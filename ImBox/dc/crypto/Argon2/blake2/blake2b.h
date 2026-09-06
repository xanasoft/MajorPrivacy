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

#ifndef PORTABLE_BLAKE2_H
#define PORTABLE_BLAKE2_H

#include "../argon2.h"

#if defined(__cplusplus)
extern "C" {
#endif

enum blake2b_constant {
    BLAKE2B_BLOCKBYTES = 128,
    BLAKE2B_OUTBYTES = 64,
    BLAKE2B_KEYBYTES = 64,
    BLAKE2B_SALTBYTES = 16,
    BLAKE2B_PERSONALBYTES = 16
};

#pragma pack(push, 1)
typedef struct __blake2b_param {
    u8 digest_length;                   /* 1 */
    u8 key_length;                      /* 2 */
    u8 fanout;                          /* 3 */
    u8 depth;                           /* 4 */
    u32 leaf_length;                    /* 8 */
    u64 node_offset;                    /* 16 */
    u8 node_depth;                      /* 17 */
    u8 inner_length;                    /* 18 */
    u8 reserved[14];                    /* 32 */
    u8 salt[BLAKE2B_SALTBYTES];         /* 48 */
    u8 personal[BLAKE2B_PERSONALBYTES]; /* 64 */
} blake2b_param;
#pragma pack(pop)

typedef struct __blake2b_state {
    u64 h[8];
    u64 t[2];
    u64 f[2];
    u8 buf[BLAKE2B_BLOCKBYTES];
    unsigned buflen;
    unsigned outlen;
    u8 last_node;
} blake2b_state;

/* Ensure param structs have not been wrongly padded */
/* Poor man's static_assert */
enum {
    blake2_size_check_0 = 1 / (!!(CHAR_BIT == 8) ? 1 : 0),
    blake2_size_check_2 =
    1 / (!!(sizeof(blake2b_param) == sizeof(u64) * CHAR_BIT) ? 1 : 0)
};

/* Streaming API */
ARGON2_LOCAL int blake2b_init(blake2b_state *S, size_t outlen);
ARGON2_LOCAL int blake2b_init_key(blake2b_state *S, size_t outlen, const void *key,
                     size_t keylen);
ARGON2_LOCAL int blake2b_init_param(blake2b_state *S, const blake2b_param *P);
ARGON2_LOCAL int blake2b_update(blake2b_state *S, const void *in, size_t inlen);
ARGON2_LOCAL int blake2b_final(blake2b_state *S, void *out, size_t outlen);

/* Simple API */
ARGON2_LOCAL int blake2b(void *out, size_t outlen, const void *in, size_t inlen,
                         const void *key, size_t keylen);

/* Argon2 Team - Begin Code */
ARGON2_LOCAL int blake2b_long(void *out, size_t outlen, const void *in, size_t inlen);
/* Argon2 Team - End Code */

#if defined(__cplusplus)
}
#endif

#endif
