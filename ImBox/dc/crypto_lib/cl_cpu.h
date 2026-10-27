/*
    * DiskCryptor crypto_lib - CPU feature probes
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * These live in their own translation unit deliberately. A probe has to be
    * callable on a CPU that lacks the feature it reports on, so it must never
    * be compiled with the /arch switch that enables that feature - otherwise
    * the compiler is free to emit the very instructions the caller is asking
    * about, and the probe faults instead of returning 0. cl_cpu.c is therefore
    * built with no /arch switch at all.
    *
    * Each probe answers the only question worth asking: can this code run the
    * instructions right now. That means the CPUID feature bit, plus OSXSAVE,
    * plus confirmation that the OS has actually enabled the register state -
    * without that last part a VEX instruction faults on an OS that never
    * turned the state on.
*/
#ifndef _CL_CPU_H_
#define _CL_CPU_H_

#if defined(_M_X64)

/* AVX present and YMM state enabled by the OS. VEX-128 needs this too: the
   encoding zeroes the upper halves, so it counts as touching them. */
int _stdcall cl_cpu_has_avx(void);

/* As above, and AVX2 present. */
int _stdcall cl_cpu_has_avx2(void);

#endif

#endif
