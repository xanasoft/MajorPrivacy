/*
 * Argon2 multi-threading support for DiskCryptor
 * Copyright (c) DavidXanatos <info@diskcryptor.org>
 *
 * Provides threading primitives for Argon2 parallel lane processing.
 * Supports both kernel mode (IS_DRIVER) and user mode builds.
 */

#include "argon2_mt.h"

#ifdef IS_DRIVER

/*
 * Kernel mode implementation using NT kernel threading APIs.
 *
 * Note: The thread function is cast to PKSTART_ROUTINE. This is safe because:
 * 1. NTAPI and __stdcall use identical calling conventions on Windows
 * 2. The thread function calls argon2_thread_exit() which invokes
 *    PsTerminateSystemThread() - this never returns, so the return
 *    type mismatch (void vs unsigned) is irrelevant
 */

int argon2_thread_create(argon2_thread_handle_t *handle,
                         argon2_thread_func func, void *arg)
{
    OBJECT_ATTRIBUTES obj_attr;
    NTSTATUS status;

    if (handle == NULL || func == NULL) {
        return -1;
    }

    InitializeObjectAttributes(&obj_attr, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

    status = PsCreateSystemThread(
        handle,
        THREAD_ALL_ACCESS,
        &obj_attr,
        NULL,
        NULL,
        (PKSTART_ROUTINE)func,
        arg);

    return NT_SUCCESS(status) ? 0 : -1;
}

int argon2_thread_join(argon2_thread_handle_t handle)
{
    PKTHREAD thread_obj;
    NTSTATUS status;

    if (handle == NULL) {
        return -1;
    }

    /* Get thread object from handle for waiting */
    status = ObReferenceObjectByHandle(
        handle,
        THREAD_ALL_ACCESS,
        *PsThreadType,
        KernelMode,
        (PVOID *)&thread_obj,
        NULL);

    if (!NT_SUCCESS(status)) {
        ZwClose(handle);
        return -1;
    }

    /* Wait for thread completion */
    KeWaitForSingleObject(thread_obj, Executive, KernelMode, FALSE, NULL);

    /* Cleanup */
    ObDereferenceObject(thread_obj);
    ZwClose(handle);

    return 0;
}

void argon2_thread_exit(void)
{
    PsTerminateSystemThread(STATUS_SUCCESS);
}

#else /* User mode */

#include <process.h>

int argon2_thread_create(argon2_thread_handle_t *handle,
                         argon2_thread_func func, void *arg)
{
    uintptr_t h;

    if (handle == NULL || func == NULL) {
        return -1;
    }

    h = _beginthreadex(NULL, 0, func, arg, 0, NULL);
    if (h == 0) {
        return -1;
    }

    *handle = (HANDLE)h;
    return 0;
}

int argon2_thread_join(argon2_thread_handle_t handle)
{
    DWORD result;

    if (handle == NULL) {
        return -1;
    }

    result = WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);

    return (result == WAIT_OBJECT_0) ? 0 : -1;
}

void argon2_thread_exit(void)
{
    _endthreadex(0);
}

#endif /* IS_DRIVER */
