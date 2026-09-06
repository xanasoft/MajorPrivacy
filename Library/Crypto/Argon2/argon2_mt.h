/*
 * Argon2 multi-threading support for DiskCryptor
 * Copyright (c) 2026 DiskCryptor
 *
 * Provides threading primitives for Argon2 parallel lane processing.
 * Supports both kernel mode (IS_DRIVER) and user mode builds.
 */

#ifndef ARGON2_MT_H
#define ARGON2_MT_H

#include "defines.h"

typedef HANDLE argon2_thread_handle_t;
typedef unsigned (__stdcall *argon2_thread_func)(void *);

int argon2_thread_create(argon2_thread_handle_t *handle,
                         argon2_thread_func func, void *arg);
int argon2_thread_join(argon2_thread_handle_t handle);
void argon2_thread_exit(void);

#endif /* ARGON2_MT_H */
