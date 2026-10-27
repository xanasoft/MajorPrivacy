/*
    * DCrypt volume_lib - keyfile mixing
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * How keyfiles fold into a password. Two schemes live here, and which one a
    * volume needs is decided by whoever created it, so both have to stay.
    *
    *   v1, additive. Each keyfile's SHA-512 is added into the password buffer
    *   word by word. Order-independent because addition is, but so is
    *   everything else about it: adding the same keyfile twice is not the same
    *   as adding it once, a keyfile can be cancelled by another, and the result
    *   is only as long as the longer of the two inputs.
    *
    *   v2, canonical. Every keyfile is hashed, the hashes are sorted and
    *   deduplicated, and the set is folded into one digest under a domain
    *   string; that digest is then mixed with the password under a second
    *   domain string. Order-independent by construction rather than by
    *   accident, duplicate-proof, and separated from the password hash so that
    *   neither can be substituted for the other.
    *
    * Only the mixing is here. Reading a keyfile is not portable - dcapi walks
    * directories with FindFirstFile, DCS reads through EFI_FILE_PROTOCOL - so
    * each host does its own I/O and hands over either the bytes or a finished
    * hash.
*/
#ifndef _KEYFILE_MIX_H_
#define _KEYFILE_MIX_H_

#include "defines.h"
#include "volume_header.h"
#include "dc_status.h"

/* SHA-512, and the unit everything here counts in */
#define DC_KF_HASH_SIZE              64

/* grown by doubling; only affects allocation, never the result */
#define DC_KF_MIXER_INITIAL_CAPACITY 16

typedef struct _dc_kf_mixer {
	u8  *hashes;    /* count hashes of DC_KF_HASH_SIZE bytes, in the order added */
	int  count;
	int  capacity;
} dc_kf_mixer;

/* ---------------------------------------------------------------------------
 * v2 - the canonical scheme
 * ------------------------------------------------------------------------ */

/* SHA-512 of a buffer, which is what a keyfile's hash is. out_hash takes
   DC_KF_HASH_SIZE bytes. */
int cp_kf_hash_data(const void *data, u32 size, u8 *out_hash);

int  cp_kf_mixer_init(dc_kf_mixer *ctx);

/* Safe on a zeroed or already-freed context, and leaves one that
   cp_kf_mixer_init can be called on again. */
void cp_kf_mixer_free(dc_kf_mixer *ctx);

/* Add a hash the caller computed itself - the hook for a host that read the
   keyfile its own way. */
int cp_kf_mixer_add_hash(dc_kf_mixer *ctx, const u8 *hash);

/* Hash these bytes and add that. */
int cp_kf_mixer_add_data(dc_kf_mixer *ctx, const void *data, u32 size);

/*
 * Fold one finished keyfile-set digest into the password.
 *
 * Separate from finish() because a caller may already hold the digest - DCS
 * takes one straight from the TPM - and because a single keyfile's own hash is
 * a valid set digest for a one-element set.
 *
 * An empty password is replaced by the digest rather than hashed with it: there
 * is nothing to separate it from.
 */
void cp_kf_mixer_combine(dc_pass *pass, const u8 *keyfiles_hash);

/*
 * Sort, deduplicate, fold into one digest, mix into the password, and release
 * the context. Returns ST_EMPTY_KEYFILES if nothing was ever added, in which
 * case the password is left alone.
 */
int cp_kf_mixer_finish(dc_kf_mixer *ctx, dc_pass *pass);

/* ---------------------------------------------------------------------------
 * v1 - the additive scheme
 * ------------------------------------------------------------------------ */

/*
 * Add one keyfile hash into the password, the way DiskCryptor always did.
 *
 * Kept bit-exact: every volume whose keyfiles were applied this way depends on
 * the arithmetic, wraparound included.
 */
void cp_kf_mix_additive(dc_pass *pass, const u8 *hash);

#endif
