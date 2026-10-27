/*
    * DiskCryptor crypto_lib - AES-256 key schedule
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
*/
#ifndef _CL_AES_KEY_H_
#define _CL_AES_KEY_H_

#define AES_ROUNDS     14
#define AES_KEY_SIZE   32
#define AES_BLOCK_SIZE 16

/* kept for source compatibility with the old crypto_fast headers */
#define ROUNDS AES_ROUNDS

/*
 * enc_key holds the standard AES-256 expanded key W[0..59].
 * dec_key holds the same material with the round-key groups in reverse order
 * and InvMixColumns applied to all but the first and last group - the
 * "equivalent inverse cipher" schedule expected by the amd64 assembler core
 * (its AES_REV_DKS build option) and by the ARM64 crypto-extension path.
 */
typedef __declspec(align(16)) struct _aes256_key {
	__declspec(align(16)) unsigned long enc_key[4 * (AES_ROUNDS + 1)];
	__declspec(align(16)) unsigned long dec_key[4 * (AES_ROUNDS + 1)];
} aes256_key;

void _stdcall aes256_set_key(const unsigned char *key, aes256_key *skey);

#endif
