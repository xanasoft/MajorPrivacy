/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     jxy-s   2022
 *
 */

#pragma once

#ifdef _KERNEL_MODE
#define PHNT_MODE PHNT_MODE_KERNEL
#endif
#pragma warning(push)
#pragma warning(disable : 4201)

// Process

typedef ULONG KPH_PROCESS_STATE;
typedef KPH_PROCESS_STATE* PKPH_PROCESS_STATE;

#define KPH_PROCESS_SECURELY_CREATED                     0x00000001ul
#define KPH_PROCESS_VERIFIED_PROCESS                     0x00000002ul
#define KPH_PROCESS_PROTECTED_PROCESS                    0x00000004ul
#define KPH_PROCESS_NO_UNTRUSTED_IMAGES                  0x00000008ul
#define KPH_PROCESS_HAS_FILE_OBJECT                      0x00000010ul
#define KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS          0x00000020ul
#define KPH_PROCESS_NO_USER_WRITABLE_REFERENCES          0x00000040ul
#define KPH_PROCESS_NO_FILE_TRANSACTION                  0x00000080ul
#define KPH_PROCESS_NOT_BEING_DEBUGGED                   0x00000100ul


#define KPH_PROCESS_STATE_MINIMUM (KPH_PROCESS_HAS_FILE_OBJECT              |\
                                   KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS  |\
                                   KPH_PROCESS_NO_USER_WRITABLE_REFERENCES  |\
                                   KPH_PROCESS_NO_FILE_TRANSACTION)

#define KPH_PROCESS_STATE_LOW     (KPH_PROCESS_STATE_MINIMUM                |\
                                   KPH_PROCESS_VERIFIED_PROCESS)


#define KPH_PROCESS_STATE_MEDIUM  (KPH_PROCESS_STATE_LOW                    |\
                                   KPH_PROCESS_PROTECTED_PROCESS)

#define KPH_PROCESS_STATE_HIGH    (KPH_PROCESS_STATE_MEDIUM                 |\
                                   KPH_PROCESS_NO_UNTRUSTED_IMAGES          |\
                                   KPH_PROCESS_NOT_BEING_DEBUGGED)

#define KPH_PROCESS_STATE_MAXIMUM (KPH_PROCESS_STATE_HIGH                   |\
                                   KPH_PROCESS_SECURELY_CREATED)

// Verification

#define KPH_PROCESS_READ_ACCESS   (STANDARD_RIGHTS_READ                 |\
                                   SYNCHRONIZE                          |\
                                   PROCESS_QUERY_INFORMATION            |\
                                   PROCESS_QUERY_LIMITED_INFORMATION    |\
                                   PROCESS_VM_READ)

#define KPH_THREAD_READ_ACCESS    (STANDARD_RIGHTS_READ                 |\
                                   SYNCHRONIZE                          |\
                                   THREAD_QUERY_INFORMATION             |\
                                   THREAD_QUERY_LIMITED_INFORMATION     |\
                                   THREAD_GET_CONTEXT)

#pragma warning(pop)
