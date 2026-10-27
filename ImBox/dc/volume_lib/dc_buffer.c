/*
    * DCrypt volume_lib - a dc_dev over a plain memory buffer
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * io_read_header and io_write_header take a device. dcapi and ImBox
    * sometimes have the header already in memory instead - a backup blob, or a
    * container the caller has read for itself - and there is no point inventing
    * a second set of buffer-shaped entry points for that. A dc_dev whose rw
    * callback is a memcpy covers it.
*/
#include "defines.h"
#include "dc_internal.h"
#include "dc_dev.h"

static int dc_buffer_rw_impl(void *ctx, void *buff, u32 length, u64 offset, int is_read)
{
	dc_buffer *b = (dc_buffer*)ctx;

	if (b == NULL || b->data == NULL || buff == NULL) return ST_INVALID_PARAM;

	/*
	 * A short read at the end is an error, not a truncation: the caller asked
	 * for a header of a particular size and either has it or does not. Written
	 * as a subtraction on the right so that offset + length cannot wrap.
	 */
	if (offset > b->size || length > b->size - offset) return ST_RW_ERR;

	if (is_read) memcpy(buff, b->data + offset, length);
	else         memcpy(b->data + offset, buff, length);

	return ST_OK;
}

dc_rw_fn dc_buffer_rw(void)
{
	return dc_buffer_rw_impl;
}

int dc_dev_from_buffer(dc_dev *dev, dc_buffer *desc, void *buff, u32 size, u32 bps)
{
	if (dev == NULL || desc == NULL || buff == NULL || size == 0) return ST_INVALID_PARAM;
	if (bps == 0) bps = SECTOR_SIZE;

	desc->data = (u8*)buff;
	desc->size = size;

	memset(dev, 0, sizeof(dc_dev));
	dev->ctx      = desc;
	dev->bps      = bps;
	dev->dsk_size = size;
	dev->head_len = min(size, DC_AREA_SIZE);
	dev->tail_len = dev->head_len;

	return ST_OK;
}
