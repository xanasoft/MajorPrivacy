/*
    * DiskCryptor crypto_lib - secure allocation hook
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Argon2 is the only part of crypto_lib that allocates, and it wants memory
    * that will not be paged out or left in the clear. Rather than hard-wire
    * DiskCryptor's allocator, route it through these two macros so the library
    * can be dropped into another host: define CL_SECURE_ALLOC and
    * CL_SECURE_FREE before building and nothing here applies.
    *
    * With neither defined, the defaults reproduce DiskCryptor's own behaviour -
    * non-paged pool in the driver, secure_alloc()/secure_free() in user mode.
    *
    *
    * WHY THE USER-MODE PAIR IS DECLARED HERE
    *
    * This header used to reach up into ../dcapi/include/misc.h for them, which
    * was wrong twice over.
    *
    * It made an MIT library that is meant to stand alone depend on a header
    * belonging to one of its GPL consumers - so the dependency pointed the
    * wrong way, and crypto_lib could not be built outside this tree.
    *
    * And that header decorates its declarations with dc_api, which is
    * __declspec(dllimport) unless DCAPI_DLL is defined - which it is not when
    * crypto_lib is built. So Argon2 compiled a dllimport reference to a symbol
    * dcapi then defines locally, and linking dcapi.dll produced:
    *
    *     LNK4217: symbol 'secure_alloc' defined in 'misc.obj' is imported by
    *              'crypto_lib.lib(argon2.obj)' in function 'argon2_hash'
    *
    * It linked, because the linker patches the indirection out, but every call
    * went through an import thunk that should never have existed. dcapi's
    * definitions carry no dc_api either, so they were never exported in the
    * first place; only the declaration claimed otherwise.
    *
    * Declared plainly here instead. The host supplies the definitions at link
    * time - the same arrangement volume_lib uses for its dc_host hooks, and the
    * signatures match dcapi/misc.c exactly.
*/
#ifndef _CL_ALLOC_H_
#define _CL_ALLOC_H_

#include "cl_platform.h"

#if !defined(CL_SECURE_ALLOC) || !defined(CL_SECURE_FREE)

 #if CL_KERNEL

  /*
   * ExAllocatePoolWithTag comes from ntifs.h, which cl_platform.h includes.
   *
   * It is deprecated in favour of ExAllocatePool2, and the kernel Debug build
   * treats the deprecation warning as an error. The driver made the same call
   * and suppresses it in driver/include/misc_mem.h; this header used to pick
   * that suppression up by including misc_mem.h, which is not something a
   * library should rely on a consumer for. So it is stated here instead.
   *
   * Moving to ExAllocatePool2 is a decision about which Windows versions the
   * driver still runs on - it needs a 2004-or-later target - and so is the
   * driver's to make, not something to change while tidying an include.
   *
   * TU-scoped by necessity: the macro expands at its call site, so the pragma
   * has to be in effect there.
   */
  #pragma warning(disable: 4996)

  #define CL_SECURE_ALLOC(s) ExAllocatePoolWithTag(NonPagedPool, (unsigned long)(s), 'A_CD')
  #define CL_SECURE_FREE(p)  ExFreePoolWithTag((p), 'A_CD')

 #else

  /* Provided by the host: dcapi/misc.c in DiskCryptor, crypto_lib/tests/stub_alloc.c
     in the test harnesses. No calling convention and no dll decoration - it is a
     static library, and both definitions are plain cdecl. */
  void *secure_alloc(unsigned long length);
  void  secure_free(void *ptr);

  #define CL_SECURE_ALLOC(s) secure_alloc((unsigned long)(s))
  #define CL_SECURE_FREE(p)  secure_free(p)

 #endif

#endif

#endif
