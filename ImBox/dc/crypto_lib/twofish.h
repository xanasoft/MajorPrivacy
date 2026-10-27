/*
    * DiskCryptor crypto_lib - Twofish-256
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
*/
#ifndef _CL_TWOFISH_H_
#define _CL_TWOFISH_H_

#define TWOFISH_KEY_SIZE   32
#define TWOFISH_BLOCK_SIZE 16

/*
 * s[] holds the four key-dependent S-boxes already composed with the MDS
 * matrix, so the round function g() is four table lookups and three xors.
 * w[] is the input/output whitening material (K0..K7), k[] the 32 round
 * subkeys (K8..K39).
 */
typedef struct _twofish256_key {
	unsigned long s[4][256], w[8], k[32];
} twofish256_key;

void _stdcall twofish256_set_key(const unsigned char *key, twofish256_key *skey);
void _stdcall twofish256_encrypt(const unsigned char *in, unsigned char *out, twofish256_key *key);
void _stdcall twofish256_decrypt(const unsigned char *in, unsigned char *out, twofish256_key *key);

#endif
