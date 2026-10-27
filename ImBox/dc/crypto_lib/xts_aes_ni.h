/*
    * SPDX-License-Identifier: MIT
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
*/
#ifndef _CL_XTS_AES_NI_H_
#define _CL_XTS_AES_NI_H_

/* Declared here so the prototypes below do not introduce it inside a
   parameter list, which is a warning EDK2 builds treat as an error. */
struct _xts_key;

void _stdcall xts_aes_ni_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, struct _xts_key *key);
void _stdcall xts_aes_ni_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, struct _xts_key *key);

#endif
