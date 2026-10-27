#ifndef _DC_HEADER_H_
#define _DC_HEADER_H_

/*
 * ImBox's view of DiskCryptor's volume header.
 *
 * The implementation behind these names is DiskCryptor's volume_lib, reached
 * through the junctions dc\setup_links.cmd creates - dc\crypto_lib,
 * dc\volume_lib and dc\_include_; dc_header.c is the adapter. The names and
 * signatures are ImBox's own and unchanged, so CryptoIO.cpp is unaffected.
 *
 * volume_header.h is DiskCryptor's now rather than a copy of it -
 * it describes the on-disk format, and there is no version of that which is
 * ImBox's. defines.h stays ImBox's: it is the host adaptation, and it declares
 * secure_alloc, secure_free, burn and make_rand, which CryptoIO.cpp defines.
 */

#include "defines.h"
/* ST_* - from the library rather than DCrypt's dcconst.h, which ImBox
   wanted nothing else out of */
#include "dc_status.h"
#define dc_api
#include "volume_header.h"

/* crypto_lib, for xts_key and the xts_encrypt/xts_decrypt macros. There is no
   SMALL build any more: crypto_lib covers x64 and ARM64, and on ARM64 that is
   the crypto-extension path rather than the portable C fallback ImBox used to
   compile there. */
#include "xts_fast.h"

/* volume_lib, for the cp_ functions CryptoIO.cpp calls directly */
#include "crypto_head.h"
#include "head_util.h"

/*
 * What ImBox still asks this layer for. Everything else it needs - the key
 * slots, the CRCs, the KDF, sealing a buffer - CryptoIO.cpp now calls on
 * volume_lib directly under its own cp_ names, so the forwarders that used to
 * sit here are gone rather than kept as an alias nobody was reading.
 */

/*
 * Installs the host and settles hardware crypto. Called from ImBox.cpp at
 * startup, and defensively by the three functions below.
 */
int dc_init_crypto();

/*
 * The smallest read that can open a header, rounded up to a whole sector.
 *
 * The library's cp_get_min_header_len does not round - the driver does that at
 * its own call sites against the device's sector size, and ImBox has no dc_dev
 * to ask. Unrounded, five or more key slots give a length that is not a
 * multiple of 512, which XTS cannot work in.
 */
int dc_get_min_header_len(dc_pass *password);

/*
 * Decrypt an encrypted header in user mode.
 *
 * out_header receives an allocated, decrypted header (caller must secure_free)
 * out_key    receives an allocated header key, or NULL if not wanted
 * out_len    receives the header's real length, which may exceed enc_len
 * out_kdf    receives the KDF that worked
 * out_slot   receives which credential opened it: 0 for the header password,
 *            a 1 based key slot index otherwise
 *
 * password->kdf may name one KDF or be KDF_ALL / KDF_DEFAULT; the search over
 * KDFs, key slots and ciphers all happens inside this one call.
 *
 * out_slot is the only way to learn which of the two it was, and the header
 * password is not interchangeable with a slot password: a slot holds the
 * header key wrapped under its own derived key, so only the header password
 * can re-wrap the other slots. Every operation that edits slots checks this.
 *
 * A buffer shorter than the header is not an error: read
 * dc_get_min_header_len() bytes, come here, and re-read if out_len says so.
 *
 * Returns ST_OK on success, an error code otherwise.
 */
int dc_decrypt_header(u8 *enc_header, int enc_len, dc_pass *password,
    dc_header **out_header, xts_key **out_key, int *out_len, int* out_kdf, int *out_slot);

/*
 * Seal a header for storage. Unlike cp_encrypt_header this allocates the
 * output and refreshes the CRC first, because ImBox builds headers field by
 * field and hands them over.
 *
 * out_enc receives the allocated ciphertext (caller must secure_free).
 * Returns ST_OK on success, an error code otherwise.
 */
int dc_encrypt_header(dc_header *header, int header_len, xts_key *hdr_key, u8 **out_enc);

#endif
