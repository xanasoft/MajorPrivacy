/*
    * DiskCryptor crypto_lib - XTS-AES on ARM64 crypto extensions
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
*/
#ifndef _CL_XTS_SERPENT_NEON_H_
#define _CL_XTS_SERPENT_NEON_H_

#ifdef _M_ARM64

#include "xts_fast.h"

int  _stdcall xts_serpent_neon_available(void);
void _stdcall xts_serpent_neon_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key);
void _stdcall xts_serpent_neon_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, xts_key *key);

#endif /* _M_ARM64 */

#endif /* _CL_XTS_SERPENT_NEON_H_ */

