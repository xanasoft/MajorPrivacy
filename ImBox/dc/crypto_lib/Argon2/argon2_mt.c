/* SPDX-License-Identifier: MIT */
/*
 * Argon2 multi-threading support for DiskCryptor
 * Copyright (c) DavidXanatos <info@diskcryptor.org>
 *
 * Provides threading primitives for Argon2 parallel lane processing.
 * Supports kernel mode (IS_DRIVER), user mode, and UEFI builds - the last
 * through EFI_MP_SERVICES_PROTOCOL, which is the only way Argon2 gets more
 * than one core in firmware.
 */

#include "argon2_mt.h"

#if defined(_UEFI)

#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>

/* Atomic increment - architecture-specific implementations */
#ifdef _M_ARM64
/* ARM64: Use EDK2 SynchronizationLib */
#include <Library/SynchronizationLib.h>
#define AtomicIncrement(x) ((long)InterlockedIncrement((UINT32 volatile *)(x)))
#else
/* x64: Use _InterlockedIncrement directly */
long _InterlockedIncrement(long volatile *);
#pragma intrinsic(_InterlockedIncrement)
#define AtomicIncrement(x) _InterlockedIncrement(x)
#endif

/* Set to 1 to enable console output for debugging MT issues */
#define ARGON2_MT_VERBOSE 0

/*
 * UEFI implementation using EFI_MP_SERVICES_PROTOCOL.
 *
 * After ReadyToBoot, non-blocking mode is not supported. We use a work-queue
 * approach with StartupAllAPs in blocking mode:
 * - argon2_thread_create() queues work items
 * - First argon2_thread_join() dispatches all work via StartupAllAPs
 * - All APs pull work from queue atomically and execute
 * - BSP also processes work after StartupAllAPs returns
 * - Subsequent joins are no-ops (work already done)
 */

static EFI_MP_SERVICES_PROTOCOL *gMpServices = NULL;
static UINTN gNumProcessors = 0;
static UINTN gNumEnabledProcessors = 0;
static UINTN gNumAPs = 0;          /* Number of available APs */

/*
 * Work queue for parallel execution
 */
#define MAX_WORK_ITEMS 64

typedef struct {
    argon2_thread_func func;
    void *arg;
} work_item_t;

static volatile long gWorkQueueHead = 0;     /* Next item to dequeue (atomic) */
static UINT32 gWorkQueueTail = 0;            /* Next slot to enqueue */
static work_item_t gWorkQueue[MAX_WORK_ITEMS];
static BOOLEAN gWorkDispatched = FALSE;      /* TRUE after StartupAllAPs called */

int argon2_mt_init(void)
{
    EFI_STATUS status;

    if (gMpServices != NULL) {
        //DEBUG((DEBUG_INFO, "Argon2 MT: Already initialized, APs=%d\n", gNumAPs));
        return 0; /* Already initialized */
    }

    //DEBUG((DEBUG_INFO, "Argon2 MT: Initializing multi-threading support\n"));

    /* Locate MP Services Protocol */
    status = gBS->LocateProtocol(
        &gEfiMpServiceProtocolGuid,
        NULL,
        (VOID **)&gMpServices);

    if (EFI_ERROR(status)) {
        //DEBUG((DEBUG_ERROR, "Argon2 MT: LocateProtocol failed: %r\n", status));
#if ARGON2_MT_VERBOSE
        Print(L"Argon2 MT: MP Services not available (%r)\r\n", status);
#endif
        gMpServices = NULL;
        return -1; /* LocateProtocol failed */
    }

    //DEBUG((DEBUG_INFO, "Argon2 MT: MP Services Protocol located\n"));

    /* Get processor counts */
    status = gMpServices->GetNumberOfProcessors(
        gMpServices,
        &gNumProcessors,
        &gNumEnabledProcessors);

    if (EFI_ERROR(status)) {
        //DEBUG((DEBUG_ERROR, "Argon2 MT: GetNumberOfProcessors failed: %r\n", status));
        gMpServices = NULL;
        return -2; /* GetNumberOfProcessors failed */
    }

    /* APs = enabled processors - 1 (BSP) */
    gNumAPs = (gNumEnabledProcessors > 1) ? (gNumEnabledProcessors - 1) : 0;

    //DEBUG((DEBUG_ERROR, "Argon2 MT: Total=%d, Enabled=%d, APs=%d\n",
    //       gNumProcessors, gNumEnabledProcessors, gNumAPs));

#if ARGON2_MT_VERBOSE
    Print(L"Argon2 MT: %d processors, %d APs available\r\n", gNumProcessors, gNumAPs);
#endif

    return 0;
}

int argon2_mt_get_num_processors(void)
{
    if (gMpServices == NULL) {
        if (argon2_mt_init() != 0) {
            return 1; /* Fall back to single processor */
        }
    }

    /* Return total usable processors (APs + BSP) */
    return (int)(gNumAPs + 1);
}

