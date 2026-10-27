/*
 * Copyright 2026 David Xanatos, xanasoft.com
 */

/*
 * This file no longer implements anything about the volume header. It is the
 * translation layer between the names CryptoIO.cpp calls and DiskCryptor's
 * volume_lib, which the driver, dcapi and the EFI bootloader also build.
 *
 * It used to be a fourth copy of that logic - 742 lines of it, forked long
 * enough ago that it had drifted: its calculate_header_crc_um masked footer_cnt
 * to five unsigned bits where the format defines it as signed, and its
 * dc_derive_key_um never returned success on the raw-key path. Both are fixed
 * by deleting them.
 *
 * The signatures below are unchanged on purpose, so CryptoIO.cpp keeps
 * building as it was. The _um suffix survives only as a name; the user-mode
 * implementation it once distinguished itself from is now the same code the
 * driver runs.
 *
 * One thing the library gained rather than lost in the move: the Argon2 cost
 * encoding 0.99.7 used (1000 + cost) lived only here, and would have taken
 * those containers with it. It is in volume_lib/crypto_head.c now, because the
 * KDF search runs inside the library and an adapter never sees those calls.
 *
 * See DiskCryptor/DCrypt/volume_lib/DESIGN.md.
 */

#include "include/dc_header.h"

/* volume_lib */
#include "dc_dev.h"
#include "crypto_head.h"
#include "head_util.h"
/* calculate_header_crc and is_volume_header_correct live here, with the device
   I/O this build does not otherwise use */
#include "header_io.h"
#include "crc32.h"

/* ---------------------------------------------------------------------------
 * Host
 * ------------------------------------------------------------------------ */

/*
 * secure_alloc, secure_free and make_rand are CryptoIO.cpp's; the library only
 * needs to be told where they are. There is no wipe hook because ImBox never
 * calls dc_change_pass_bak, and no log hook because ImBox has no DbgMsg.
 */
static void *imbox_secure_alloc(size_t size)
{
    return secure_alloc((unsigned long)size);
}

static void imbox_secure_free(void *p)
{
    secure_free(p);
}

static void imbox_rand_bytes(void *buff, size_t size)
{
    make_rand(buff, size);
}

static const dc_host g_imbox_host = {
    imbox_secure_alloc,
    imbox_secure_free,
    imbox_rand_bytes,
    NULL,       /* no entropy contribution: BCryptGenRandom is the source */
    NULL,       /* no diagnostic sink */
    NULL        /* no wipe */
};

int dc_init_crypto()
{
    static int initialized = 0;

    if (!initialized)
    {
        dc_lib_init(&g_imbox_host);

        /* ImBox has no configuration to consult, and every target it builds
           for either has AES-NI/ARM64 crypto extensions or falls back on its
           own inside xts_init. */
        xts_init(1);

        initialized = 1;
    }
    return 1;
}

/* ---------------------------------------------------------------------------
 * The two things ImBox asks for that the library does not offer directly
 * ------------------------------------------------------------------------ */

int dc_get_min_header_len(dc_pass *password)
{
    /*
     * Rounded up to a whole sector, which the library's raw answer is not.
     *
     * cp_get_min_header_len returns max(DC_BASE_SIZE + slot_size * n,
     * DC_AREA_SIZE), and slot_size is PKCS_DERIVE_MAX + sizeof(dc_slot_info) =
     * 224, so from five slots on it lands off a sector boundary - 1024 + 5*224
     * is 2144. CCryptoIO uses this value directly as a read length and, in
     * ChangePassword, as the length it re-seals the header with, and XTS
     * counts down whole sectors: an unaligned length underflows the loop.
     *
     * The driver never sees this because io_read_header and dc_probe_decrypt_at
     * wrap the same call in ROUND_TO_FULL_SECTORS against the device's sector
     * size. ImBox has no dc_dev to ask, and its own DiskRead and DiskWrite
     * already assert 512-byte multiples, so that is the size to round to.
     */
    int len = cp_get_min_header_len(password);

    return ((len + XTS_SECTOR_SIZE - 1) / XTS_SECTOR_SIZE) * XTS_SECTOR_SIZE;
}

/* ---------------------------------------------------------------------------
 * Whole-header operations
 * ------------------------------------------------------------------------ */

