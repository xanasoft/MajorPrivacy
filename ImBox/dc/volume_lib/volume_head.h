/*
    * DCrypt volume_lib - header level operations on a mounted volume
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * See PROVENANCE.md: this file began as an extraction from DiskCryptor's
    * driver, and everything that still carried ntldr's expression was rewritten
    * before the licence changed.
*/
#ifndef _VOLUME_HEAD_H_
#define _VOLUME_HEAD_H_

#include "defines.h"
#include "volume_header.h"
#include "xts_fast.h"
#include "dc_dev.h"

/*
 * Header level helpers split out of misc_volume.c. Step 1 of the volume_lib
 * extraction - see volume_lib/DESIGN.md.
 */

int dc_update_key_slots(dc_header *header, xts_key **hdr_key, dc_pass *old_pass, dc_pass *new_pass, ULONG *interrupt_cmd);
int dc_change_slot_pass(dc_header *header, dc_pass *old_pass, dc_pass *new_pass, ULONG *interrupt_cmd);
int dc_change_pass_bak(dc_dev *dev, dc_rw_fn rw, dc_header *header, dc_pass *old_pass, dc_pass *new_pass, u32 flags, void *wipe, ULONG *interrupt_cmd);
int dc_update_backup(dc_dev *dev, dc_rw_fn rw, dc_header *header, u8 *bak_salt, xts_key *bak_key, u8 *key_slots, u32 flags);

#endif
