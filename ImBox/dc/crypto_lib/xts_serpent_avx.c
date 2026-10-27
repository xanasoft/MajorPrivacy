/*
    * DiskCryptor crypto_lib - XTS-Serpent, AVX (four blocks, VEX-encoded)
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * The same four-block core as the SSE2 build, compiled with /arch:AVX so the
    * intrinsics come out VEX-encoded. This is still 128-bit and still four
    * blocks wide - the gain is purely that VEX's three-operand form removes the
    * register-copy moves that two-operand SSE needs, which Serpent's bitsliced
    * S-boxes generate a lot of.
    *
    * This only matters on parts with AVX but not AVX2 (Sandy/Ivy Bridge,
    * Bulldozer/Piledriver). Anything newer takes the AVX2 core instead.
    *
    * Built only when CL_ENABLE_AVX is defined, and then needs /arch:AVX. MSVC
    * rejects /arch:AVX together with /kernel, so the driver build omits this
    * and stays on the SSE2 core.
*/
#if defined(_M_X64) && defined(CL_ENABLE_AVX)

#include "cl_platform.h"
#include "cl_cpu.h"
#include "xts_serpent_avx.h"

#define CL_SP_PREFIX xts_serpent_avx_
#include "xts_serpent_x86.inl"

int _stdcall xts_serpent_avx_available(void)
{
	/*
	 * The probe itself lives in cl_cpu.c, which is compiled without /arch:AVX.
	 * Asking "can this CPU run AVX" from a translation unit the compiler may
	 * emit AVX into is a trap worth avoiding.
	 */
	return cl_cpu_has_avx();
}

#endif /* _M_X64 && CL_ENABLE_AVX */
