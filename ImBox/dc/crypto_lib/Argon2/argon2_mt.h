/* SPDX-License-Identifier: MIT */
/*
 * Argon2 multi-threading support for DiskCryptor
 * Copyright (c) 2026 DiskCryptor
 *
 * Provides threading primitives for Argon2 parallel lane processing.
 * Supports kernel mode (IS_DRIVER), user mode, and UEFI builds - the last
 * through EFI_MP_SERVICES_PROTOCOL, which is the only way Argon2 gets more
 * than one core in firmware.
 */

#ifndef ARGON2_MT_H
#define ARGON2_MT_H

#include "defines.h"

#if defined(_UEFI)

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>
#include <Protocol/MpService.h>

/*
 * UEFI uses MP Services Protocol for multi-processor support.
 * The "handle" stores an EFI_EVENT pointer (cast to UINTN) for completion tracking.
 */
typedef UINTN argon2_thread_handle_t;
typedef VOID (EFIAPI *argon2_thread_func)(VOID *);

/* Initialize MP services - must be called before using threading */
int argon2_mt_init(void);

/* Get number of available processors */
int argon2_mt_get_num_processors(void);

/* Get detailed MT status for debugging */
int argon2_mt_get_status(UINTN *out_num_processors, UINTN *out_num_aps);

#else /* Windows kernel or user mode */

typedef HANDLE argon2_thread_handle_t;
typedef unsigned (__stdcall *argon2_thread_func)(void *);

#endif

int argon2_thread_create(argon2_thread_handle_t *handle,
                         argon2_thread_func func, void *arg);
int argon2_thread_join(argon2_thread_handle_t handle);
void argon2_thread_exit(void);

#endif /* ARGON2_MT_H */

