/*
    * DCrypt volume_lib - header level operations on a mounted volume
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * See PROVENANCE.md: this file began as an extraction from DiskCryptor's
    * driver, and everything that still carried ntldr's expression was rewritten
    * before the licence changed. Nothing here was among it - all four of these
    * are new code with no counterpart in 1.1.846.118.
    *
    * Split out of misc_volume.c as step 1 of the volume_lib extraction. These
    * four operate on a header and a device, not on a device name: they do no
    * dc_find_hook, take no busy_lock and test no mount state, which is what
    * makes them movable. The entry points that do those things stayed behind
    * in misc_volume.c.
    *
    * Moved verbatim - no behaviour change and no signature change. The one
    * edit is that dc_update_key_slots lost its `static`, because
    * dc_change_pass still calls it from misc_volume.c.
    *
    * See volume_lib/DESIGN.md.
*/

#include "defines.h"
#include "dc_internal.h"
#include "volume_head.h"
#include "crypto_head.h"
#include "header_io.h"
#include "crc32.h"


int dc_update_key_slots(dc_header *header, xts_key **hdr_key, dc_pass *old_pass, dc_pass *new_pass, ULONG *interrupt_cmd)
{
	int        slot_size;
	int        used_slots = 0;
	int        i;
	dc_slot_info* slot_info = NULL;
	u8*        key_slot = NULL;
	u32        info_crc = 0;
	u8         old_dk[PKCS_DERIVE_MAX];
	u8         new_dk[PKCS_DERIVE_MAX];
	int        resl = ST_OK;

	dc_derive_key(old_pass, old_pass->kdf, header->salt, old_dk, interrupt_cmd);
	dc_derive_key(new_pass, new_pass->kdf, header->salt, new_dk, interrupt_cmd);

	/* update key slots */
	slot_size = header->slot_area_len / header->key_slot_count;
	//slot_size = cp_get_key_slot_size(slot_type);
	for (i = 0; i < header->key_slot_count; i++) {
		key_slot = ((u8*)header) + DC_BASE_SIZE + (slot_size * i);
		slot_info = (dc_slot_info*)(((u8*)header) + DC_BASE_SIZE + header->slot_area_len + (header->slot_info_size * i));
		if ((slot_info->flags & SF_ACTIVE) && !(slot_info->flags & SF_CORRUPT)) {
			info_crc = crc32(pv(&slot_info->flags), header->slot_info_size - 4);
			if (slot_info->crc == crc32_combine(info_crc, crc32(key_slot, slot_size), slot_size)) {
				if (!cp_swap_slot_key(key_slot, old_dk, new_dk, slot_info->type)) {
					resl = ST_SLOT_NOT_OK;
					break;
				}
				slot_info->crc = crc32_combine(info_crc, crc32(key_slot, slot_size), slot_size);
				used_slots++;
				continue;
			}
			slot_info->flags |= SF_CORRUPT;
			DbgMsg("slot %d is corrupted, clearing\n", i);
		}
		// if slot is not active or corrupted we just fill it with random data
		dc_rand(key_slot, slot_size);
		slot_info->crc = 0;
	}

	/* both passwords are expanded here; neither expansion may survive the
	   call that made it */
	burn(old_dk, sizeof(old_dk));
	burn(new_dk, sizeof(new_dk));

	if ( used_slots > 0 )
	{
		/* prepare header key - preserve salt */
		if ((*hdr_key = dc_alloc(sizeof(xts_key))) == NULL) {
			resl = ST_NOMEM;
		}
		else if (!cp_set_header_key(*hdr_key, header->salt, header->alg_1, new_pass, interrupt_cmd)) {
			resl = ST_INVALID_PARAM;
		}
	}

	return resl;
}

