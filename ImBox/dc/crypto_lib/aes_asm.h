/*
    * DiskCryptor crypto_lib - AES-256 block core selection
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Resolves the single-block AES-256 primitives to the best core available
    * for the target. On amd64 that is the Gladman assembler core in
    * amd64/aes_amd64.nasm; on ARM64 it is the crypto-extension path in
    * xts_aes_ce.c. Both consume the key schedule built by aes256_set_key().
*/
#ifndef _CL_AES_ASM_H_
#define _CL_AES_ASM_H_

#include "aes_key.h"

#define aes256_asm_set_key aes256_set_key

#if defined(_M_ARM64)

 void _stdcall aes256_arm64_encrypt(const unsigned char *in, unsigned char *out, aes256_key *key);
 void _stdcall aes256_arm64_decrypt(const unsigned char *in, unsigned char *out, aes256_key *key);

 #define aes256_asm_encrypt aes256_arm64_encrypt
 #define aes256_asm_decrypt aes256_arm64_decrypt

#elif defined(_M_X64)

 void _stdcall aes256_asm_encrypt(const unsigned char *in, unsigned char *out, aes256_key *key);
 void _stdcall aes256_asm_decrypt(const unsigned char *in, unsigned char *out, aes256_key *key);

#else
 #error crypto_lib supports amd64 and ARM64 only
#endif

#endif
