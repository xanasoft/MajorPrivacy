/*
    * DiskCryptor crypto_lib - CRC-32 (IEEE 802.3, reflected)
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
*/
#ifndef _CL_CRC32_H_
#define _CL_CRC32_H_

unsigned long _stdcall crc32(const unsigned char *p, unsigned long len);
unsigned long _stdcall crc32_combine(unsigned long crc1, unsigned long crc2, unsigned long len2);

#endif
