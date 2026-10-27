/*
    * DiskCryptor crypto_lib - HMAC-SHA-512
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Written from RFC 2104 / FIPS-198.
    *
    * The context keeps the block-sized padded key so that done() can run the
    * outer hash without re-deriving it, which is what makes the PBKDF2 inner
    * loop cheap.
*/
#include "cl_platform.h"
#include "sha512_hmac.h"

#define IPAD 0x36
#define OPAD 0x5C

void _stdcall sha512_hmac_init(sha512_hmac_ctx *ctx, const void *key, size_t keylen)
{
	unsigned char block[SHA512_BLOCK_SIZE];
	size_t        i;

	/* keys longer than the block size are replaced by their digest */
	if (keylen > SHA512_BLOCK_SIZE) {
		sha512_init(&ctx->hash);
		sha512_hash(&ctx->hash, (const unsigned char*)key, keylen);
		sha512_done(&ctx->hash, ctx->padded_key);
		memset(ctx->padded_key + SHA512_DIGEST_SIZE, 0, SHA512_BLOCK_SIZE - SHA512_DIGEST_SIZE);
	} else {
		memcpy(ctx->padded_key, key, keylen);
		memset(ctx->padded_key + keylen, 0, SHA512_BLOCK_SIZE - keylen);
	}

	for (i = 0; i < SHA512_BLOCK_SIZE; i++) {
		block[i] = ctx->padded_key[i] ^ IPAD;
	}

	sha512_init(&ctx->hash);
	sha512_hash(&ctx->hash, block, SHA512_BLOCK_SIZE);

	memset(block, 0, sizeof(block));
}

void _stdcall sha512_hmac_hash(sha512_hmac_ctx *ctx, const void *ptr, size_t length)
{
	sha512_hash(&ctx->hash, (const unsigned char*)ptr, length);
}

void _stdcall sha512_hmac_done(sha512_hmac_ctx *ctx, unsigned char *out)
{
	unsigned char block[SHA512_BLOCK_SIZE];
	unsigned char inner[SHA512_DIGEST_SIZE];
	size_t        i;

	sha512_done(&ctx->hash, inner);

	for (i = 0; i < SHA512_BLOCK_SIZE; i++) {
		block[i] = ctx->padded_key[i] ^ OPAD;
	}

	sha512_init(&ctx->hash);
	sha512_hash(&ctx->hash, block, SHA512_BLOCK_SIZE);
	sha512_hash(&ctx->hash, inner, SHA512_DIGEST_SIZE);
	sha512_done(&ctx->hash, out);

	memset(block, 0, sizeof(block));
	memset(inner, 0, sizeof(inner));
}

void _stdcall sha512_hmac(const void *k, size_t k_len, const void *d, size_t d_len, unsigned char *out)
{
	sha512_hmac_ctx ctx;

	sha512_hmac_init(&ctx, k, k_len);
	sha512_hmac_hash(&ctx, d, d_len);
	sha512_hmac_done(&ctx, out);

	memset(&ctx, 0, sizeof(ctx));
}
