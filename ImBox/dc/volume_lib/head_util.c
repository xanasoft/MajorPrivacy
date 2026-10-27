/*
    * DCrypt volume_lib - header helpers kernel mode does not need
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * See head_util.h. Ported from dcapi/dc_header.c; the only changes are the
    * rename into the cp_ family and dcapi's DC_CTL_GET_RAND round trip to the
    * driver becoming dc_rand().
*/
#include "defines.h"
#include "dc_internal.h"
#include "head_util.h"
#include "crypto_head.h"
#include "header_io.h"
#include "crc32.h"

/* ---------------------------------------------------------------------------
 * Sealing and unsealing a buffer
 * ------------------------------------------------------------------------ */

int cp_encrypt_header(dc_header *header, int len, xts_key *hdr_key, u8 *out)
{
	u8 salt[HEADER_SALT_SIZE];

	if (header == NULL || out == NULL || len < DC_AREA_SIZE) return ST_INVALID_PARAM;
	if (hdr_key == NULL || hdr_key->encrypt == NULL) return ST_ERROR;

	/*
	 * XTS counts down whole sectors - `while (len -= XTS_SECTOR_SIZE)` - so a
	 * length that is not a multiple of one underflows to SIZE_MAX and runs off
	 * the end of the buffer. Every legitimate header length is a sector
	 * multiple; the driver reaches that with ROUND_TO_FULL_SECTORS before it
	 * ever gets here. Say so rather than trusting the caller to have done it.
	 */
	if ((len % XTS_SECTOR_SIZE) != 0) return ST_INV_DATA_SIZE;

	/* the salt must survive the encryption: it derives the key */
	memcpy(salt, header->salt, HEADER_SALT_SIZE);

	memcpy(out, header, len);
	xts_encrypt(out, out, len, 0, hdr_key);
	memcpy(out, salt, HEADER_SALT_SIZE);

	/*
	 * Key slots are wrapped under their own slot keys, never under the header
	 * key, so they are put back as they were rather than left encrypted.
	 */
	if (header->version >= DC_HDR_VERSION_2 &&
	    (header->feature_flags & FF_KEY_SLOTS) != 0)
	{
		cp_copy_keylots(header, (u8*)header, out);
	}

	burn(salt, sizeof(salt));
	return ST_OK;
}

int cp_decrypt_header_with_key(const u8 *blob, int len, xts_key *hdr_key, dc_header *out)
{
	if (blob == NULL || out == NULL || len < DC_AREA_SIZE) return 0;
	if (hdr_key == NULL || hdr_key->decrypt == NULL) return 0;

	/* whole sectors only - see cp_encrypt_header */
	if ((len % XTS_SECTOR_SIZE) != 0) return 0;

	/* DC_AREA_SIZE first, so that head_len can be trusted before it is used */
	xts_decrypt(blob, (u8*)out, DC_AREA_SIZE, 0, hdr_key);

	/*
	 * The salt was never encrypted, so decrypting over it produced rubbish.
	 * cp_try_decrypt_header avoids this by copying back only from ->sign
	 * onward; here the decrypt already landed in out, so put the salt back.
	 * It is outside the CRC range either way, so this is about the caller
	 * getting a usable header, not about validation.
	 */
	memcpy(out->salt, blob, HEADER_SALT_SIZE);

	if (is_volume_header_correct(out) == FALSE) return 0;

	if (len > DC_AREA_SIZE) {
		xts_decrypt(blob + DC_AREA_SIZE, ((u8*)out) + DC_AREA_SIZE,
		            len - DC_AREA_SIZE, DC_AREA_SIZE, hdr_key);
	}

	/* slots are not under the header key; carry them over verbatim */
	if ((out->feature_flags & FF_KEY_SLOTS) != 0) {
		cp_copy_keylots(out, (u8*)blob, (u8*)out);
	}
	return 1;
}

/* ---------------------------------------------------------------------------
 * Key slots
 * ------------------------------------------------------------------------ */

int cp_has_key_slots(dc_header *header)
{
	if (header == NULL) return 0;
	if (header->version < DC_HDR_VERSION_2) return 0;
	if (!(header->feature_flags & FF_KEY_SLOTS)) return 0;
	if (header->slot_area_len == 0) return 0;
	if (header->key_slot_count == 0) return 0;
	if (header->slot_info_size == 0) return 0;
	return 1;
}

