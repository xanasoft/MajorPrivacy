/*
    * DCrypt volume_lib - reading and writing a header on a device
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * See PROVENANCE.md: this file began as an extraction from DiskCryptor's
    * driver, and everything that still carried ntldr's expression was rewritten
    * before the licence changed.
*/
#include "defines.h"
#include "dc_internal.h"
#include "header_io.h"
#include "crypto_head.h"
#include "crc32.h"

int io_read_header_full(dc_dev *dev, dc_rw_fn rw, u64 pos, dc_header **header, xts_key *hdr_key, int hdr_len)
{
	int      buff_len = ROUND_TO_FULL_SECTORS(max(hdr_len, PAGE_SIZE), dev->bps);
	u8      *buff = NULL;
	u8      *buff2 = NULL;
	int      resl = ST_OK;

	/* v2 headers handling */
	if ((*header)->version < DC_HDR_VERSION_2) {
		return ST_OK; /* nothing to do for v1 headers */
	}

	do
	{
		/* allocate raw buffer and read raw header data from disk */
		if ( (buff = dc_alloc(buff_len)) == NULL ) { resl = ST_NOMEM; break; }
		if ( (resl = rw(dev->ctx, buff, hdr_len, pos, 1)) != ST_OK ) break;

		/* if header length is greater than read length, reallocate header buffer and read the rest of header */
		if (hdr_len < (int)(*header)->head_len)
		{
			/* reallocate raw buffer */
			buff2 = buff;
			buff_len = ROUND_TO_FULL_SECTORS(max((int)(*header)->head_len, PAGE_SIZE), dev->bps);
			if ( (buff = dc_alloc(buff_len)) == NULL ) { resl = ST_NOMEM; break; }
			memcpy(buff, buff2, hdr_len);

			/* reallocate (partially) decrypted header */
			memcpy(buff2, *header, hdr_len);
			dc_free(*header);
			buff_len = ROUND_TO_FULL_SECTORS(((dc_header*)buff2)->head_len, dev->bps);
			if ((*header = dc_alloc(buff_len)) == NULL) { resl = ST_NOMEM; break; }
			memcpy(*header, buff2, hdr_len);

			dc_free(buff2);
			buff2 = NULL;

			/* read the rest of header */
			if ( (resl = rw(dev->ctx, buff + hdr_len, (int)(*header)->head_len - hdr_len, pos + hdr_len, 1)) != ST_OK ) break;

			hdr_len = (int)(*header)->head_len;
		}

		/*
		 * Decrypt the rest; cp_decrypt_header only covered DC_AREA_SIZE.
		 *
		 * The source is buff + DC_AREA_SIZE, not buff. buff holds the raw
		 * header from offset 0, so the tail's ciphertext starts DC_AREA_SIZE
		 * into it - which is also the XTS offset it was encrypted at, hence the
		 * same constant appearing three times on this line.
		 *
		 * Reading from buff instead decrypted the base's ciphertext a second
		 * time under the tail's tweak and wrote the result over the tail, so
		 * every header longer than DC_AREA_SIZE came back with rubbish past the
		 * base. It went unnoticed because nothing that decides whether a volume
		 * mounts lives out there: the base carries the keys, and the key slots
		 * are copied across raw just below.
		 */
		if (hdr_len > DC_AREA_SIZE) {
			xts_decrypt(buff + DC_AREA_SIZE, ((u8*)*header) + DC_AREA_SIZE,
			            hdr_len - DC_AREA_SIZE, DC_AREA_SIZE, hdr_key);
		}

		/* load keyslots */
		if ((*header)->feature_flags & FF_KEY_SLOTS) {
			cp_copy_keylots(*header, buff, (u8*)*header);
		}
	} while (0);

	if (resl != ST_OK) {
		dc_free(*header); *header = NULL;
	}
	if (buff != NULL) dc_free(buff);
	if (buff2 != NULL) dc_free(buff2);
	return resl;
}

