/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

/*
 * dcvolume.dll - see dcvolume.h.
 *
 * Three things live here and nothing else does. The library proper is
 * DiskCryptor's, compiled in unchanged.
 *
 *   The host services. secure_alloc, secure_free, burn and make_rand were
 *   ImBox's, in CryptoIO.cpp. They had to move: crypto_lib's Argon2 links to
 *   secure_alloc by name, so whichever binary contains Argon2 has to contain
 *   that symbol, and that is now this one. ImBox imports them back, which is
 *   an improvement on the previous arrangement rather than a concession to it -
 *   there is one allocator in the process instead of one per binary.
 *
 *   The function table, for a consumer that loads this at run time.
 *
 * What is deliberately NOT here is the extended header's payload encoding. See
 * the note in dcvolume.h: the one consumer of it already has that codec.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include "dcvolume.h"

/* ---------------------------------------------------------------------------
 * Host services
 * ------------------------------------------------------------------------ */

/*
 * Every block carries its own length, so secure_free can wipe it without being
 * told how much. The header sits in front of the pointer handed out, which is
 * why secure_free walks backwards to find it.
 */
struct mem_block
{
	SIZE_T size;
	PVOID  data;
};

void *secure_alloc(unsigned long length)
{
	struct mem_block *mem;
	SIZE_T size = length;
	SIZE_T full_size = sizeof(struct mem_block) + size;

	/*
	 * On 32 bit the xts_key has to be executable, because the key schedule is
	 * reached through generated thunks. x64 and ARM64 do not, and crypto_lib
	 * does not build for 32 bit at all - but the distinction is kept, because
	 * it documents why the allocation is VirtualAlloc rather than malloc.
	 */
#ifdef _M_IX86
	mem = (struct mem_block*)VirtualAlloc(NULL, full_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
	mem = (struct mem_block*)VirtualAlloc(NULL, full_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
	if (mem == NULL)
		return NULL;

	mem->data = ((char*)mem) + sizeof(struct mem_block);
	mem->size = size;
	RtlSecureZeroMemory(mem->data, mem->size);

	/* keep it out of the pagefile; failure only costs the guarantee */
	VirtualLock(mem, full_size);
	return mem->data;
}

void secure_free(void *ptr)
{
	struct mem_block *mem;
	SIZE_T full_size;

	if (ptr == NULL)
		return;

	mem = (struct mem_block*)((BYTE*)ptr - sizeof(struct mem_block));
	if (mem->data != ptr) { /* sanity check, should never happen */
#ifdef _DEBUG
		DebugBreak();
#endif
		return;
	}

	full_size = sizeof(struct mem_block) + mem->size;
	RtlSecureZeroMemory(mem->data, mem->size);
	VirtualUnlock(mem, full_size);
	VirtualFree(mem, 0, MEM_RELEASE);
}

void burn(void *ptr, size_t size)
{
	RtlSecureZeroMemory(ptr, size);
}

void make_rand(void *ptr, size_t size)
{
	BCryptGenRandom(NULL, (BYTE*)ptr, (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

/* ---------------------------------------------------------------------------
 * The function table
 * ------------------------------------------------------------------------ */

static const dcvol_api g_api = {
	DCVOL_API_VERSION,
	(u32)sizeof(dcvol_api),

	secure_alloc,
	secure_free,
	burn,
	make_rand,
	dc_init_crypto,

	dc_get_min_header_len,
	dc_decrypt_header,
	dc_encrypt_header,
	cp_encrypt_header,
	cp_decrypt_header_with_key,
	cp_decrypt_header,
	cp_set_header_key,
	cp_get_min_header_len,

	calculate_header_crc,
	is_volume_header_correct,

	dc_derive_key,
	argon2_mk_params,
	cp_kdf_list,

	cp_has_key_slots,
	cp_get_slot_info,
	cp_get_slot_payload,
	cp_set_slot,
	cp_get_key_slot_size,
	cp_wrap_header_key,
	cp_swap_slot_key,
	cp_copy_keylots,
	dc_change_slot_pass,
	dc_update_key_slots,

	get_ext_header,
	cp_ext_header_crc,

	cp_kf_hash_data,
	cp_kf_mixer_init,
	cp_kf_mixer_free,
	cp_kf_mixer_add_hash,
	cp_kf_mixer_add_data,
	cp_kf_mixer_combine,
	cp_kf_mixer_finish,
	cp_kf_mix_additive,

	dc_dev_from_buffer,
	dc_buffer_rw,
	io_read_header,
	io_write_header,

	xts_set_key,
	crc32,
	crc32_combine,
};

const dcvol_api *dcvol_get_api(unsigned int version)
{
	if (version != DCVOL_API_VERSION)
		return NULL;

	/* idempotent, and the caller has no other way of knowing it is needed */
	dc_init_crypto();

	return &g_api;
}

/* ---------------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------------ */

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
	UNREFERENCED_PARAMETER(reserved);

	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		/*
		 * No thread notifications - nothing here is per-thread, and a DLL that
		 * asks for them slows every thread creation in the process.
		 */
		DisableThreadLibraryCalls(hModule);

		/*
		 * Installing the host is a struct copy and xts_init is a CPU feature
		 * test; neither loads a module, takes a lock or touches another DLL, so
		 * both are safe under the loader lock. Doing it here means an implicit
		 * consumer - ImBox - is ready before main rather than depending on
		 * having remembered to call dc_init_crypto.
		 */
		dc_init_crypto();
		break;
	}
	return TRUE;
}