int cp_get_slot_info(dc_header *header, int slot_idx, dc_slot_info *info)
{
	int           slot_size;
	u8           *slot_info_start;
	dc_slot_info *slot_info;
	u8           *key_slot;
	u32           info_crc, slot_crc;

	if (!cp_has_key_slots(header)) return ST_INCOMPATIBLE;
	if (slot_idx < 0 || slot_idx >= header->key_slot_count) return ST_BAD_INDEX;

	slot_size = header->slot_area_len / header->key_slot_count;

	slot_info_start = ((u8*)header) + DC_BASE_SIZE + header->slot_area_len;
	slot_info = (dc_slot_info*)(slot_info_start + slot_idx * header->slot_info_size);

	memcpy(info, slot_info, min(sizeof(dc_slot_info), header->slot_info_size));
	if (sizeof(dc_slot_info) > header->slot_info_size) {
		memset(((u8*)info) + header->slot_info_size, 0,
		       sizeof(dc_slot_info) - header->slot_info_size);
	}

	if (!(info->flags & SF_ACTIVE)) {
		return ST_OK;
	}

	key_slot = ((u8*)header) + DC_BASE_SIZE + slot_idx * slot_size;

	info_crc = crc32((const u8*)&slot_info->flags, header->slot_info_size - 4);
	slot_crc = crc32((const u8*)key_slot, slot_size);
	if (slot_info->crc != crc32_combine(info_crc, slot_crc, slot_size)) {
		/* reported to the caller, not written back to the header */
		info->flags |= SF_CORRUPT;
	}
	return ST_OK;
}

int cp_get_slot_payload(dc_header *header, int slot_idx, u8 *payload, int len)
{
	int  slot_size;
	u8  *key_slot;

	if (!cp_has_key_slots(header)) return ST_INCOMPATIBLE;
	if (slot_idx < 0 || slot_idx >= header->key_slot_count) return ST_BAD_INDEX;

	slot_size = header->slot_area_len / header->key_slot_count;
	if (payload != NULL && len > slot_size) return ST_SMALL_BUFF;

	key_slot = ((u8*)header) + DC_BASE_SIZE + slot_idx * slot_size;
	memcpy(payload, key_slot, min(len, slot_size));
	return ST_OK;
}

int cp_set_slot(dc_header *header, int slot_idx, dc_slot_info *info, u8 *payload, int len)
{
	int           slot_size;
	u8           *slot_info_start;
	dc_slot_info *slot_info;
	u8           *key_slot;
	u32           info_crc, slot_crc;

	if (!cp_has_key_slots(header)) return ST_INCOMPATIBLE;
	if (slot_idx < 0 || slot_idx >= header->key_slot_count) return ST_BAD_INDEX;

	slot_size = header->slot_area_len / header->key_slot_count;
	if (payload != NULL && len > slot_size) return ST_INVALID_PARAM;

	slot_info_start = ((u8*)header) + DC_BASE_SIZE + header->slot_area_len;
	slot_info = (dc_slot_info*)(slot_info_start + slot_idx * header->slot_info_size);

	if (info == CP_SLOT_CLEAR) {
		memset(slot_info, 0, header->slot_info_size);
	} else if (info != NULL) {
		memcpy(slot_info, info, min(sizeof(dc_slot_info), header->slot_info_size));
	}

	key_slot = ((u8*)header) + DC_BASE_SIZE + slot_idx * slot_size;
	if (payload != NULL) {
		if (payload != CP_SLOT_RANDOM) {
			memcpy(key_slot, payload, len);
		}
		if (len < slot_size) {
			/*
			 * dcapi asked the driver for these bytes over DC_CTL_GET_RAND; the
			 * library asks its host. An unused slot has to look exactly like a
			 * used one, so the remainder is random, never zero.
			 */
			dc_rand(key_slot + len, slot_size - len);
		}
	}

	info_crc = crc32((const u8*)&slot_info->flags, header->slot_info_size - 4);
	slot_crc = crc32((const u8*)key_slot, slot_size);
	slot_info->crc = crc32_combine(info_crc, slot_crc, slot_size);
	return ST_OK;
}

/* ---------------------------------------------------------------------------
 * Extended header
 * ------------------------------------------------------------------------ */

unsigned long cp_ext_header_crc(dc_ext_header *ext_hdr)
{
	return crc32((const u8*)&ext_hdr->size, ext_hdr->size - 4);
}

/* ---------------------------------------------------------------------------
 * KDF search order
 * ------------------------------------------------------------------------ */

extern const int dc_default_kdfs[];
extern const int dc_all_kdfs[];

const int *cp_kdf_list(int selector)
{
	if (selector == KDF_ALL)     return dc_all_kdfs;
	if (selector == KDF_DEFAULT) return dc_default_kdfs;
	return NULL;
}
