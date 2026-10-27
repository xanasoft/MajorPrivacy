/*
    * DCrypt volume_lib - header helpers kernel mode does not need
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * Step 4 of the volume_lib extraction: the things dcapi, DCS and ImBox do
    * that the driver has no equivalent for. Two shapes of gap:
    *
    *   - the driver only ever seals and unseals a header against a device, so
    *     there was no buffer-only form. dcapi needs one for a backup blob and
    *     ImBox for a container it has already read.
    *   - the driver never edits key slots or the extended header. dcapi does,
    *     from the GUI, so those helpers lived there.
    *
    * Ported from dcapi/dc_header.c, renamed into the cp_ family so that the
    * library has one naming convention rather than dcapi's _um suffix.
*/
#ifndef _HEAD_UTIL_H_
#define _HEAD_UTIL_H_

#include "defines.h"
#include "volume_header.h"
#include "xts_fast.h"

/* ---------------------------------------------------------------------------
 * Sealing and unsealing a buffer
 * ------------------------------------------------------------------------ */

/*
 * Seal a plaintext header into out, which must hold len bytes.
 *
 * Three things are deliberately left in the clear, and all three have to be,
 * not merely happen to be:
 *   - the salt, because it is what derives the key that decrypts the rest;
 *   - the key slots, because each is wrapped under its own slot key and
 *     re-encrypting them under the header key would make them unopenable;
 *   - nothing else.
 *
 * XTS runs over the whole length from offset 0, so the tweak for the part
 * beyond DC_AREA_SIZE continues naturally. io_write_header does the same, and
 * cp_decrypt_header + the tail decrypt in io_read_header_full undo exactly it.
 */
int cp_encrypt_header(dc_header *header, int len, xts_key *hdr_key, u8 *out);

/*
 * Unseal a header already in memory with a key that is already known - a
 * cached header key, or one recovered from a backup. No password, no KDF, so
 * no cancellation. out must hold len bytes; it may alias blob.
 *
 * Returns 1 if the result is a valid header, 0 otherwise.
 */
int cp_decrypt_header_with_key(const u8 *blob, int len, xts_key *hdr_key, dc_header *out);

/* ---------------------------------------------------------------------------
 * Key slots
 * ------------------------------------------------------------------------ */

/* Version, feature bit, slot count, area length and descriptor size all agree. */
int cp_has_key_slots(dc_header *header);

/*
 * Copy out a slot descriptor. If the slot is active but its CRC does not check
 * out, SF_CORRUPT is set in the returned copy - the header on disk is not
 * touched.
 *
 * ST_OK, ST_INCOMPATIBLE if the header has no slots, ST_BAD_INDEX.
 */
int cp_get_slot_info(dc_header *header, int slot_idx, dc_slot_info *info);

/* Copy out a slot's wrapped key material. ST_SMALL_BUFF if len exceeds the slot. */
int cp_get_slot_payload(dc_header *header, int slot_idx, u8 *payload, int len);

/*
 * Write a slot and refresh its CRC.
 *
 * info NULL leaves the descriptor alone; CP_SLOT_CLEAR zeroes it.
 * payload NULL leaves the key material alone; CP_SLOT_RANDOM fills the whole
 * slot with random bytes, which is how a slot is erased - an erased slot has
 * to be indistinguishable from one in use, so it is filled rather than zeroed.
 * Any space beyond len is filled with random bytes either way.
 */
#define CP_SLOT_CLEAR   ((dc_slot_info*)-1)
#define CP_SLOT_RANDOM  ((u8*)-1)
int cp_set_slot(dc_header *header, int slot_idx, dc_slot_info *info, u8 *payload, int len);

/* ---------------------------------------------------------------------------
 * Extended header
 * ------------------------------------------------------------------------ */

/*
 * CRC over the extended header's own data.
 *
 * Only the checksum is here. Reading and writing the fields inside it is a
 * serialisation problem, and dcapi solves it with misc/SVariant.c - 536 lines
 * that pull in <stdlib.h> and <string.h> and would follow the library into
 * kernel and EFI builds that cannot use them, for one consumer. Those two
 * functions stay in dcapi; get_ext_header, which locates and validates the
 * block, is already here in header_io.c.
 */
unsigned long cp_ext_header_crc(dc_ext_header *ext_hdr);

/* ---------------------------------------------------------------------------
 * KDF search order
 * ------------------------------------------------------------------------ */

/*
 * The list a caller walks when dc_pass.kdf is a selector rather than a single
 * KDF: KDF_DEFAULT or KDF_ALL. Terminated by -1. NULL for anything else.
 *
 * Replaces the two tables in dcapi/dc_header.c, which tpm_sup.c reaches today
 * with a bare `extern const int dc_all_kdfs[]`.
 */
const int *cp_kdf_list(int selector);

#endif /* _HEAD_UTIL_H_ */
