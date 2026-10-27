/*
    * SPDX-License-Identifier: MIT
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * The AVX2 Serpent core exists only in builds that define CL_ENABLE_AVX2
    * (user mode). MSVC will not accept /arch:AVX2 alongside /kernel, so the
    * driver build omits it and the dispatcher stays on the SSE2 core.
*/
#ifndef _CL_XTS_SERPENT_AVX2_H_
#define _CL_XTS_SERPENT_AVX2_H_

#if defined(_M_X64) && defined(CL_ENABLE_AVX2)

int  _stdcall xts_serpent_avx2_available(void);
/* Declared here so the prototypes below do not introduce it inside a
   parameter list, which is a warning EDK2 builds treat as an error. */
struct _xts_key;

void _stdcall xts_serpent_avx2_encrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, struct _xts_key *key);
void _stdcall xts_serpent_avx2_decrypt(const unsigned char *in, unsigned char *out, size_t len, unsigned __int64 offset, struct _xts_key *key);

#endif
#endif
