/*
    * DiskCryptor crypto_lib - PBKDF2-HMAC-SHA-512
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
*/
#ifndef _CL_SHA512_PKCS5_2_H_
#define _CL_SHA512_PKCS5_2_H_

#include "cl_platform.h"

void _stdcall sha512_pkcs5_2(int i_count, const void *pwd, size_t pwd_len,
                             const void *salt, size_t salt_len,
                             unsigned char *dk, size_t dklen);

#endif
