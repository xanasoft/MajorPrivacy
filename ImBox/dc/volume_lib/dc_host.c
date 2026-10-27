/*
    * DCrypt volume_lib - host installation
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
*/
#include <stdarg.h>

#include "defines.h"
#include "dc_dev.h"
#include "dc_internal.h"

static dc_host g_host;
static int     g_installed;

int dc_lib_init(const dc_host *host)
{
	if (host == NULL || host->secure_alloc == NULL ||
	    host->secure_free == NULL || host->rand_bytes == NULL) {
		return ST_INVALID_PARAM;
	}

	if (g_installed) {
		/*
		 * Idempotent for the same host, an error for a different one. Two
		 * components in one process sharing this library must agree on the
		 * allocator: the second one silently winning would mean memory
		 * allocated by one being released by the other.
		 */
		return (memcmp(&g_host, host, sizeof(dc_host)) == 0) ? ST_OK : ST_ERROR;
	}

	memcpy(&g_host, host, sizeof(dc_host));
	g_installed = 1;
	return ST_OK;
}

void *dc_alloc(size_t size)
{
	return g_host.secure_alloc(size);
}

void dc_free(void *p)
{
	/* the moved code frees unconditionally in its cleanup paths */
	if (p != NULL) g_host.secure_free(p);
}

void dc_rand(void *buff, size_t size)
{
	g_host.rand_bytes(buff, size);
}

void dc_rand_seed(const void *buff, size_t size)
{
	if (g_host.rand_add_seed != NULL) g_host.rand_add_seed(buff, size);
}

void dc_wipe(void *wipe_ctx, u64 offset, int size)
{
	if (g_host.wipe != NULL && wipe_ctx != NULL) g_host.wipe(wipe_ctx, offset, size);
}

/*
 * Forwarded as a va_list rather than as a formatted string: the kernel has no
 * usable vsnprintf at arbitrary IRQL, and vDbgPrintEx takes a va_list directly.
 * A host that wants the text can format it itself.
 */
void dc_log(const char *fmt, ...)
{
	va_list args;

	if (g_host.log == NULL) return;

	va_start(args, fmt);
	g_host.log(fmt, args);
	va_end(args);
}
