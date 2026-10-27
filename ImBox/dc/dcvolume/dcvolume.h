/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

/*
 * dcvolume.dll - DiskCryptor's volume_lib and crypto_lib, as one library.
 *
 * Two consumers, reaching it two different ways.
 *
 *   ImBox.exe links dcvolume.lib and is bound by the Windows loader before
 *   main. It has no meaningful existence without the library - every container
 *   it opens goes through it - so a missing DLL should stop it starting, and
 *   an implicit import says exactly that.
 *
 *   MajorPrivacy.exe loads it at run time, and only when the header editor is
 *   opened. The GUI is a privacy suite that happens to be able to edit volume
 *   headers; it must start, and keep working, on a machine where this DLL was
 *   never installed. So nothing links against it, and the editor reports a
 *   plain error instead of the process failing to load.
 *
 * For the second case there is one export to resolve rather than forty:
 * dcvol_get_api() hands back a table of every function below. Resolve it,
 * check the version, and call through the table.
 *
 * Every function keeps the name and signature it has inside DiskCryptor -
 * cp_* from volume_lib, dc_* from the driver and from ImBox's adapter,
 * crc32/xts_* from crypto_lib. There is no dcvolume-specific renaming, so the
 * DiskCryptor sources remain the documentation for all of it. The only
 * additions carry a dcvol_ prefix and are listed at the end.
 */

#ifndef _DCVOLUME_H_
#define _DCVOLUME_H_

#include "defines.h"
#include "dc_status.h"
#include "volume_header.h"
#include "xts_fast.h"
#include "dc_dev.h"
#include "crypto_head.h"
#include "head_util.h"
#include "header_io.h"
#include "volume_head.h"
#include "keyfile_mix.h"
#include "crc32.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bump when the table below changes shape. A consumer that asks for a version
 * this DLL does not implement gets NULL rather than a table it would misread.
 */
#define DCVOL_API_VERSION  2

/*
 * The extended header's payload is deliberately not handled here.
 *
 * It is a tagged variant buffer, and MajorPrivacy already has a reader and
 * writer for that exact encoding in FW::CVariant - the two share VariantDefs.h,
 * and each parses what the other writes. Carrying dcapi's C implementation as
 * well would have put a third copy of one format in the process, and a GPL file
 * inside a library that is otherwise MIT crypto_lib and volume_lib.
 *
 * So the block's frame stays here, where the header format lives - get_ext_header
 * finds and validates it, cp_ext_header_crc checksums it - and what is inside it
 * is the consumer's business.
 */

/* ---------------------------------------------------------------------------
 * Host services
 *
 * These four are the library's own, not the caller's. crypto_lib's Argon2
 * links to secure_alloc/secure_free by name (see crypto_lib/cl_alloc.h) and
 * volume_lib reaches them through the dc_host it is given, so they have to
 * live on this side of the boundary. ImBox used to define them itself and now
 * imports them, which keeps one allocator across the whole process.
 *
 * secure_alloc returns page-locked memory that secure_free wipes before
 * releasing. Use it for anything that is, or was derived from, a password.
 * ------------------------------------------------------------------------ */

void *secure_alloc(unsigned long length);
void  secure_free(void *ptr);
void  burn(void *ptr, size_t size);
void  make_rand(void *ptr, size_t size);

/*
 * Installs the host and settles hardware crypto. Idempotent; the DLL calls it
 * on load, so a consumer only needs it to be sure.
 */
int dc_init_crypto(void);

/* ---------------------------------------------------------------------------
 * Whole-header operations - ImBox's adapter, see dc\dc_header.c
 * ------------------------------------------------------------------------ */

int dc_get_min_header_len(dc_pass *password);

int dc_decrypt_header(u8 *enc_header, int enc_len, dc_pass *password,
    dc_header **out_header, xts_key **out_key, int *out_len, int *out_kdf, int *out_slot);

int dc_encrypt_header(dc_header *header, int header_len, xts_key *hdr_key, u8 **out_enc);

/* ---------------------------------------------------------------------------
 * The function table
 *
 * One entry per export, in the order they are declared above. Resolve
 * "dcvol_get_api", call it with DCVOL_API_VERSION, and use what comes back.
 * ------------------------------------------------------------------------ */

