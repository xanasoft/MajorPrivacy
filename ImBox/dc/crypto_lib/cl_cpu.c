/*
    * DiskCryptor crypto_lib - CPU feature probes
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * See cl_cpu.h. This file must be compiled WITHOUT any /arch switch.
*/
#include "cl_platform.h"
#include "cl_cpu.h"

#if defined(_M_X64)

/* OSXSAVE + AVX feature bit + OS-enabled XMM/YMM state */
static int cl_avx_state_enabled(void)
{
	int info[4];

	__cpuid(info, 1);
	if ((info[2] & (1 << 27)) == 0) return 0;    /* OSXSAVE */
	if ((info[2] & (1 << 28)) == 0) return 0;    /* AVX */

	/*
	 * The OS must have enabled XMM and YMM state, otherwise the upper halves
	 * are not preserved across a context switch and the instruction faults.
	 */
#if CL_KERNEL
	return (RtlGetEnabledExtendedFeatures(XSTATE_MASK_GSSE) & XSTATE_MASK_GSSE) != 0;
#else
	return (_xgetbv(0) & 0x6) == 0x6;
#endif
}

int _stdcall cl_cpu_has_avx(void)
{
	return cl_avx_state_enabled();
}

int _stdcall cl_cpu_has_avx2(void)
{
	int info[4];

	__cpuid(info, 0);
	if (info[0] < 7) return 0;                   /* leaf 7 not supported */

	if (cl_avx_state_enabled() == 0) return 0;

	__cpuidex(info, 7, 0);
	return (info[1] & (1 << 5)) != 0;            /* AVX2 */
}

#endif /* _M_X64 */
