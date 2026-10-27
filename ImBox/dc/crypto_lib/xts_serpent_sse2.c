/*
    * DiskCryptor crypto_lib - XTS-Serpent, SSE2 (four blocks in parallel)
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * The baseline amd64 Serpent core. SSE2 is architectural on x86-64, so this
    * is a floor rather than a tier that can fail to be available. The body
    * lives in xts_serpent_x86.inl, shared with the VEX-encoded build.
*/
#if defined(_M_X64)

#include "xts_serpent_sse2.h"

#define CL_SP_PREFIX xts_serpent_sse2_
#include "xts_serpent_x86.inl"

int _stdcall xts_serpent_sse2_available(void)
{
	return 1;
}

#endif /* _M_X64 */