int io_read_header(dc_dev *dev, dc_rw_fn rw, u64 pos, dc_header **header, xts_key **out_key, dc_pass *password, int* out_kdf, ULONG *interrupt_cmd)
{
	xts_key *hdr_key = NULL;
	int      resl;

	/*
	 * How much to read before anything is known about the volume.
	 *
	 * Two lower bounds apply and the larger wins: the device's own idea of its
	 * header length, and whatever the password needs - a slot unlock has to
	 * reach the slot holding its key, which sits past the base. Rounded up to
	 * whole sectors, the unit both the device and XTS work in.
	 *
	 * This only opens the base. head_len lives inside the header, so it is not
	 * knowable until the base is decrypted; io_read_header_full comes back for
	 * the rest once it is.
	 */
	int hdr_len = ROUND_TO_FULL_SECTORS(
	                  max((int)dev->head_len, cp_get_min_header_len(password)),
	                  dev->bps);

	if ((*header = dc_alloc(hdr_len)) == NULL) {
		return ST_NOMEM;
	}

	resl = rw(dev->ctx, *header, hdr_len, pos, 1);

	if (resl == ST_OK && (hdr_key = dc_alloc(sizeof(xts_key))) == NULL) {
		resl = ST_NOMEM;
	}

	if (resl == ST_OK) {
		/* one call covers the whole search: every KDF the password admits,
		   every slot it names, every cipher */
		if (cp_decrypt_header(hdr_key, *header, hdr_len, password,
		                      out_kdf, NULL, NULL, interrupt_cmd) == 0) {
			resl = ST_PASS_ERR;
		}
	}

	if (resl == ST_OK) {
		resl = io_read_header_full(dev, rw, pos, header, hdr_key, hdr_len);
	}

	if (resl != ST_OK) {
		dc_free(*header);
		*header = NULL;
		dc_free(hdr_key);
		return resl;
	}

	/* the key belongs to the caller now, if it asked for one */
	if (out_key != NULL) {
		*out_key = hdr_key;
	} else {
		dc_free(hdr_key);
	}
	return ST_OK;
}

unsigned long calculate_header_crc(dc_header* header)
{
	if (header->version >= DC_HDR_VERSION_2) {
		// version 2 and later have the CRC calculated only over the header base
		return crc32((const unsigned char*)&header->version, DC_CRC_AREA_SIZE_2 - ((int)header->footer_cnt << 4));
	}
	return crc32((const unsigned char*)&header->version, DC_CRC_AREA_SIZE_1);
}

/*
 * Three things have to hold before a decrypted buffer may be treated as a
 * header, and they are checked cheapest-first.
 *
 * The salt is tested before the signature rather than after. A sector that was
 * never written, or was wiped, is all zeroes - and zero is a value the salt may
 * never take, because it is what derives the key that decrypted everything
 * else. Catching that first means a blank sector is rejected for the reason it
 * is actually wrong, instead of getting as far as a CRC over zeroes.
 */
BOOLEAN is_volume_header_correct(dc_header *header)
{
	u32 i, salt_bits;

	if (header == NULL) {
		return FALSE;
	}

	for (i = 0, salt_bits = 0; i < HEADER_SALT_SIZE; i++) {
		salt_bits |= header->salt[i];
	}
	if (salt_bits == 0) {
		return FALSE;
	}

	if (header->sign != DC_VOLUME_SIGN) {
		return FALSE;
	}

	return (header->hdr_crc == calculate_header_crc(header)) ? TRUE : FALSE;
}

