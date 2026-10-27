/*
    * DCrypt volume_lib - internal host access
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * What the library calls instead of the driver facilities it used to link
    * against by name. Not part of the public interface - a consumer installs a
    * dc_host (see dc_dev.h) and never sees these.
    *
    * The names are short on purpose: the substitution that brought this code
    * out of the driver was a rename at every call site, and short names keep
    * those lines the same length and the same shape they were.
    *
    *   mm_secure_alloc  ->  dc_alloc
    *   mm_secure_free   ->  dc_free
    *   cp_rand_bytes    ->  dc_rand
    *   cp_rand_add_seed ->  dc_rand_seed
    *   dc_wipe_process  ->  dc_wipe
    *   DbgMsg           ->  DbgMsg   (kept; now a macro onto dc_log)
*/
#ifndef _DC_INTERNAL_H_
#define _DC_INTERNAL_H_

#include "defines.h"
#include "dc_status.h"
#include "dc_dev.h"

/*
 * Allocation fit to hold key material: locked against paging where the
 * platform can do it, wiped on release. dc_free(NULL) is a no-op, which the
 * moved code relies on in its cleanup paths.
 */
void *dc_alloc(size_t size);
void  dc_free(void *p);

void  dc_rand(void *buff, size_t size);

/* No-op when the host did not supply rand_add_seed. */
void  dc_rand_seed(const void *buff, size_t size);

/* No-op when the host supplied no wipe hook, or when wipe_ctx is NULL. */
void  dc_wipe(void *wipe_ctx, u64 offset, int size);

/*
 * Diagnostics. Kept under the driver's name so that the 35 call sites in the
 * moved code did not have to change at all.
 */
void  dc_log(const char *fmt, ...);

#ifdef DbgMsg
 #undef DbgMsg
#endif
#define DbgMsg dc_log

#endif /* _DC_INTERNAL_H_ */
