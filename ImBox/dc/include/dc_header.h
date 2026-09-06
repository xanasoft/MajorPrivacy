#ifndef _DC_HEADER_H_
#define _DC_HEADER_H_

#include "defines.h"
#include "dcconst.h"
#define dc_api
#include "volume_header.h"

#if defined(_M_IX86) || defined(_M_ARM64)
#define SMALL
#endif

#ifdef SMALL
#include "..\crypto_small\xts_small.h"
#else
#include "..\crypto_fast\xts_fast.h"
#endif

//#define EXT_HDR_GUID  'DIUG' // GUID
//#define EXT_HDR_LABEL  'LBL'
//#define EXT_HDR_COMMENT  'CMNT'

//typedef struct _dc_ext_data {
//	//unsigned char  volume_guid[16];
//	char           volume_comment[57]; // 41
//} dc_ext_data;

/*
* Initialize crypto subsystem (called before starting crypto operations)
*/
int dc_init_crypto();

/*
 * Calculate CRC for a header structure
 * For version 2 and later, CRC is calculated only over the header base (fixed-size part)
 * For version 1, CRC is calculated over the entire 2KiB header
 */
unsigned long dc_api calculate_header_crc_um(struct _dc_header* header);

/*
 * Check if a header structure has correct signature and CRC
 * Returns TRUE if header is valid, FALSE otherwise
 */
BOOLEAN dc_api is_volume_header_correct_um(dc_header *header);

/*
 * Helper function to expand a single cost parameter into memory_cost, time_cost and parallelism.
 */
int dc_api argon2_mk_params_um(int kdf, u32* memory_cost, u32* time_cost, u32* parallelism);

/*
 * Derive encryption key from password and salt (user-mode)
 * Supports both SHA512-PBKDF2 (cost=0) and Argon2id (cost>0)
 * Returns 1 on success, 0 on failure
 */
int dc_api dc_derive_key_um(dc_pass *password, int kdf, u8 *salt, u8 *dk);

/*
 * Try to decrypt header with derived key, testing all cipher types
 * Returns 1 if decryption succeeded (valid header found), 0 otherwise
 * On success, hdr_key contains the working key and hcopy contains decrypted header
 */
int dc_api dc_try_decrypt_header_um(u8 *dk, xts_key *hdr_key, u8 *enc_header, dc_header *hcopy, int header_len);

/*
* Get the minimum header length required to support the features used in the header
* This is used to determine the expected header size when decrypting, based on the version and feature flags.
* For example, a header with key slots requires at least the base size plus the slot area and info size, while a header without key slots may only require the base size.
*/
int dc_api dc_get_min_header_len(dc_pass *password);

/*
 * Decrypt an encrypted header backup in user mode
 * enc_header: encrypted header data
 * enc_len: length of encrypted data
 * password: password to decrypt with
 * out_header: receives allocated decrypted header (caller must secure_free)
 * out_key: receives allocated header key (caller must secure_free)
 * out_len: receives actual header length
 * Returns ST_OK on success, error code otherwise
 */
int dc_api dc_decrypt_header(u8 *enc_header, int enc_len, dc_pass *password,
    dc_header **out_header, xts_key **out_key, int *out_len, int* out_kdf);

int dc_decrypt_header_with_key(u8 *enc_header, int header_len, dc_header *header, xts_key *hdr_key);

/*
* Set up an XTS key structure for header encryption/decryption based on the provided password and salt
* hdr_key: receives allocated and initialized XTS key (caller must secure_free)
* salt: salt to use for key derivation (must be HEADER_SALT_SIZE bytes)
* cipher: cipher index to use for the header (0-2, corresponding to AES, Twofish, Serpent)
* password: password to derive the key from
* Returns 1 on success, 0 otherwise
*/
int dc_api dc_set_header_key(xts_key *hdr_key, u8 salt[HEADER_SALT_SIZE], int cipher, dc_pass *password);

/*
 * Encrypt a header for storage
 * header: decrypted header to encrypt
 * header_len: length of header
 * hdr_key: encryption key
 * out_enc: receives allocated encrypted header (caller must secure_free)
 * Returns ST_OK on success, error code otherwise
 */
