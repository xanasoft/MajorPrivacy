/*
    * DiskCryptor crypto_lib - HMAC_DRBG over SHA-512 (NIST SP 800-90A)
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Written from NIST SP 800-90A rev 1, section 10.1.2. Validated against the
    * CAVP drbgvectors_pr_false HMAC_DRBG SHA-512 vectors.
    *
    * Note on the instantiate interface: SP 800-90A takes entropy and nonce
    * separately and concatenates them into the seed material. This API takes
    * them already concatenated in the entropy buffer, which is what the caller
    * naturally has.
*/
#include "cl_platform.h"
#include "sha512_hmac_drbg.h"

/* a piece of seed material; NULL/0 entries are skipped */
typedef struct _drbg_part {
	const void *ptr;
	size_t      len;
} drbg_part;

/*
 * SP 800-90A 10.1.2.2 - HMAC_DRBG Update.
 *
 *   K = HMAC(K, V || 0x00 || provided_data)
 *   V = HMAC(K, V)
 *   if provided_data is non-empty:
 *       K = HMAC(K, V || 0x01 || provided_data)
 *       V = HMAC(K, V)
 */
static void drbg_update(sha512_hmac_drbg_ctx *ctx, const drbg_part *parts, int nparts)
{
	sha512_hmac_ctx hctx;
	unsigned char   sep;
	size_t          total = 0;
	int             i, round;

	for (i = 0; i < nparts; i++) {
		if (parts[i].ptr != NULL) total += parts[i].len;
	}

	for (round = 0; round < 2; round++)
	{
		sep = (unsigned char)round;

		sha512_hmac_init(&hctx, ctx->key, sizeof(ctx->key));
		sha512_hmac_hash(&hctx, ctx->val, sizeof(ctx->val));
		sha512_hmac_hash(&hctx, &sep, 1);
		for (i = 0; i < nparts; i++) {
			if (parts[i].ptr != NULL && parts[i].len != 0) {
				sha512_hmac_hash(&hctx, parts[i].ptr, parts[i].len);
			}
		}
		sha512_hmac_done(&hctx, ctx->key);

		sha512_hmac(ctx->key, sizeof(ctx->key), ctx->val, sizeof(ctx->val), ctx->val);

		/* with no provided data the second pass is skipped */
		if (total == 0) break;
	}

	memset(&hctx, 0, sizeof(hctx));
}

int _stdcall sha512_hmac_drbg_instantiate(sha512_hmac_drbg_ctx *ctx,
                                          const void *entropy,  size_t entropy_len,
                                          const void *personal, size_t personal_len)
{
	drbg_part parts[2];

	if (ctx == NULL || entropy == NULL) return -1;
	if (entropy_len < SHA512_HMAC_DRBG_MIN_ENTROPY_BYTES) return -1;
	if (entropy_len > SHA512_HMAC_DRBG_MAX_ENTROPY_BYTES) return -1;
	if (personal != NULL && personal_len > SHA512_HMAC_DRBG_MAX_PERSONAL_BYTES) return -1;

	memset(ctx->key, 0x00, sizeof(ctx->key));
	memset(ctx->val, 0x01, sizeof(ctx->val));

	parts[0].ptr = entropy;  parts[0].len = entropy_len;
	parts[1].ptr = personal; parts[1].len = (personal != NULL) ? personal_len : 0;

	drbg_update(ctx, parts, 2);
	ctx->reseed_counter = 1;
	return 0;
}

int _stdcall sha512_hmac_drbg_reseed(sha512_hmac_drbg_ctx *ctx,
                                     const void *entropy,    size_t entropy_len,
                                     const void *additional, size_t additional_len)
{
	drbg_part parts[2];

	if (ctx == NULL || entropy == NULL) return -1;
	if (entropy_len < SHA512_HMAC_DRBG_MIN_ENTROPY_BYTES) return -1;
	if (entropy_len > SHA512_HMAC_DRBG_MAX_ENTROPY_BYTES) return -1;
	if (additional != NULL && additional_len > SHA512_HMAC_DRBG_MAX_ADDITIONAL_BYTES) return -1;

	parts[0].ptr = entropy;    parts[0].len = entropy_len;
	parts[1].ptr = additional; parts[1].len = (additional != NULL) ? additional_len : 0;

	drbg_update(ctx, parts, 2);
	ctx->reseed_counter = 1;
	return 0;
}

int _stdcall sha512_hmac_drbg_generate(sha512_hmac_drbg_ctx *ctx,
                                       const void    *additional, size_t additional_len,
                                       unsigned char *output,     size_t output_len)
{
	drbg_part parts[1];
	size_t    n;

	if (ctx == NULL || output == NULL) return -1;
	if (output_len > SHA512_HMAC_DRBG_MAX_GENERATED_BYTES) return -1;
	if (additional != NULL && additional_len > SHA512_HMAC_DRBG_MAX_ADDITIONAL_BYTES) return -1;
	if (ctx->reseed_counter > SHA512_HMAC_DRBG_RESEED_INTERVAL) return -1;

	parts[0].ptr = additional;
	parts[0].len = (additional != NULL) ? additional_len : 0;

	if (parts[0].len != 0) {
		drbg_update(ctx, parts, 1);
	}

	while (output_len != 0)
	{
		sha512_hmac(ctx->key, sizeof(ctx->key), ctx->val, sizeof(ctx->val), ctx->val);

		n = (output_len < SHA512_DIGEST_SIZE) ? output_len : SHA512_DIGEST_SIZE;
		memcpy(output, ctx->val, n);
		output += n; output_len -= n;
	}

	drbg_update(ctx, parts, 1);
	ctx->reseed_counter++;
	return 0;
}