int dc_change_slot_pass(dc_header *header, dc_pass *old_pass, dc_pass *new_pass, ULONG *interrupt_cmd)
{
	u8         dk[PKCS_DERIVE_MAX];
	u8         sk[PKCS_DERIVE_MAX];
	u8        *slot;
	dc_slot_info *info;
	int        slot_index;
	int        slot_size;
	u32        slot_off;
	u32        info_off;
	u32        info_crc;
	int        resl = ST_OK;

	do
	{
		if (header->version < DC_HDR_VERSION_2 || !(header->feature_flags & FF_KEY_SLOTS)) {
			DbgMsg("dc_change_slot_pass: key slots not supported by header\n");
			resl = ST_INVALID_PARAM;
			break;
		}

		if (new_pass->slot < 1 || new_pass->slot > header->key_slot_count) {
			DbgMsg("dc_change_slot_pass: invalid slot number %d\n", new_pass->slot);
			resl = ST_INVALID_PARAM;
			break;
		}

		/* validate slot layout fits within header */
		if (header->key_slot_count == 0 || header->slot_area_len == 0 || header->slot_info_size < sizeof(dc_slot_info)) {
			DbgMsg("dc_change_slot_pass: invalid slot layout\n");
			resl = ST_INV_VOLUME;
			break;
		}

		slot_index = new_pass->slot - 1;
		slot_size = header->slot_area_len / header->key_slot_count;
		slot_off = DC_BASE_SIZE + (slot_size * slot_index);
		info_off = DC_BASE_SIZE + header->slot_area_len + (header->slot_info_size * slot_index);

		/* validate offsets are within header bounds */
		if (slot_off + slot_size > header->head_len) {
			DbgMsg("dc_change_slot_pass: slot offset %u + size %d exceeds header length %u\n", slot_off, slot_size, header->head_len);
			resl = ST_INV_VOLUME;
			break;
		}
		if (info_off + header->slot_info_size > header->head_len) {
			DbgMsg("dc_change_slot_pass: info offset %u + size %u exceeds header length %u\n", info_off, header->slot_info_size, header->head_len);
			resl = ST_INV_VOLUME;
			break;
		}

		slot = ((u8*)header) + slot_off;
		info = (dc_slot_info*)(((u8*)header) + info_off);

		if (!dc_derive_key(old_pass, old_pass->kdf, header->salt, dk, interrupt_cmd)) {
			DbgMsg("dc_change_slot_pass: failed to derive old password key\n");
			resl = ST_INVALID_PARAM;
			break;
		}

		if (!dc_derive_key(new_pass, new_pass->kdf, header->salt, sk, interrupt_cmd)) {
			DbgMsg("dc_change_slot_pass: failed to derive new password key\n");
			resl = ST_INVALID_PARAM;
			break;
		}

		/* update slot info */
		info->flags |= SF_ACTIVE;
		info->flags &= ~SF_CORRUPT;
		info->type = DC_SLOT_TYPE_0;
		info->data_0.slot_kdf = (u8)new_pass->kdf;

		/* wrap the header key into the slot */
		if (!cp_wrap_header_key(slot, sk, dk, info->type)) {
			resl = ST_SLOT_NOT_OK;
			break;
		}

		/* compute and set CRC: crc32(info excluding crc field) combined with crc32(slot data) */
		info_crc = crc32(pv(&info->flags), header->slot_info_size - 4);
		info->crc = crc32_combine(info_crc, crc32(slot, slot_size), slot_size);

	} while (0);

	/* derived and slot keys, both dead from here on */
	burn(dk, sizeof(dk));
	burn(sk, sizeof(sk));

	return resl;
}