typedef struct _dcvol_api {

	u32   version;      /* DCVOL_API_VERSION this table was built for */
	u32   size;         /* sizeof(dcvol_api), so a newer DLL can be detected */

	/* host services */
	void *(*secure_alloc)(unsigned long length);
	void  (*secure_free)(void *ptr);
	void  (*burn)(void *ptr, size_t size);
	void  (*make_rand)(void *ptr, size_t size);
	int   (*dc_init_crypto)(void);

	/* whole-header */
	int   (*dc_get_min_header_len)(dc_pass *password);
	int   (*dc_decrypt_header)(u8 *enc_header, int enc_len, dc_pass *password,
	                           dc_header **out_header, xts_key **out_key,
	                           int *out_len, int *out_kdf, int *out_slot);
	int   (*dc_encrypt_header)(dc_header *header, int header_len, xts_key *hdr_key, u8 **out_enc);
	int   (*cp_encrypt_header)(dc_header *header, int len, xts_key *hdr_key, u8 *out);
	int   (*cp_decrypt_header_with_key)(const u8 *blob, int len, xts_key *hdr_key, dc_header *out);
	int   (*cp_decrypt_header)(xts_key *hdr_key, dc_header *header, int hdr_len, dc_pass *password,
	                           int *out_kdf, u8 *out_dk, int *out_slot, ULONG *interrupt_cmd);
	int   (*cp_set_header_key)(xts_key *hdr_key, u8 *salt, int cipher, dc_pass *password, ULONG *interrupt_cmd);
	int   (*cp_get_min_header_len)(dc_pass *password);

	/* header validation */
	unsigned long (*calculate_header_crc)(dc_header *header);
	BOOLEAN       (*is_volume_header_correct)(dc_header *header);

	/* key derivation */
	int   (*dc_derive_key)(dc_pass *password, int kdf, u8 *salt, u8 *dk, ULONG *interrupt_cmd);
	int   (*argon2_mk_params)(int kdf, u32 *memory_cost, u32 *time_cost, u32 *parallelism);
	const int *(*cp_kdf_list)(int selector);

	/* key slots */
	int   (*cp_has_key_slots)(dc_header *header);
	int   (*cp_get_slot_info)(dc_header *header, int slot_idx, dc_slot_info *info);
	int   (*cp_get_slot_payload)(dc_header *header, int slot_idx, u8 *payload, int len);
	int   (*cp_set_slot)(dc_header *header, int slot_idx, dc_slot_info *info, u8 *payload, int len);
	int   (*cp_get_key_slot_size)(int type);
	int   (*cp_wrap_header_key)(u8 *slot, u8 *sk, u8 *dk, int type);
	int   (*cp_swap_slot_key)(u8 *slot, u8 *old_dk, u8 *new_dk, int type);
	int   (*cp_copy_keylots)(dc_header *header, u8 *in_buff, u8 *out_buff);
	int   (*dc_change_slot_pass)(dc_header *header, dc_pass *old_pass, dc_pass *new_pass, ULONG *interrupt_cmd);
	int   (*dc_update_key_slots)(dc_header *header, xts_key **hdr_key, dc_pass *old_pass,
	                             dc_pass *new_pass, ULONG *interrupt_cmd);

	/* extended header - the frame only, see above */
	int   (*get_ext_header)(dc_header *header, dc_ext_header **out_ext_hdr);
	unsigned long (*cp_ext_header_crc)(dc_ext_header *ext_hdr);

	/* keyfiles - the GUI reads the files, this folds them into the password */
	int   (*cp_kf_hash_data)(const void *data, u32 size, u8 *out_hash);
	int   (*cp_kf_mixer_init)(dc_kf_mixer *ctx);
	void  (*cp_kf_mixer_free)(dc_kf_mixer *ctx);
	int   (*cp_kf_mixer_add_hash)(dc_kf_mixer *ctx, const u8 *hash);
	int   (*cp_kf_mixer_add_data)(dc_kf_mixer *ctx, const void *data, u32 size);
	void  (*cp_kf_mixer_combine)(dc_pass *pass, const u8 *keyfiles_hash);
	int   (*cp_kf_mixer_finish)(dc_kf_mixer *ctx, dc_pass *pass);
	void  (*cp_kf_mix_additive)(dc_pass *pass, const u8 *hash);

	/* a header in a buffer, for working on a backup file without a device */
	int      (*dc_dev_from_buffer)(dc_dev *dev, dc_buffer *desc, void *buff, u32 size, u32 bps);
	dc_rw_fn (*dc_buffer_rw)(void);
	int      (*io_read_header)(dc_dev *dev, dc_rw_fn rw, u64 pos, dc_header **header,
	                           xts_key **out_key, dc_pass *password, int *out_kdf, ULONG *interrupt_cmd);
	int      (*io_write_header)(dc_dev *dev, dc_rw_fn rw, u64 pos, dc_header *header,
	                            xts_key *hdr_key, dc_pass *password, u32 flags, ULONG *interrupt_cmd);

	/* crypto_lib odds and ends */
	int           (_stdcall *xts_set_key)(const unsigned char *key, int alg, xts_key *skey);
	unsigned long (_stdcall *crc32)(const unsigned char *p, unsigned long len);
	unsigned long (_stdcall *crc32_combine)(unsigned long crc1, unsigned long crc2, unsigned long len2);

} dcvol_api;

/*
 * The one export a dynamic consumer has to resolve. Returns NULL for a version
 * this DLL does not implement.
 */
const dcvol_api *dcvol_get_api(unsigned int version);

typedef const dcvol_api *(*dcvol_get_api_fn)(unsigned int version);

#define DCVOL_GET_API_NAME  "dcvol_get_api"
#define DCVOL_DLL_NAME      L"dcvolume.dll"

#ifdef __cplusplus
}
#endif

#endif /* _DCVOLUME_H_ */
