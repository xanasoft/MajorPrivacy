/*
    * DiskCryptor crypto_lib - Serpent-256
    * Copyright (c) 2026 David Xanatos <info@diskcryptor.org>
    *
    * SPDX-License-Identifier: MIT
    *
    *
    * This library is distributed in the hope that it will be useful, but
    * WITHOUT ANY WARRANTY; without even the implied warranty of
    * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
    * General Public License for more details.
*/
#ifndef _CL_SERPENT_H_
#define _CL_SERPENT_H_

#define SERPENT_KEY_SIZE     32
#define SERPENT_EXPKEY_WORDS 132
#define SERPENT_BLOCK_SIZE   16

typedef struct _serpent256_key {
	unsigned long expkey[SERPENT_EXPKEY_WORDS];
} serpent256_key;

void _stdcall serpent256_set_key(const unsigned char *key, serpent256_key *skey);
void _stdcall serpent256_encrypt(const unsigned char *in, unsigned char *out, serpent256_key *key);
void _stdcall serpent256_decrypt(const unsigned char *in, unsigned char *out, serpent256_key *key);

#endif