int io_write_header(dc_dev *dev, dc_rw_fn rw, u64 pos, dc_header *header, xts_key *hdr_key, dc_pass *password, u32 flags, ULONG *interrupt_cmd)
{
	u8         salt[HEADER_SALT_SIZE];
	int        hdr_len = ROUND_TO_FULL_SECTORS(DC_AREA_SIZE, dev->bps);
	int        mem_len = DC_AREA_SIZE;
	dc_header *hcopy = NULL;
	xts_key   *h_key = hdr_key;
	u64        offset = 0;
	u32        length = hdr_len;
	int        resl;

	if (hdr_key != NULL && hdr_key->encrypt == NULL) return ST_ERROR;
	if (password != NULL && password->size == 0) return ST_ERROR;

	if (header->version >= DC_HDR_VERSION_2)
	{
		hdr_len = ROUND_TO_FULL_SECTORS(header->head_len, dev->bps);
		mem_len = header->head_len;

		/* when updating header calculate minumum required write length and offset and check header correctness */
		if ((flags & HF_UPDATE_ALL) != 0 && (flags & HF_UPDATE_ALL) != HF_UPDATE_ALL)
		{
			if (hdr_key == NULL) {
				DbgMsg("partial header update atempt without header key\n");
				return ST_ERROR;
			}

			length = DC_BASE_SIZE;
			if (header->feature_flags & FF_KEY_SLOTS) {
				length += header->slot_area_len + (header->key_slot_count * header->slot_info_size);
			}

			if (flags & HF_UPDATE_EXT) {
				if (header->ext_hdr_off == 0) {
					DbgMsg("extended header update atempt but ext_hdr_off is zero\n");
					flags &= ~HF_UPDATE_EXT;
				}
				else if ((u32)header->ext_hdr_off + MIN_EXT_HDR_SIZE >= header->head_len || header->ext_hdr_off < length) {
					DbgMsg("invalid extended header offset\n");
					return ST_INV_VOLUME;
				}
			}

			if (flags & HF_UPDATE_SLOTS) {
				if (!(header->feature_flags & FF_KEY_SLOTS)) {
					DbgMsg("slots area update atempt but slot not enabled\n");
					flags &= ~HF_UPDATE_SLOTS;
				}
				else if (header->head_len <= length) {
					DbgMsg("invalid keyslot area length\n");
					return ST_INV_VOLUME;
				}
			}

			if (flags & HF_UPDATE_BASE)
				offset = 0;
			else if (flags & HF_UPDATE_SLOTS)
				offset = DC_BASE_SIZE;
			else if (flags & HF_UPDATE_EXT)
				offset = header->ext_hdr_off;
			else
				return ST_OK; // nothing to do
			offset = (offset / (u64)dev->bps) * (u64)dev->bps; // align to sector, round down

			if (flags & HF_UPDATE_EXT)
				length = header->head_len;
			else if (flags & HF_UPDATE_SLOTS)
				length = DC_BASE_SIZE + header->slot_area_len + (header->key_slot_count * header->slot_info_size);
			else if (flags & HF_UPDATE_BASE)
				length = DC_BASE_SIZE;
			length = (u32)ROUND_TO_FULL_SECTORS((length - offset), dev->bps); // align to sector, round up

			if (offset + length > header->head_len) {
				DbgMsg("invalid header, update range %I64u, %d is invalid\n", offset, length);
				return ST_INV_VOLUME;
			}
		}
		else // full header write
		{
			length = hdr_len;
		}

		if ((flags & HF_CLEAR_SLOTS) && (header->feature_flags & FF_KEY_SLOTS)) {
			/* clear key slots and info - use random data for slots area, and zero info area */
			dc_rand(((u8*)header) + DC_BASE_SIZE, header->slot_area_len);
			memset(((u8*)header) + DC_BASE_SIZE + header->slot_area_len, 0, (header->key_slot_count * header->slot_info_size));
		}
	}

	//DbgMsg("io_write_header: offset %I64u, length %u, flags 0x%X\n", offset, length, flags);

	/*
	 * XTS works a sector at a time and counts the length down by one sector per
	 * turn, so a length that is not a whole number of them wraps and runs past
	 * the buffer. Refuse instead: a header that long cannot have come from this
	 * function, and writing one half-sealed would leave key material in the
	 * clear on disk - much worse than not writing it at all.
	 */
	if ((mem_len % XTS_SECTOR_SIZE) != 0) {
		DbgMsg("header length %d is not a whole number of sectors\n", mem_len);
		return ST_INV_DATA_SIZE;
	}

	/*
	 * The sealing itself. Everything above decided what to write and how much;
	 * from here it is: take a copy, check it, get a key, encrypt, put back the
	 * parts that must not be encrypted, write.
	 *
	 * PAGE_SIZE is a floor on the allocation, not on the header: a partial
	 * update writes from an offset, and the gap-filling below can reach past
	 * mem_len.
	 */
	hcopy = dc_alloc(max(hdr_len, PAGE_SIZE));
	if (hcopy == NULL) {
		resl = ST_NOMEM;
		goto done;
	}
	memcpy(hcopy, header, mem_len);

	if (is_volume_header_correct(hcopy) == FALSE) {
		resl = ST_INV_VOLUME;
		goto done;
	}

	if (h_key != NULL) {
		/*
		 * Re-sealing under a key the caller already has. The salt that derived
		 * it has to stay exactly as it is, so keep a copy to put back after the
		 * encryption runs over it.
		 */
		memcpy(salt, header->salt, HEADER_SALT_SIZE);
	} else {
		/*
		 * Sealing under a password instead, which means a fresh salt and a key
		 * derived from it.
		 *
		 * The header goes into the entropy pool first. At boot the pool has
		 * had little chance to collect anything, and the header - full of
		 * ciphertext and a previous salt - is the best material to hand.
		 */
		if ((h_key = dc_alloc(sizeof(xts_key))) == NULL) {
			resl = ST_NOMEM;
			goto done;
		}

		dc_rand_seed(header, DC_AREA_SIZE);
		dc_rand(salt, HEADER_SALT_SIZE);
		memcpy(hcopy->salt, salt, HEADER_SALT_SIZE);

		if (!cp_set_header_key(h_key, salt, header->alg_1, password, interrupt_cmd)) {
			resl = ST_INVALID_PARAM;
			goto done;
		}
	}

	xts_encrypt(pv(hcopy), pv(hcopy), mem_len, 0, h_key);

	/*
	 * Two things must not stay encrypted, for the same underlying reason - they
	 * are what a reader needs before it holds any key at all.
	 *
	 * The salt derives the header key, so it goes back in the clear. The key
	 * slots are each wrapped under their own slot key rather than the header
	 * key, so they are copied across verbatim - but only when the write will
	 * actually cover them, and not when the caller asked for them to be wiped.
	 */
	memcpy(hcopy->salt, salt, HEADER_SALT_SIZE);

	if (header->version >= DC_HDR_VERSION_2 && !(flags & HF_CLEAR_SLOTS) &&
	    (header->feature_flags & FF_KEY_SLOTS) &&
	    offset + length > DC_BASE_SIZE &&
	    offset < DC_BASE_SIZE + header->slot_area_len)
	{
		cp_copy_keylots(header, (u8*)header, (u8*)hcopy);
	}

	/*
	 * A sector-aligned write can reach past the header proper. Whatever it
	 * covers beyond that is random rather than zeroes, so the end of the header
	 * is not visible to someone looking at the disk.
	 */
	if ((offset + length) > mem_len) {
		dc_rand(((u8*)hcopy) + mem_len, (int)(offset + length) - mem_len);
	}

	resl = rw(dev->ctx, ((u8*)hcopy) + offset, length, offset + pos, 0);

done:
	burn(salt, sizeof(salt));
	if (h_key != NULL && h_key != hdr_key) dc_free(h_key);
	dc_free(hcopy);
	return resl;
}

