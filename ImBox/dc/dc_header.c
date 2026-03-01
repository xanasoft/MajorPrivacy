#include "include\defines.h"
#include "include\boot\dc_header.h"
#ifdef SMALL
#include "crypto_small\sha512_pkcs5_2_small.h"
#else
#include "crypto_fast/sha512_pkcs5_2.h"
#endif
#include "..\dc\crypto\Argon2\argon2.h"
#include "crypto_fast/crc32.h"

int dc_derive_key(dc_pass* password, u8* salt, u8* dk)
{
	if (password->cost == 0) {
		/* Existing SHA512-PBKDF2 */
		sha512_pkcs5_2(1000, password->pass, password->size, salt, PKCS5_SALT_SIZE, dk, PKCS_DERIVE_MAX);
	} else {
		/* Argon2id key derivation */

		// Compute the memory cost (m_cost) in MiB
		int m_cost_mib = 64 + (password->cost - 1) * 32;
		if (m_cost_mib > 1024) // Cap the memory cost at 1024 MiB
			m_cost_mib = 1024;

		// Convert memory cost to KiB for Argon2
		UINT memory_cost = m_cost_mib * 1024;

		// Compute the time cost
		UINT time_cost;
		if (password->cost <= 31)
			time_cost = 3 + ((password->cost - 1) / 3);
		else
			time_cost = 13 + (password->cost - 31);

		// single-threaded
		UINT parallelism = 1;

		int ret = argon2id_hash_raw(time_cost, memory_cost, parallelism, password->pass, password->size, salt, PKCS5_SALT_SIZE, dk, PKCS_DERIVE_MAX, NULL);
		if (ret != ARGON2_OK) {
			return 0;
		}
	}
	return 1;
}

int dc_encrypt_header(dc_header* header, dc_pass* password, UCHAR* salt)
{
	u8        dk[DISKKEY_SIZE];
	xts_key   hdr_key;

	if (!dc_derive_key(password, salt, dk))
		return 0;

	// initialize encryption keys
	xts_set_key(dk, header->alg_1, &hdr_key);

	// encrypt the volume header
	xts_encrypt((const unsigned char*)header, (unsigned char*)header, sizeof(dc_header), 0, &hdr_key);

	// save salt
	memcpy(header->salt, salt, PKCS5_SALT_SIZE);

	/* prevent leaks */
	memset(dk, 0, sizeof(dk));
	memset(&hdr_key, 0, sizeof(xts_key));

	return 1;
}

int dc_decrypt_header(dc_header *header, dc_pass *password)
{
	u8        dk[DISKKEY_SIZE];
	int       i, succs = 0;
	xts_key   hdr_key;
	dc_header hcopy;

	if (!dc_derive_key(password, header->salt, dk))
		return 0;

	for (i = 0; i < CF_CIPHERS_NUM; i++)
	{
		xts_set_key(dk, i, &hdr_key);

		xts_decrypt(pv(header), pv(&hcopy), sizeof(dc_header), 0, &hdr_key);

		/* Magic 'DCRP' */
		if (hcopy.sign != DC_VOLUME_SIGN) {
			continue;
		}

		/* Check CRC of header */
		if (hcopy.hdr_crc != crc32(pv(&hcopy.version), DC_CRC_AREA_SIZE)) {
			continue;
		}

		/* copy decrypted part to output */
		memcpy(&header->sign, &hcopy.sign, DC_ENCRYPTEDDATASIZE);
		succs = 1; break;
	}

	/* prevent leaks */
	memset(dk, 0, sizeof(dk));
	memset(&hdr_key, 0, sizeof(xts_key));
	memset(&hcopy, 0, sizeof(dc_header));

	return succs;
}