/*
 * Get detailed MT status for debugging.
 * Returns: 0 = MT available, negative = error code indicating failure point
 */
int argon2_mt_get_status(UINTN *out_num_processors, UINTN *out_num_aps)
{
    if (gMpServices == NULL) {
        int init_result = argon2_mt_init();
        if (init_result != 0) {
            if (out_num_processors) *out_num_processors = 1;
            if (out_num_aps) *out_num_aps = 0;
            return init_result;
        }
    }

    if (out_num_processors) *out_num_processors = gNumProcessors;
    if (out_num_aps) *out_num_aps = gNumAPs;

    return (gNumAPs == 0) ? -3 : 0;
}

/*
 * Worker function that runs on each AP.
 * Pulls work items from queue and executes them.
 */
static VOID EFIAPI ap_worker(VOID *unused)
{
    (VOID)unused;

    for (;;) {
        long idx;
        work_item_t *item;

        /* Atomically get next work item index */
        idx = AtomicIncrement(&gWorkQueueHead) - 1;

        /* Check if there's work to do */
        if (idx >= (long)gWorkQueueTail) {
            break; /* No more work */
        }

        /* Execute the work item */
        item = &gWorkQueue[idx];
        if (item->func != NULL) {
            item->func(item->arg);
        }
    }
}

/*
 * Handle values:
 * 0 = invalid
 * 1..MAX_WORK_ITEMS = queued work item index + 1
 * DISPATCHED_HANDLE = work was already dispatched
 */
#define DISPATCHED_HANDLE ((UINTN)-1)

int argon2_thread_create(argon2_thread_handle_t *handle,
                         argon2_thread_func func, void *arg)
{
    if (handle == NULL || func == NULL) {
        return -1;
    }

    /* Initialize MP services if needed */
    if (gMpServices == NULL) {
        argon2_mt_init();
    }

    /* If no APs available, run synchronously */
    if (gMpServices == NULL || gNumAPs == 0) {
        //DEBUG((DEBUG_INFO, "Argon2 MT: No APs, running synchronously\n"));
        func(arg);
        *handle = DISPATCHED_HANDLE;
        return 0;
    }

    /* Reset queue if this is the first item after a dispatch */
    if (gWorkDispatched) {
        gWorkQueueHead = 0;
        gWorkQueueTail = 0;
        gWorkDispatched = FALSE;
    }

    /* Add work to queue */
    if (gWorkQueueTail >= MAX_WORK_ITEMS) {
        //DEBUG((DEBUG_ERROR, "Argon2 MT: Work queue full\n"));
        /* Run synchronously as fallback */
        func(arg);
        *handle = DISPATCHED_HANDLE;
        return 0;
    }

    gWorkQueue[gWorkQueueTail].func = func;
    gWorkQueue[gWorkQueueTail].arg = arg;
    *handle = gWorkQueueTail + 1; /* Handle is 1-based index */
    gWorkQueueTail++;

    //DEBUG((DEBUG_INFO, "Argon2 MT: Queued work item %d\n", gWorkQueueTail));

    return 0;
}

int argon2_thread_join(argon2_thread_handle_t handle)
{
    EFI_STATUS status;

    /* Already dispatched or invalid */
    if (handle == DISPATCHED_HANDLE || handle == 0) {
        return 0;
    }

    /* Only dispatch once - on first join call */
    if (!gWorkDispatched && gWorkQueueTail > 0) {
        //DEBUG((DEBUG_ERROR, "Argon2 MT: Dispatching %d work items to %d APs\n",
        //       gWorkQueueTail, gNumAPs));

#if ARGON2_MT_VERBOSE
        Print(L"Argon2 MT: Dispatching %d items to %d APs\r\n", gWorkQueueTail, gNumAPs);
#endif

        /* Start all APs running the worker function */
        status = gMpServices->StartupAllAPs(
            gMpServices,
            ap_worker,
            FALSE,          /* SingleThread = FALSE (run on all APs simultaneously) */
            NULL,           /* WaitEvent = NULL (blocking mode) */
            0,              /* Timeout = 0 (infinite) */
            NULL,           /* ProcedureArgument */
            NULL);          /* FailedCpuList */

        if (EFI_ERROR(status) && status != EFI_NOT_STARTED) {
            //DEBUG((DEBUG_ERROR, "Argon2 MT: StartupAllAPs failed: %r\n", status));
#if ARGON2_MT_VERBOSE
            Print(L"Argon2 MT: StartupAllAPs failed: %r\r\n", status);
#endif
        }

        /* BSP also processes remaining work items */
        ap_worker(NULL);

        gWorkDispatched = TRUE;

        //DEBUG((DEBUG_ERROR, "Argon2 MT: All work completed\n"));
    }

    return 0;
}

void argon2_thread_exit(void)
{
    /*
     * In UEFI, the AP procedure simply returns.
     * No special exit handling needed.
     */
}

#elif defined(IS_DRIVER)

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

#endif /* _UEFI / IS_DRIVER */

