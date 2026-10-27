/*
    * DCrypt volume_lib - keyfile mixing
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    * See keyfile_mix.h. Consolidated from dcapi/keyfiles.c and DcsPkg's
    * dc_keyfiles.c, which had the same scheme written twice - same domain
    * strings, same sort-then-deduplicate, same fold - differing only in how
    * they allocated, how they wiped, and which sort they had available.
    *
    * The sort here is the insertion sort DCS was using rather than dcapi's
    * qsort: there is no qsort in kernel mode or in firmware, the arrays are
    * tens of entries at most, and one sort means one order to depend on.
*/
#include "defines.h"
#include "dc_internal.h"
#include "keyfile_mix.h"
#include "sha512.h"

/*
 * Domain separation. These two strings are why a keyfile-set digest can never
 * be mistaken for a password-plus-keyfiles digest, and they are part of the
 * on-disk contract: change either and every volume that used keyfiles stops
 * opening.
 */
static const char kf_domain_set[]  = "DC2-KFSET-v1";
static const char kf_domain_pass[] = "DC2-PW-KF-v1";

/* ---------------------------------------------------------------------------
 * Hashing
 * ------------------------------------------------------------------------ */

int cp_kf_hash_data(const void *data, u32 size, u8 *out_hash)
{
	sha512_ctx sha;

	if (data == NULL || size == 0 || out_hash == NULL) {
		return ST_INVALID_PARAM;
	}

	sha512_init(&sha);
	sha512_hash(&sha, (const unsigned char*)data, size);
	sha512_done(&sha, out_hash);

	burn(&sha, sizeof(sha));
	return ST_OK;
}

/* ---------------------------------------------------------------------------
 * The hash set
 * ------------------------------------------------------------------------ */

int cp_kf_mixer_init(dc_kf_mixer *ctx)
{
	if (ctx == NULL) {
		return ST_INVALID_PARAM;
	}

	ctx->hashes   = NULL;
	ctx->count    = 0;
	ctx->capacity = 0;
	return ST_OK;
}

void cp_kf_mixer_free(dc_kf_mixer *ctx)
{
	if (ctx == NULL) {
		return;
	}

	if (ctx->hashes != NULL) {
		/* these are keyfile digests: wipe before releasing */
		burn(ctx->hashes, (size_t)ctx->capacity * DC_KF_HASH_SIZE);
		dc_free(ctx->hashes);
	}

	ctx->hashes   = NULL;
	ctx->count    = 0;
	ctx->capacity = 0;
}

static int kf_grow(dc_kf_mixer *ctx)
{
	int  want;
	u8  *bigger;

	if (ctx->count < ctx->capacity) {
		return ST_OK;
	}

	want = (ctx->capacity != 0) ? ctx->capacity * 2 : DC_KF_MIXER_INITIAL_CAPACITY;

	if ((bigger = dc_alloc((size_t)want * DC_KF_HASH_SIZE)) == NULL) {
		return ST_NOMEM;
	}

	if (ctx->hashes != NULL) {
		memcpy(bigger, ctx->hashes, (size_t)ctx->count * DC_KF_HASH_SIZE);
		burn(ctx->hashes, (size_t)ctx->capacity * DC_KF_HASH_SIZE);
		dc_free(ctx->hashes);
	}

	ctx->hashes   = bigger;
	ctx->capacity = want;
	return ST_OK;
}

int cp_kf_mixer_add_hash(dc_kf_mixer *ctx, const u8 *hash)
{
	int resl;

	if (ctx == NULL || hash == NULL) {
		return ST_INVALID_PARAM;
	}

	if ((resl = kf_grow(ctx)) != ST_OK) {
		return resl;
	}

	memcpy(ctx->hashes + (size_t)ctx->count * DC_KF_HASH_SIZE, hash, DC_KF_HASH_SIZE);
	ctx->count++;
	return ST_OK;
}

int cp_kf_mixer_add_data(dc_kf_mixer *ctx, const void *data, u32 size)
{
	u8  hash[DC_KF_HASH_SIZE];
	int resl;

	if (ctx == NULL) {
		return ST_INVALID_PARAM;
	}

	if ((resl = cp_kf_hash_data(data, size, hash)) == ST_OK) {
		resl = cp_kf_mixer_add_hash(ctx, hash);
	}

	burn(hash, sizeof(hash));
	return resl;
}

/* ---------------------------------------------------------------------------
 * Ordering
 * ------------------------------------------------------------------------ */

static int kf_hash_cmp(const u8 *a, const u8 *b)
{
	int i;

	for (i = 0; i < DC_KF_HASH_SIZE; i++) {
		if (a[i] != b[i]) {
			return (a[i] < b[i]) ? -1 : 1;
		}
	}
	return 0;
}

/*
 * Sorting is what makes the result independent of the order the caller added
 * keyfiles in - which matters because a user picking three files in a dialogue
 * has no idea what order they were enumerated in.
 */