int dc_decrypt_header(u8 *enc_header, int enc_len, dc_pass *password,
    dc_header **out_header, xts_key **out_key, int *out_len, int *out_kdf, int *out_slot)
{
    dc_header *header  = NULL;
    xts_key   *hdr_key = NULL;
    int        head_len = DC_AREA_SIZE;
    int        resl = ST_ERROR;

    dc_init_crypto();

    if (enc_header == NULL || out_header == NULL) return ST_INVALID_PARAM;
    if (enc_len < DC_AREA_SIZE) return ST_INV_VOLUME;

    do
    {
        if ((header = (dc_header*)secure_alloc(enc_len)) == NULL) { resl = ST_NOMEM; break; }
        if ((hdr_key = (xts_key*)secure_alloc(sizeof(xts_key))) == NULL) { resl = ST_NOMEM; break; }

        memcpy(header, enc_header, enc_len);

        /* the whole search - every KDF, every slot, every cipher - is one call.
           The loop over dc_all_kdfs / dc_default_kdfs that used to be here is
           inside it. */
        if (cp_decrypt_header(hdr_key, header, enc_len, password, out_kdf, NULL, out_slot, NULL) == 0) {
            resl = ST_PASS_ERR; break;
        }

        if (header->version >= DC_HDR_VERSION_2)
        {
            int have, tail;

            head_len = header->head_len;
            if (head_len < DC_AREA_SIZE || head_len > DC_AREA_MAX_SIZE) {
                resl = ST_INV_VOLUME; break;
            }

            /*
             * A buffer shorter than the header is not an error.
             *
             * head_len is inside the header, so a caller cannot know it before
             * the base is open. The normal flow is to read
             * cp_get_min_header_len() bytes, come here, and re-read once
             * out_len says how long the header actually is - ImBox writes 64 KiB
             * headers and opens them from a 2 KiB read. Refusing the short
             * buffer made every v2 volume with a head_len above the minimum
             * fail to open, and fail as ST_INV_VOLUME, which the callers report
             * as a wrong password.
             *
             * So decrypt as much as was actually read. Whatever the caller has,
             * it gets in the clear; out_len tells it whether to come back for
             * the rest.
             */
            have = (enc_len < head_len) ? enc_len : head_len;

            /* cp_decrypt_header covers DC_AREA_SIZE; the tail continues at that
               XTS offset. Whole sectors only - xts_decrypt counts down by
               XTS_SECTOR_SIZE and a partial one would run off the end - so a
               short read that stops mid-sector leaves that sector for the
               re-read. */
            tail = ((have - DC_AREA_SIZE) / XTS_SECTOR_SIZE) * XTS_SECTOR_SIZE;
            if (tail > 0) {
                xts_decrypt(enc_header + DC_AREA_SIZE, ((u8*)header) + DC_AREA_SIZE,
                            tail, DC_AREA_SIZE, hdr_key);
            }

            /*
             * Slots are wrapped under their own keys, so carry them over raw -
             * but only once the whole slot area plus its descriptors is in the
             * buffer. cp_copy_keylots checks them against head_len, not against
             * what was read, so on a short buffer it would copy past the end.
             * cp_get_min_header_len sizes the first read to cover the slots
             * whenever the password names one, so this costs nothing there.
             */
            if (header->feature_flags & FF_KEY_SLOTS) {
                u32 slots_end = (u32)DC_BASE_SIZE + header->slot_area_len +
                                (u32)header->key_slot_count * header->slot_info_size;

                if (slots_end <= (u32)enc_len) {
                    cp_copy_keylots(header, enc_header, (u8*)header);
                }
            }
        }

        *out_header = header;
        header = NULL;
        if (out_key != NULL) { *out_key = hdr_key; hdr_key = NULL; }
        if (out_len != NULL) *out_len = head_len;
        resl = ST_OK;
    } while (0);

    if (header  != NULL) secure_free(header);
    if (hdr_key != NULL) secure_free(hdr_key);
    return resl;
}

int dc_encrypt_header(dc_header *header, int header_len, xts_key *hdr_key, u8 **out_enc)
{
    dc_header *scratch = NULL;
    u8        *enc = NULL;
    int        resl;

    dc_init_crypto();

    if (header == NULL || hdr_key == NULL || out_enc == NULL) return ST_ERROR;

    /*
     * The CRC is refreshed before sealing, as it always was here: ImBox builds
     * a header field by field and hands it over. cp_encrypt_header seals what
     * it is given, so the CRC is fixed on a copy rather than on the caller's
     * header.
     */
    if ((scratch = (dc_header*)secure_alloc(header_len)) == NULL) return ST_NOMEM;
    if ((enc = (u8*)secure_alloc(header_len)) == NULL) {
        secure_free(scratch);
        return ST_NOMEM;
    }

    memcpy(scratch, header, header_len);
    scratch->hdr_crc = calculate_header_crc(scratch);

    resl = cp_encrypt_header(scratch, header_len, hdr_key, enc);

    secure_free(scratch);

    if (resl != ST_OK) {
        secure_free(enc);
        return resl;
    }
    *out_enc = enc;
    return ST_OK;
}
