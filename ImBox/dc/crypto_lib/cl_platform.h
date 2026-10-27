/*
    * DiskCryptor crypto_lib - host environment shims
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * crypto_lib builds unchanged for user mode and for kernel mode. The only
    * thing that genuinely differs is how wide vector state is claimed: on amd64
    * the SSE registers are part of the normal kernel register set and need no
    * ceremony, but the upper halves of the YMM registers do - a driver must ask
    * the kernel to preserve them around any AVX code.
    *
    * CL_KERNEL is 1 when building for kernel mode. IS_DRIVER is accepted as
    * well as the toolset's own _KERNEL_MODE so the library drops into the
    * existing DiskCryptor projects without extra defines.
*/
#ifndef _CL_PLATFORM_H_
#define _CL_PLATFORM_H_

#if defined(_KERNEL_MODE) || defined(IS_DRIVER)
 #define CL_KERNEL 1
 #include <ntifs.h>
#else
 #define CL_KERNEL 0
#endif

/*
 * Structured exception handling. User mode and kernel mode both have it;
 * EDK2 does not, and the EFI build defines _UEFI. Only one thing in the
 * library uses it - the Hyper-V AES probe in xts_fast.c.
 */
#if defined(_UEFI) || defined(CL_NO_SEH)
 #define CL_SEH 0
#else
 #define CL_SEH 1
#endif

/*
 * Standard headers, chosen once for the whole library. crypto_lib sources
 * include this file rather than <string.h>/<memory.h>/<stddef.h> directly.
 *
 * EDK2 has no C runtime. The EFI host puts its own <intrin.h> on the include
 * path - a shim that supplies the UEFI types, the rotate intrinsics, and
 * memcpy/memset mapped onto CopyMem/SetMem - and that one header covers what
 * <stddef.h> and <string.h> provide elsewhere. Including the real CRT headers
 * as well would be worse than useless: the shim turns memcpy and memset into
 * macros, and <string.h> would then try to declare through them.
 *
 * Off EFI, the real headers. MSVC declares _rotl/_rotr/_rotr64 and
 * _byteswap_uint64 for every target, ARM64 included, so <intrin.h> is
 * unconditional there.
 */
#if defined(_UEFI)
 #include <intrin.h>
#else
 #include <stddef.h>
 #include <string.h>
 #if defined(_MSC_VER)
  #include <intrin.h>
 #endif
#endif

/*
 * Claim/release AVX state. In user mode the OS preserves it for us and these
 * are no-ops. In kernel mode KeSaveExtendedProcessorState can fail, in which
 * case the caller must fall back to an SSE2-only path.
 */
#if CL_KERNEL && defined(_M_X64)

 #define CL_AVX_STATE      XSTATE_SAVE _cl_xstate
 #define CL_AVX_ACQUIRE()  NT_SUCCESS(KeSaveExtendedProcessorState(XSTATE_MASK_GSSE, &_cl_xstate))
 #define CL_AVX_RELEASE()  KeRestoreExtendedProcessorState(&_cl_xstate)

#else

 #define CL_AVX_STATE      int _cl_xstate = 0
 #define CL_AVX_ACQUIRE()  (1)
 #define CL_AVX_RELEASE()  ((void)_cl_xstate)

#endif

#endif
