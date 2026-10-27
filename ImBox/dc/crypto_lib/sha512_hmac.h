/*
    * DiskCryptor crypto_lib - HMAC-SHA-512
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
*/
#ifndef _CL_SHA512_HMAC_H_
#define _CL_SHA512_HMAC_H_

#include "sha512.h"

typedef struct _sha512_hmac_ctx {
	sha512_ctx    hash;
	unsigned char padded_key[SHA512_BLOCK_SIZE];
} sha512_hmac_ctx;

void _stdcall sha512_hmac_init(sha512_hmac_ctx *ctx, const void *key, size_t keylen);
void _stdcall sha512_hmac_hash(sha512_hmac_ctx *ctx, const void *ptr, size_t length);
void _stdcall sha512_hmac_done(sha512_hmac_ctx *ctx, unsigned char *out);
void _stdcall sha512_hmac(const void *k, size_t k_len, const void *d, size_t d_len, unsigned char *out);

#endif