static void kf_sort(u8 *hashes, int count)
{
	u8  held[DC_KF_HASH_SIZE];
	int i, j;

	for (i = 1; i < count; i++)
	{
		memcpy(held, hashes + (size_t)i * DC_KF_HASH_SIZE, DC_KF_HASH_SIZE);

		for (j = i - 1;
		     j >= 0 && kf_hash_cmp(hashes + (size_t)j * DC_KF_HASH_SIZE, held) > 0;
		     j--)
		{
			memcpy(hashes + (size_t)(j + 1) * DC_KF_HASH_SIZE,
			       hashes + (size_t)j * DC_KF_HASH_SIZE, DC_KF_HASH_SIZE);
		}

		memcpy(hashes + (size_t)(j + 1) * DC_KF_HASH_SIZE, held, DC_KF_HASH_SIZE);
	}

	burn(held, sizeof(held));
}

/*
 * Drop repeats, which are adjacent once sorted.
 *
 * Selecting the same keyfile twice - directly and again through the folder it
 * sits in - must mean the same thing as selecting it once, or the result would
 * depend on how the user happened to reach it.
 *
 * Compacts in one forward sweep rather than closing each gap as it is found.
 * Two reasons: it is O(n) instead of O(n-squared), and the copy is between
 * disjoint slots, so it needs memcpy and not memmove. EDK2 has no C runtime -
 * CryptoPkg's IntrinsicLib supplies memcpy and memset to satisfy the compiler's
 * own intrinsics, and nothing else. A memmove here links everywhere except the
 * EFI bootloader, which is the worst place to find out.
 */
static int kf_dedup(u8 *hashes, int count)
{
	int i;
	int out;

	if (count < 2) {
		return count;
	}

	for (i = 1, out = 1; i < count; i++)
	{
		const u8 *cur = hashes + (size_t)i * DC_KF_HASH_SIZE;

		/* compare against the last one kept, not the last one seen */
		if (kf_hash_cmp(hashes + (size_t)(out - 1) * DC_KF_HASH_SIZE, cur) == 0) {
			continue;
		}

		if (out != i) {
			memcpy(hashes + (size_t)out * DC_KF_HASH_SIZE, cur, DC_KF_HASH_SIZE);
		}
		out++;
	}

	return out;
}

/* ---------------------------------------------------------------------------
 * Mixing
 * ------------------------------------------------------------------------ */

void cp_kf_mixer_combine(dc_pass *pass, const u8 *keyfiles_hash)
{
	sha512_ctx sha;

	if (pass == NULL || keyfiles_hash == NULL) {
		return;
	}

	if (pass->size == 0) {
		/* nothing to separate the keyfiles from; the digest is the password */
		memcpy(pass->pass, keyfiles_hash, DC_KF_HASH_SIZE);
		pass->size = DC_KF_HASH_SIZE;
		return;
	}

	sha512_init(&sha);
	sha512_hash(&sha, (const unsigned char*)kf_domain_pass, sizeof(kf_domain_pass) - 1);
	sha512_hash(&sha, p8(pass->pass), pass->size);
	sha512_hash(&sha, keyfiles_hash, DC_KF_HASH_SIZE);
	sha512_done(&sha, p8(pass->pass));

	pass->size = DC_KF_HASH_SIZE;

	burn(&sha, sizeof(sha));
}

int cp_kf_mixer_finish(dc_kf_mixer *ctx, dc_pass *pass)
{
	sha512_ctx sha;
	u8         set_hash[DC_KF_HASH_SIZE];
	int        i;

	if (ctx == NULL || pass == NULL) {
		return ST_INVALID_PARAM;
	}

	if (ctx->count == 0) {
		cp_kf_mixer_free(ctx);
		return ST_EMPTY_KEYFILES;
	}

	kf_sort(ctx->hashes, ctx->count);
	ctx->count = kf_dedup(ctx->hashes, ctx->count);

	/* one digest over the whole set, in its canonical order */
	sha512_init(&sha);
	sha512_hash(&sha, (const unsigned char*)kf_domain_set, sizeof(kf_domain_set) - 1);
	for (i = 0; i < ctx->count; i++) {
		sha512_hash(&sha, ctx->hashes + (size_t)i * DC_KF_HASH_SIZE, DC_KF_HASH_SIZE);
	}
	sha512_done(&sha, set_hash);
	burn(&sha, sizeof(sha));

	cp_kf_mixer_combine(pass, set_hash);

	burn(set_hash, sizeof(set_hash));
	cp_kf_mixer_free(ctx);

	return ST_OK;
}

void cp_kf_mix_additive(dc_pass *pass, const u8 *hash)
{
	u32       *acc;
	const u32 *add;
	int        words, i;

	if (pass == NULL || hash == NULL) {
		return;
	}

	/*
	 * The addition runs over the whole digest width whatever the password
	 * length, so the bytes past the password have to hold a defined value
	 * first. They are part of the sum either way.
	 */
	memset(p8(pass->pass) + pass->size, 0, (MAX_PASSWORD * 2) - pass->size);

	acc   = p32(pass->pass);
	add   = (const u32*)hash;
	words = DC_KF_HASH_SIZE / (int)sizeof(u32);

	for (i = 0; i < words; i++) {
		/* wraps at 32 bits, and always has - this is the v1 contract */
		acc[i] = acc[i] + add[i];
	}

	/* the result is as long as the longer of the two inputs */
	if (pass->size < DC_KF_HASH_SIZE) {
		pass->size = DC_KF_HASH_SIZE;
	}
}