int dc_change_pass_bak(dc_dev *dev, dc_rw_fn rw, dc_header *header, dc_pass *old_pass, dc_pass *new_pass, u32 flags, void *wipe, ULONG *interrupt_cmd)
{
	dc_header *hback = NULL;
	xts_key   *bak_key = NULL;
	u64        backup_pos = dev->dsk_size - dev->head_len;
	int        slots_valid = 0;
	int        slots_len = 0;
	int        resl;

	do
	{
		/* read and validate backup header */
		if ((resl = io_read_header(dev, rw, backup_pos, &hback, &bak_key, old_pass, NULL, interrupt_cmd)) != ST_OK) {
			DbgMsg("failed to read backup header, error code: %d\n", resl);
			resl = ST_INV_VOLUME;
			break;
		}

		/* check if slot layout matches between primary and backup */
		if (header->version >= DC_HDR_VERSION_2 && (header->feature_flags & FF_KEY_SLOTS))
		{
			slots_len = header->slot_area_len + (header->key_slot_count * header->slot_info_size);

			if (hback->slot_area_len == header->slot_area_len &&
				hback->key_slot_count == header->key_slot_count &&
				hback->slot_info_size == header->slot_info_size) {
				slots_valid = 1;
			} else {
				DbgMsg("backup header slot layout mismatch, clearing slots\n");
			}
		}

		/* copy primary header data to backup, skipping salt and key slots+info */
		memcpy(((u8*)hback) + HEADER_SALT_SIZE, ((u8*)header) + HEADER_SALT_SIZE, DC_BASE_SIZE - HEADER_SALT_SIZE);

		/* copy data after slot area (ext header, etc.) */
		if ((u32)DC_BASE_SIZE + slots_len < dev->head_len) {
			memcpy(((u8*)hback) + DC_BASE_SIZE + slots_len, ((u8*)header) + DC_BASE_SIZE + slots_len, dev->head_len - DC_BASE_SIZE - slots_len);
		}

		/* change slot password on backup header */
		if (new_pass->slot != 0)
		{
			if ( (resl = dc_change_slot_pass(hback, old_pass, new_pass, interrupt_cmd)) != ST_OK) {
				break;
			}

			flags |= HF_UPDATE_SLOTS;
		}

		/* change primary password  */
		else
		{
			/* io_write_header changes salt and creates new key from password when hdr_key is not NULL */
			dc_free(bak_key);
			bak_key = NULL;

			if (slots_valid && !(flags & HF_CLEAR_SLOTS)) {
				/* update all key slots with new primary password (uses backup's salt) */
				if ((resl = dc_update_key_slots(hback, &bak_key, old_pass, new_pass, interrupt_cmd)) != ST_OK) {
					break;
				}
			}
			else if (slots_len > 0) {
				/* slot layout mismatch, or clearing - fill slots with random data and clear slot info */
				dc_rand(((u8*)hback) + DC_BASE_SIZE, header->slot_area_len);
				memset(((u8*)hback) + DC_BASE_SIZE + header->slot_area_len, 0, header->key_slot_count * header->slot_info_size);
			}
		}

		if (flags & HF_HEADER_FILL) {
			/* wipe backup header area */
			dc_wipe(wipe, backup_pos, dev->head_len);
		}

		/* write new backup header */
		if ((resl = io_write_header(dev, rw, backup_pos, hback, bak_key, new_pass, flags, interrupt_cmd)) != ST_OK) {
			DbgMsg("failed to write new backup header, error code: %d\n", resl);
			break;
		}

	} while (0);

	if (hback != NULL)   dc_free(hback);
	if (bak_key != NULL) dc_free(bak_key);

	return resl;
}

