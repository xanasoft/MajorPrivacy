/*
    * DiskCryptor crypto_lib - XTS-AES on ARM64 crypto extensions
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
*/
#ifndef _CL_XTS_AES_CE_H_
#define _CL_XTS_AES_CE_H_

#ifdef _M_ARM64

#include "xts_fast.h"

int  _stdcall xts_aes_ce_available(void);
void _stdcall xts_aes_ce_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key);
void _stdcall xts_aes_ce_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key);

void _stdcall aes256_arm64_encrypt(const unsigned char *in, unsigned char *out, aes256_key *key);
void _stdcall aes256_arm64_decrypt(const unsigned char *in, unsigned char *out, aes256_key *key);

#endif /* _M_ARM64 */

#endif /* _CL_XTS_AES_CE_H_ */