int dc_api dc_encrypt_header(dc_header *header, int header_len, xts_key *hdr_key, u8 **out_enc);

///*
// * Load and decrypt header from a backup file
// * file_path: path to header backup file
// * password: password to decrypt with
// * out_header: receives allocated decrypted header
// * out_key: receives allocated header key
// * out_len: receives header length
// * Returns ST_OK on success, error code otherwise
// */
//int dc_api dc_load_header_file(wchar_t *file_path, dc_pass *password,
//    dc_header **out_header, xts_key **out_key, int *out_len);
//
///*
// * Save encrypted header to a file
// * file_path: path to save to
// * header: decrypted header
// * header_len: header length
// * hdr_key: encryption key
// * Returns ST_OK on success, error code otherwise
// */
//int dc_api dc_save_header_file(wchar_t *file_path, dc_header *header, int header_len, xts_key *hdr_key);

/*
* Get the size of the keyslot payload for a given slot type
* type: slot type identifier (currently only type 0 is defined)
* Returns the size of the keyslot payload in bytes, or 0 for unknown types
*/
int dc_api dc_get_key_slot_size(int type);

/*
* Check if a v2 header has valid keyslot configuration
* Validates version, slot area length, slot count, and slot info size
* Returns TRUE if key slots are available and properly configured, FALSE otherwise
*/
BOOL dc_api dc_has_key_slots(dc_header* header);

/*
* Retrieve keyslot descriptor information for a given slot index
* header: decrypted header with key slots
* slot_idx: zero-based slot index (must be < header->key_slot_count)
* info: receives a copy of the slot descriptor (dc_slot_info)
* Returns ST_OK on success, ST_INCOMPATIBLE if no key slots, ST_BAD_INDEX if out of range
*/
int dc_api dc_get_slot_info(dc_header* header, int slot_idx, dc_slot_info* info);

/*
* Retrieve the encrypted payload (ciphertext) for a given keyslot
* header: decrypted header with key slots
* slot_idx: zero-based slot index (must be < header->key_slot_count)
* payload: buffer to receive the slot ciphertext
* len: size of the payload buffer
* Returns pointer to slot payload on success, NULL if no key slots or invalid index
*/
int dc_api dc_get_slot_payload(dc_header* header, int slot_idx, u8* payload, int len);

/*
* Set keyslot data (descriptor and/or encrypted payload) for a given slot index
* header: decrypted header with key slots
* slot_idx: zero-based slot index (must be < header->key_slot_count)
* info: slot descriptor to write (NULL to skip), copied up to slot_info_size bytes
* payload: encrypted key data to write (NULL to skip, (u8*)-1 to pad with random)
* len: size of payload data (remaining slot space is filled with random bytes)
* Updates the slot CRC (combined CRC of descriptor and ciphertext)
* Returns ST_OK on success, ST_INCOMPATIBLE / ST_BAD_INDEX / ST_INVALID_PARAM on error
*/
int dc_api dc_set_slot(dc_header* header, int slot_idx, dc_slot_info* info, u8* payload, int len);

/*
* Helper to wrap the header key with slot key
* slot: output buffer for wrapped key (must be at least the size of the header key)
* sk: derived key for the slot (from password and salt)
* dk: derived key for the header (from password and salt)
* type: 0 for wrapping (encrypting header key), 1 for unwrapping (decrypting header key)
* Returns 1 on success, 0 on failure (e.g. invalid parameters)
*/
int dc_api dc_wrap_header_key(u8* slot, u8* sk, u8* hk, int type);

/* 
* Calculate CRC for the extended header
*/
unsigned long dc_api calculate_ext_header_crc_um(struct _dc_ext_header* ext_hdr);

/*
* Reads extended data from the extended header.
*/
//int dc_api read_ext_header(u8* data, int size, dc_ext_data* ext_data);

/*
* Writes extended data to the extended header and updates the CRC.
* Returns new length, or 0 on error (e.g. data too large to fit in header).
*/
//int dc_api write_ext_header(dc_ext_data* ext_data, u8* data, int size);
#endif