int dc_update_backup(dc_dev *dev, dc_rw_fn rw, dc_header *header, u8 *bak_salt, xts_key *bak_key, u8 *key_slots, u32 flags)
{
	dc_header *hcopy = NULL;
	u8        *old_backup = NULL;
	u64        backup_pos = dev->dsk_size - dev->head_len;
	int        resl;
	int        slot_size;
	int        i;
	dc_slot_info* slot_info = NULL;
	u8*        key_slot = NULL;
	u32        info_crc = 0;

	if (!(dev->opt & DC_DEV_BACKUP_HEADER)) {
		return ST_ERROR;
	}

	/* validate header */
	if (!is_volume_header_correct(header)) {
		resl = ST_INV_VOLUME;
		goto finish;
	}

	/* the caller owns the backup key; there is no fallback */
	if (bak_salt == NULL || bak_key == NULL || bak_key->encrypt == NULL) {
		resl = ST_ERROR;
		goto finish;
	}

	/* validate storage size for backup header */
	if (dev->opt & DC_DEV_STORAGE_ON_END) {
		if (dev->stor_len < dev->head_len * 2) {
			resl = ST_NF_SPACE;
			goto finish;
		}
	} else {
		if (dev->tail_len < dev->head_len) {
			resl = ST_NF_SPACE;
			goto finish;
		}
	}

	/* prepare backup header copy with its own salt */
	if ((hcopy = dc_alloc(dev->head_len)) == NULL) {
		return ST_NOMEM;
	}
	memcpy(hcopy, header, dev->head_len);
	memcpy(hcopy->salt, bak_salt, HEADER_SALT_SIZE);

	/* Key slots are encrypted with a key derived from the header salt.
	* The backup header has its own salt, so we must preserve the backup's
	* existing keyslot area which is encrypted with the backup salt.
	* The passed 'header' is the primary header whose key slots are bound
	* to the primary salt - we cannot use those for the backup. */
	if (header->version >= DC_HDR_VERSION_2 && (header->feature_flags & FF_KEY_SLOTS) && header->slot_area_len > 0)
	{
		if ((u32)DC_BASE_SIZE + header->slot_area_len > dev->head_len) {
			DbgMsg("dc_update_backup: invalid slot area length %u\n", header->slot_area_len);
			resl = ST_INV_VOLUME;
			goto finish;
		}

		if (flags & HF_CLEAR_SLOTS) {
			/* clear slots with random data and clear slot info */
			dc_rand(((u8*)hcopy) + DC_BASE_SIZE, header->slot_area_len);
			memset(((u8*)hcopy) + DC_BASE_SIZE + header->slot_area_len, 0, header->key_slot_count * header->slot_info_size);
		}
		else 
		{
			if (key_slots != NULL) {
				/* Use provided key slots directly */
				memcpy(((u8*)hcopy) + DC_BASE_SIZE, key_slots, header->slot_area_len);
			}
			else {
				/* read old backup header raw bytes from disk */
				if ((old_backup = dc_alloc(dev->head_len)) == NULL) {
					resl = ST_NOMEM;
					goto finish;
				}
				if ((resl = rw(dev->ctx, old_backup, dev->head_len, backup_pos, 1)) != ST_OK) {
					DbgMsg("dc_update_backup: failed to read old backup header\n");
					goto finish;
				}

				/* copy the raw keyslot area from old backup header to hcopy
				* (key slots start at DC_BASE_SIZE with length slot_area_len) */
				memcpy(((u8*)hcopy) + DC_BASE_SIZE, old_backup + DC_BASE_SIZE, header->slot_area_len);
			}

			/* update key slotinto crc */
			slot_size = hcopy->slot_area_len / hcopy->key_slot_count;
			//slot_size = cp_get_key_slot_size(slot_type);
			for (i = 0; i < hcopy->key_slot_count; i++) {
				key_slot = ((u8*)hcopy) + DC_BASE_SIZE + (slot_size * i);
				slot_info = (dc_slot_info*)(((u8*)hcopy) + DC_BASE_SIZE + hcopy->slot_area_len + (hcopy->slot_info_size * i));
				if ((slot_info->flags & SF_ACTIVE) && !(slot_info->flags & SF_CORRUPT)) {
					info_crc = crc32(pv(&slot_info->flags), hcopy->slot_info_size - 4);
					slot_info->crc = crc32_combine(info_crc, crc32(key_slot, slot_size), slot_size);
				}
			}
		}
	}

	resl = io_write_header(dev, rw, backup_pos, hcopy, bak_key, NULL, flags, NULL);

finish:
	if (old_backup != NULL) dc_free(old_backup);
	if (hcopy != NULL) dc_free(hcopy);
	DbgMsg("dc_update_backupfinished: pos=%I64u; resl=%d\n", backup_pos, resl);
	return resl;
}
