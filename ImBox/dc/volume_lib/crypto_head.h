/*
    * DCrypt volume_lib - key derivation, header decryption, key slots
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * See PROVENANCE.md: this file began as an extraction from DiskCryptor's
    * driver, and everything that still carried ntldr's expression was rewritten
    * before the licence changed.
*/
#ifndef _CRYPTO_HEAD_H_
#define _CRYPTO_HEAD_H_

#include "defines.h"
#include "volume_header.h"
#include "xts_fast.h"

int argon2_mk_params(int kdf, u32* memory_cost, u32* time_cost, u32* parallelism);
int dc_derive_key(dc_pass* password, int kdf, u8* salt, u8* dk, ULONG *interrupt_cmd);
int cp_try_decrypt_header(u8* dk, int alg, xts_key* hdr_key, dc_header* header);
int cp_get_key_slot_size(int type);
int cp_swap_slot_key(u8* slot, u8* old_dk, u8* new_dk, int type);
int cp_wrap_header_key(u8* slot, u8* sk, u8* dk, int type);
int cp_get_min_header_len(dc_pass *password);
/*
 * out_dk receives the PKCS_DERIVE_MAX bytes that actually opened the header -
 * the password-derived key, or the key recovered from a slot. NULL if not
 * wanted; the driver passes NULL. dcapi uses it so the GUI can cache the key
 * and not pay for the KDF twice on the same volume.
 */
int cp_decrypt_header(xts_key *hdr_key, dc_header *header, int hdr_len, dc_pass *password, int* out_kdf, u8 *out_dk, int *out_slot, ULONG *interrupt_cmd);
int cp_set_header_key(xts_key *hdr_key, u8 salt[HEADER_SALT_SIZE], int cipher, dc_pass *password, ULONG *interrupt_cmd);
int cp_copy_keylots(dc_header *header, u8 *in_buff, u8 *out_buff);

/*
 * The KDF search orders, defined in crypto_head.c and terminated by -1.
 * A caller walks one of these when dc_pass.kdf is KDF_ALL or KDF_DEFAULT
 * rather than naming a single KDF.
 *
 * head_util.h wraps them as cp_kdf_list(); they are declared here as well
 * because both dcapi and DCS reach for the arrays directly, and a local
 * extern in each consumer is how they drift.
 */
extern const int dc_default_kdfs[];
extern const int dc_all_kdfs[];
//int cp_calculate_header_mac(dc_header* header);
//int cp_validate_header_mac(dc_header* header);

#endif