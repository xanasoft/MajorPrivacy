#pragma once
/*
 * The three status codes DiskCryptor's FAT and ISO image writers return.
 *
 * fat_sup.c and iso_sup.c are compiled here exactly as DiskCryptor ships them,
 * so that they can be refreshed from upstream without a merge. They include
 * "dcconst.h" and "misc.h", and a quoted include finds the copy sitting next to
 * the source first - so these two files stand in for DiskCryptor's without
 * either source being touched.
 *
 * Only what those two files actually use is here. DiskCryptor's real dcconst.h
 * is several hundred status codes and driver interface definitions, none of
 * which an image writer has any use for.
 */

#define ST_OK		0
#define ST_ERROR	1
#define ST_NOMEM	9