int get_ext_header(dc_header *header, dc_ext_header **out_ext_hdr)
{
	dc_ext_header* ext_hdr;

	*out_ext_hdr = NULL;

	if (header->version < DC_HDR_VERSION_2) 
		return ST_INCOMPATIBLE;

	if (header->ext_hdr_off == 0) 
		return ST_INCOMPATIBLE;

	if (header->ext_hdr_off < DC_BASE_SIZE + ((header->feature_flags & FF_KEY_SLOTS) ? (header->slot_area_len + (header->key_slot_count * header->slot_info_size)) : 0)) {
		DbgMsg("invalid extended header offset, overlaps with key slots area\n");
		return ST_INV_VOLUME;
	}

	if ((u32)header->ext_hdr_off + MIN_EXT_HDR_SIZE > header->head_len) {
		DbgMsg("invalid extended header offset %d\n", header->ext_hdr_off);
		return ST_INV_VOLUME;
	}
	
	ext_hdr = (dc_ext_header*)(((u8*)header) + header->ext_hdr_off);
	if (header->ext_hdr_off + ext_hdr->size > header->head_len || ext_hdr->size < MIN_EXT_HDR_SIZE) {
		DbgMsg("invalid extended header size %d\n", ext_hdr->size);
		return ST_INV_VOLUME;
	}

	if (ext_hdr->crc != crc32((const unsigned char*)&ext_hdr->size, ext_hdr->size - 4)) {
		DbgMsg("invalid extended header CRC\n");
		return ST_INV_VOLUME;
	}

	*out_ext_hdr = ext_hdr;
	return ST_OK;
}
