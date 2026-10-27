/*
    * DiskCryptor crypto_lib - HMAC_DRBG over SHA-512 (NIST SP 800-90A)
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
*/
#ifndef _CL_SHA512_HMAC_DRBG_H_
#define _CL_SHA512_HMAC_DRBG_H_

#include "sha512.h"
#include "sha512_hmac.h"

/* SP 800-90A section 10.1, table 2, for a 256-bit security strength */
#define SHA512_HMAC_DRBG_MIN_ENTROPY_BYTES    (256 / 8)
#define SHA512_HMAC_DRBG_MAX_ENTROPY_BYTES    ((1ull << 35) / 8)
#define SHA512_HMAC_DRBG_MAX_PERSONAL_BYTES   ((1ull << 35) / 8)
#define SHA512_HMAC_DRBG_MAX_ADDITIONAL_BYTES ((1ull << 35) / 8)
#define SHA512_HMAC_DRBG_MAX_GENERATED_BYTES  ((1 << 19) / 8)

/* the standard permits up to 2^48; we reseed far more often than that */
#define SHA512_HMAC_DRBG_RESEED_INTERVAL 128

typedef struct _sha512_hmac_drbg_ctx {
	unsigned char key[SHA512_DIGEST_SIZE];
	unsigned char val[SHA512_DIGEST_SIZE];
	unsigned long reseed_counter;
} sha512_hmac_drbg_ctx;

/* all three return 0 on success and a negative value on failure */

int _stdcall sha512_hmac_drbg_instantiate(sha512_hmac_drbg_ctx *ctx,
                                          const void *entropy,  size_t entropy_len,
                                          const void *personal, size_t personal_len);

int _stdcall sha512_hmac_drbg_reseed(sha512_hmac_drbg_ctx *ctx,
                                     const void *entropy,    size_t entropy_len,
                                     const void *additional, size_t additional_len);

int _stdcall sha512_hmac_drbg_generate(sha512_hmac_drbg_ctx *ctx,
                                       const void    *additional, size_t additional_len,
                                       unsigned char *output,     size_t output_len);

#endif
