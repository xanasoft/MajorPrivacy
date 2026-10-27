/*
    * DiskCryptor crypto_lib - PBKDF2-HMAC-SHA-512
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Written from PKCS #5 v2.1 / RFC 8018, section 5.2.
    *
    * The HMAC context is initialised once per block and copied for each
    * iteration, so the i_count iterations cost one SHA-512 compression pair
    * each instead of re-absorbing the padded key every time.
*/
#include "cl_platform.h"
#include "sha512_hmac.h"
#include "sha512_pkcs5_2.h"

void _stdcall sha512_pkcs5_2(int i_count, const void *pwd, size_t pwd_len,
                             const void *salt, size_t salt_len,
                             unsigned char *dk, size_t dklen)
{
	sha512_hmac_ctx base, ctx;
	unsigned char   u[SHA512_DIGEST_SIZE], t[SHA512_DIGEST_SIZE];
	unsigned char   counter[4];
	unsigned long   block;
	size_t          i, n;
	int             j;

	sha512_hmac_init(&base, pwd, pwd_len);

	for (block = 1; dklen != 0; block++)
	{
		/* U_1 = PRF(P, S || INT_BE32(block)) */
		counter[0] = (unsigned char)(block >> 24);
		counter[1] = (unsigned char)(block >> 16);
		counter[2] = (unsigned char)(block >> 8);
		counter[3] = (unsigned char)(block);

		ctx = base;
		sha512_hmac_hash(&ctx, salt, salt_len);
		sha512_hmac_hash(&ctx, counter, sizeof(counter));
		sha512_hmac_done(&ctx, u);
		memcpy(t, u, SHA512_DIGEST_SIZE);

		/* T = U_1 ^ U_2 ^ ... ^ U_c */
		for (j = 1; j < i_count; j++)
		{
			ctx = base;
			sha512_hmac_hash(&ctx, u, SHA512_DIGEST_SIZE);
			sha512_hmac_done(&ctx, u);

			for (i = 0; i < SHA512_DIGEST_SIZE; i++) t[i] ^= u[i];
		}

		n = (dklen < SHA512_DIGEST_SIZE) ? dklen : SHA512_DIGEST_SIZE;
		memcpy(dk, t, n);
		dk += n; dklen -= n;
	}

	memset(&base, 0, sizeof(base));
	memset(&ctx,  0, sizeof(ctx));
	memset(u, 0, sizeof(u));
	memset(t, 0, sizeof(t));
}
