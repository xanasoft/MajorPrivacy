#include "framework.h"
#include "CryptoIO.h"
#include "ImBox.h"
#include "Common\helpers.h"

extern "C" {
#include "dc_header.h"
/* volume_lib, for the two key slot operations the driver performs in
   dc_change_pass. They take a header and two passwords and nothing else, so
   ImBox runs exactly the code the driver runs rather than a second copy of it. */
#include "volume_head.h"
/* is_volume_header_correct, calculate_header_crc and get_ext_header - the
   validation SetHeader performs on an edited header before writing it */
#include "header_io.h"

/*
 * secure_alloc, secure_free, burn and make_rand used to be defined right here.
 * They are dcvolume.dll's now - crypto_lib's Argon2 links to secure_alloc by
 * name, so the binary holding Argon2 has to hold that symbol too, and that is
 * the DLL. dc\include\defines.h still declares them; the loader binds them.
 */

}

static_assert(IMBOX_SLOT_NAME_LEN == SLOT_LABEL_LEN, "SSlotInfo name length must match the header format");
static_assert(SPASS_SLOT_ANY == -KEY_SLOT_MAX, "SPASS_SLOT_ANY must match the header format");
static_assert(SPASS_KDF_RAW == KDF_NONE, "SPASS_KDF_RAW must match the header format");
static_assert(SPASS_KDF_DEFAULT == KDF_DEFAULT, "SPASS_KDF_DEFAULT must match the header format");
static_assert(SPASS_KDF_ALL == KDF_ALL, "SPASS_KDF_ALL must match the header format");
static_assert(SPASS_RAW_KEY_SIZE == DISKKEY_SIZE, "SPASS_RAW_KEY_SIZE must match the header format");
static_assert(IMBOX_SLOTS_DEFAULT == KEY_SLOT_COUNT, "IMBOX_SLOTS_DEFAULT must match the header format");
static_assert(IMBOX_SLOTS_MAX == KEY_SLOT_MAX, "IMBOX_SLOTS_MAX must match the header format");

struct SCryptoIO
{
	SCryptoIO()
	{
		password = (dc_pass*)secure_alloc(sizeof(dc_pass));
	}

	~SCryptoIO()
	{
		if (password) secure_free(password);
		burn(&benc_k, sizeof(xts_key));
	}

	std::wstring Cipher;
	int SlotCount = CRYPTO_SLOTS_DEFAULT;
	bool AllowFormat = false;
	int hdr_len = DC_AREA_SIZE;

	dc_pass* password = NULL;
	xts_key benc_k;

	SSection* section;
};


/* ---------------------------------------------------------------------------
 * Shared header handling
 *
 * Everything below follows DiskCryptor's own rules, which are spelled out in
 * driver/misc_volume.c and volume_lib/header_io.c. The one that drives most of
 * the code is this: a key slot stores the header key XOR-wrapped under a key
 * the slot password derives from the *header salt*. Change the salt and every
 * slot in the header becomes garbage. So any write that must keep the slots
 * keeps the salt, and re-seals under the key the header was opened with.
 * ------------------------------------------------------------------------ */

static int st_to_err(int st)
{
	switch (st)
	{
	case ST_OK:				return ERR_OK;
	case ST_NOMEM:			return ERR_MALLOC_ERROR;
	case ST_PASS_ERR:		return ERR_WRONG_PASSWORD;
	case ST_RW_ERR:
	case ST_IO_ERROR:		return ERR_IO_FAILED;
	case ST_INCOMPATIBLE:
	case ST_BAD_INDEX:
	case ST_INVALID_PARAM:	return ERR_INVALID_PARAM;
	default:				return ERR_INTERNAL;
	}
}

//
// The length of the header on disk, plus the checks that make it safe to use
// as one. A length that is not a whole number of sectors is rejected outright:
// XTS counts its length down one sector at a time, so a partial one wraps and
// runs off the end of the buffer.
//
static int get_header_len(dc_header* header, int* out_len)
{
	int len = (header->version >= DC_HDR_VERSION_2) ? (int)header->head_len : DC_AREA_SIZE;

	if (len < DC_AREA_SIZE || len > DC_AREA_MAX_SIZE || (len % XTS_SECTOR_SIZE) != 0)
		return ERR_INTERNAL;

	if (header->version >= DC_HDR_VERSION_2 && (header->feature_flags & FF_KEY_SLOTS))
	{
		// the slot area and its descriptors both have to be inside the header
		if ((u32)DC_BASE_SIZE + header->slot_area_len + (u32)header->key_slot_count * header->slot_info_size > (u32)len)
			return ERR_INTERNAL;
	}

	*out_len = len;
	return ERR_OK;
}

//
// How much to read before anything about the header is known.
//
// A password that names a key slot needs the read to reach that slot, which
// sits past the base - dc_get_min_header_len works that out and rounds up to a
// sector. It cannot exceed the container.
//
static int get_read_len(CAbstractIO* pIO, dc_pass* password)
{
	ULONG64 uSize = pIO->GetDiskSize();
	int len = dc_get_min_header_len(password);

	if ((ULONG64)len > uSize)
		len = (int)((uSize / XTS_SECTOR_SIZE) * XTS_SECTOR_SIZE);

	return len;
}

//
// Read and decrypt the volume header.
//
// head_len lives inside the header, so a header longer than the first read
// takes two passes: open the base, learn how long it really is, read it all.
// The second pass re-uses the key the first one produced - the KDF has already
// run, and with Argon2 that is seconds we do not spend twice.
//
// On success out_header holds the complete plaintext header, with the key
// slots carried over raw - they are wrapped under their own keys, never under
// the header key, so they are never part of the encryption.
//
// password->kdf is updated to the KDF that actually opened the header, which
// everything downstream depends on: KDF_ALL and KDF_DEFAULT are search orders,
// and nothing can derive a key from them.
//
static int ReadHeader(CAbstractIO* pIO, dc_pass* password, dc_header** out_header,
                      xts_key** out_key, int* out_head_len, int* out_kdf, int* out_slot)
{
	dc_header* header = NULL;
	u8* enc_header = NULL;
	xts_key* hdr_key = NULL;
	int kdf = -1;
	int slot = 0;
	int head_len = 0;
	int enc_len = get_read_len(pIO, password);
	int ret;

	for (;;)
	{
		if ((enc_header = (u8*)secure_alloc(enc_len)) == NULL) {
			DbgPrint(L"Malloc Failed\n");
			ret = ERR_MALLOC_ERROR;
			goto finish;
		}

		if (!pIO->DiskRead(enc_header, enc_len, 0)) {
			DbgPrint(L"DiskRead Failed\n");
			ret = ERR_IO_FAILED;
			goto finish;
		}

		if (hdr_key == NULL)
		{
			// the whole search - every KDF the password admits, every key slot
			// it names, every cipher - happens in here
			ret = dc_decrypt_header(enc_header, enc_len, password, &header, &hdr_key, NULL, &kdf, &slot) == ST_OK ? ERR_OK : ERR_WRONG_PASSWORD;
			if (ret != ERR_OK) {
				DbgPrint(L"Wrong Password\n");
				goto finish;
			}

			password->kdf = kdf;
		}
		else
		{
			if ((header = (dc_header*)secure_alloc(enc_len)) == NULL) {
				DbgPrint(L"Malloc Failed\n");
				ret = ERR_MALLOC_ERROR;
				goto finish;
			}

			if (!cp_decrypt_header_with_key(enc_header, enc_len, hdr_key, header)) {
				DbgPrint(L"Failed to decrypt the full header\n");
				ret = ERR_INTERNAL;
				goto finish;
			}
		}

		if ((ret = get_header_len(header, &head_len)) != ERR_OK) {
			DbgPrint(L"Invalid header length\n");
			goto finish;
		}

		if (head_len <= enc_len)
			break;

		if ((ULONG64)head_len > pIO->GetDiskSize()) {
			DbgPrint(L"Header does not fit into the container\n");
			ret = ERR_INTERNAL;
			goto finish;
		}

		// go round once more for the rest of it - the key slots and anything
		// else past the base are only present on the second pass
		secure_free(header);
		header = NULL;
		secure_free(enc_header);
		enc_header = NULL;
		enc_len = head_len;
	}

	*out_header = header;
	header = NULL;
	if (out_key != NULL) { *out_key = hdr_key; hdr_key = NULL; }
	if (out_head_len != NULL) *out_head_len = head_len;
	if (out_kdf != NULL) *out_kdf = kdf;
	if (out_slot != NULL) *out_slot = slot;
	ret = ERR_OK;

finish:
	if (header) secure_free(header);
	if (enc_header) secure_free(enc_header);
	if (hdr_key) secure_free(hdr_key);
	return ret;
}

//
// A header backup file is ciphertext, so it is read and written through the
// ordinary cached path. FILE_FLAG_NO_BUFFERING would demand a sector-aligned
// buffer, which secure_alloc cannot give - it hands out its block sixteen
// bytes into the page.
//
static int WriteHeaderFile(const std::wstring& Path, const void* data, int size)
{
	DWORD BytesWritten;
	int ret = ERR_OK;

	// CREATE_ALWAYS, not OPEN_ALWAYS: an existing backup of a longer header
	// would otherwise keep its tail behind the one written here
	HANDLE hFile = CreateFile(Path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return ERR_FILE_NOT_OPENED;

	if (!WriteFile(hFile, data, size, &BytesWritten, NULL) || (int)BytesWritten != size)
		ret = ERR_FILE_NOT_OPENED;

	CloseHandle(hFile);
	return ret;
}

static int ReadHeaderFile(const std::wstring& Path, u8** out_data, int* out_size)
{
	LARGE_INTEGER fileSize;
	DWORD BytesRead;
	u8* data = NULL;
	int size;
	int ret = ERR_OK;

	HANDLE hFile = CreateFile(Path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return ERR_FILE_NOT_OPENED;

	if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart < DC_AREA_SIZE ||
		fileSize.QuadPart > DC_AREA_MAX_SIZE || (fileSize.QuadPart % XTS_SECTOR_SIZE) != 0) {
		DbgPrint(L"Invalid header backup file\n");
		CloseHandle(hFile);
		return ERR_FILE_NOT_OPENED;
	}

	size = (int)fileSize.QuadPart;
	if ((data = (u8*)secure_alloc(size)) == NULL) {
		DbgPrint(L"Malloc Failed\n");
		CloseHandle(hFile);
		return ERR_MALLOC_ERROR;
	}

	if (!ReadFile(hFile, data, size, &BytesRead, NULL) || (int)BytesRead != size) {
		secure_free(data);
		CloseHandle(hFile);
		return ERR_FILE_NOT_OPENED;
	}

	CloseHandle(hFile);
	*out_data = data;
	*out_size = size;
	return ret;
}


/* ---------------------------------------------------------------------------
 * CCryptoIO
 * ------------------------------------------------------------------------ */

CCryptoIO::CCryptoIO(CAbstractIO* pIO, const SPassword* pass, const std::wstring& Cipher, int SlotCount)
{
	m = new SCryptoIO;
	m->Cipher = Cipher;
	m->SlotCount = SlotCount;
	m->AllowFormat = false;

	if (m->password) memcpy(m->password, pass, sizeof(SPassword));

	m->section = NULL;

	m_pIO = pIO;
}

CCryptoIO::~CCryptoIO()
{
	delete m;
}

ULONG64 CCryptoIO::GetDiskSize() const
{
	ULONG64 uSize = m_pIO->GetDiskSize();
	if (uSize < m->hdr_len)
		return 0;
	return uSize - m->hdr_len;
}

bool CCryptoIO::CanBeFormated() const
{
	return m->AllowFormat;
}

//
// Lay out a fresh v2 header, key slots included.
//
// This is init_header_v2 from driver/misc_volume.c: the slot area is sized and
// filled with random bytes, and the descriptors are left zeroed, which is what
// marks every slot inactive. No slot is in use yet, so this is the one write
// where the header gets a salt of its own - see WriteHeader.
//
int CCryptoIO::CreateHeader()
{
	dc_header* header = NULL;
	int slot_size;
	int slot_count;
	int cipher;
	int ret;

	m->hdr_len = 64*1024; // 64 KiB header is default

	DbgPrint(L"Creating DCv2 header, size: %u\n", m->hdr_len);

	if (m->Cipher.empty() || _wcsicmp(m->Cipher.c_str(), L"AES") == 0 || _wcsicmp(m->Cipher.c_str(), L"Rijndael") == 0)
		cipher = CF_AES;
	else if (_wcsicmp(m->Cipher.c_str(), L"TWOFISH") == 0)
		cipher = CF_TWOFISH;
	else if (_wcsicmp(m->Cipher.c_str(), L"SERPENT") == 0)
		cipher = CF_SERPENT;
	else if (_wcsicmp(m->Cipher.c_str(), L"AES-TWOFISH") == 0)
		cipher = CF_AES_TWOFISH;
	else if (_wcsicmp(m->Cipher.c_str(), L"TWOFISH-SERPENT") == 0)
		cipher = CF_TWOFISH_SERPENT;
	else if (_wcsicmp(m->Cipher.c_str(), L"SERPENT-AES") == 0)
		cipher = CF_SERPENT_AES;
	else if (_wcsicmp(m->Cipher.c_str(), L"AES-TWOFISH-SERPENT") == 0)
		cipher = CF_AES_TWOFISH_SERPENT;
	else {
		DbgPrint(L"Unknown Cipher\n");
		return ERR_UNKNOWN_CIPHER;
	}

	// KDF_ALL and KDF_DEFAULT say which KDFs to try when opening a container.
	// Creating one has nothing to search, so a selector has to become a choice.
	if (m->password->kdf < 0) {
		if (m->password->kdf != KDF_DEFAULT) {
			DbgPrint(L"Invalid KDF %d\n", m->password->kdf);
			return ERR_INVALID_PARAM;
		}
		m->password->kdf = KDF_ARGON_DEFAULT;
	}
	else if (m->password->kdf > KDF_ARGON_MAX) {
		DbgPrint(L"Invalid KDF %d\n", m->password->kdf);
		return ERR_INVALID_PARAM;
	}

	slot_count = (m->SlotCount == CRYPTO_SLOTS_DEFAULT) ? KEY_SLOT_COUNT : m->SlotCount;
	slot_size = cp_get_key_slot_size(DC_SLOT_TYPE_0);
	if (slot_count < 0 || slot_count > KEY_SLOT_MAX) {
		DbgPrint(L"Invalid key slot count %d\n", slot_count);
		return ERR_INVALID_PARAM;
	}
	if (DC_BASE_SIZE + slot_count * (slot_size + (int)sizeof(dc_slot_info)) > m->hdr_len) {
		DbgPrint(L"Header is too small for %d key slots\n", slot_count);
		return ERR_INVALID_PARAM;
	}

	if ((header = (dc_header*)secure_alloc(m->hdr_len)) == NULL) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}

	memset((BYTE*)header, 0, m->hdr_len);

	make_rand(&header->disk_id, sizeof(header->disk_id));
	make_rand(header->key_1, sizeof(header->key_1));

	header->sign     = DC_VOLUME_SIGN;
	header->version  = DC_HDR_VERSION_2;
	header->flags    = VF_NO_REDIR; // no redirection
	header->alg_1    = cipher;
	header->head_kdf = (u8)m->password->kdf;
	header->data_off = m->hdr_len;
	header->head_len = m->hdr_len;

	if (slot_count > 0)
	{
		header->feature_flags |= FF_KEY_SLOTS;
		header->key_slot_count = (u8)slot_count;
		header->slot_area_len  = (u16)(slot_count * slot_size);
		header->slot_info_size = sizeof(dc_slot_info);

		// an unused slot has to be indistinguishable from one in use, so the
		// area is filled with random bytes rather than left zeroed. The
		// descriptors stay zero: no SF_ACTIVE, no slot.
		make_rand(((u8*)header) + DC_BASE_SIZE, header->slot_area_len);
	}

	// dc_encrypt_header refreshes hdr_crc, so it is not computed here
	ret = WriteHeader(header, NULL);

	//BYTE buffer[512];
	//memset(buffer, 0, sizeof(buffer));
	//m_pIO->DiskWrite(buffer, sizeof(buffer), m_pIO->GetDiskSize() - sizeof(buffer));

	secure_free(header);
	return ret;
}

int CCryptoIO::InitCrypto()
{
	dc_header* header = NULL;
	int head_len = 0;
	int kdf = -1;
	int slot = 0;
	int ret;

	if (!m->password)
		return ERR_KEY_REQUIRED;

	m->AllowFormat = m_pIO->CanBeFormated();
	if (m->AllowFormat) {
		if ((ret = CreateHeader()) != ERR_OK)
			return ret;
	}

	DbgPrint(L"Trying to decrypt header...\n");

	ret = ReadHeader(m_pIO, m->password, &header, NULL, &head_len, &kdf, &slot);
	if (ret != ERR_OK) {
		// a container written a moment ago has to open again; if it does not,
		// the fault is here rather than in the password
		if (m->AllowFormat && ret == ERR_WRONG_PASSWORD)
			ret = ERR_INTERNAL;
		goto finish;
	}

	if ((ULONG64)head_len >= m_pIO->GetDiskSize()) {
		DbgPrint(L"Invalid header length\n");
		ret = ERR_INTERNAL;
		goto finish;
	}

	xts_set_key(header->key_1, header->alg_1, &m->benc_k);

	m->hdr_len = head_len;

	if (slot != 0)
		DbgPrint(L"Opened with key slot %d\n", slot);

	ReportVolumeInfo(header, head_len, kdf, slot);

	DbgPrint(L"Success\n");

finish:
	if (header) secure_free(header);
	return ret;
}

//
// What the caller is told about a volume it just opened. Shared by the mount
// path and by GetHeader, which reports the same set.
//
static void FillCryptInfo(SCryptInfo* pInfo, dc_header* header, int head_len, int kdf, int slot)
{
	pInfo->cipher_id  = (unsigned char)header->alg_1;
	pInfo->head_kdf   = kdf;
	pInfo->version    = header->version;
	pInfo->head_len   = head_len;
	pInfo->slot_count = cp_has_key_slots(header) ? header->key_slot_count : 0;
	pInfo->disk_id    = header->disk_id;
	pInfo->used_slot  = slot;
}

void CCryptoIO::ReportVolumeInfo(dc_header* header, int head_len, int kdf, int slot)
{
	if (!m->section)
		return;

	FillCryptInfo(&m->section->out.info, header, head_len, kdf, slot);

	//if (m->section && header->info_magic == DC_INFO_MAGIC) {
	//	m->section->id = SECTION_PARAM_ID_DATA;
	//	m->section->size = header->info_size;
	//	memcpy(m->section->data, header->info_data, header->info_size);
	//}

	if (!cp_has_key_slots(header))
		return;

	// one descriptor per slot. cp_get_slot_info does the CRC check, so a slot
	// whose ciphertext and descriptor disagree is reported SF_CORRUPT instead
	// of passing for an active one.
	const int capacity = (int)((sizeof(m->section->_3k) - 2 * sizeof(USHORT)) / sizeof(SSlotInfo));
	int count = min((int)header->key_slot_count, capacity);

	SSlotInfo* pSlots = (SSlotInfo*)m->section->data;
	memset(pSlots, 0, count * sizeof(SSlotInfo));

	for (int i = 0; i < count; i++)
	{
		dc_slot_info info;
		if (cp_get_slot_info(header, i, &info) != ST_OK)
			continue;

		pSlots[i].flags = info.flags;
		pSlots[i].type  = info.type;
		pSlots[i].kdf   = info.data_0.slot_kdf;
		memcpy(pSlots[i].name, info.slot_name, SLOT_LABEL_LEN);
	}

	m->section->id = SECTION_PARAM_ID_SLOTS;
	m->section->size = (USHORT)(count * sizeof(SSlotInfo));
}

int CCryptoIO::Init()
{
	int ret = m_pIO ? m_pIO->Init() : ERR_UNKNOWN_TYPE;

	if (ret == ERR_OK)
		ret = InitCrypto();

	secure_free(m->password);
	m->password = NULL;

	return ret;
}

int CCryptoIO::WriteHeader(dc_header* header, xts_key* hdr_key)
{
	int ret = ERR_INTERNAL;
	xts_key own_key;
	xts_key* key = hdr_key;
	u8 *enc_header = NULL;

	if (key == NULL)
	{
		// a key has to be derived, and an empty password is not one -
		// io_write_header refuses the same case for the same reason
		if (!m->password || m->password->size == 0)
			return ERR_KEY_REQUIRED;

		//
		// No key was handed in, so the header gets a fresh salt and a key
		// derived from the password over it. This is only ever correct when
		// nothing in the header is bound to the old salt - that is, when there
		// are no active key slots to orphan.
		//
		make_rand(header->salt, HEADER_SALT_SIZE);

		if (!cp_set_header_key(&own_key, header->salt, header->alg_1, m->password, NULL)) {
			burn(&own_key, sizeof(own_key));
			return ERR_INTERNAL;
		}
		key = &own_key;
	}

	// leaves the salt in the clear, carries the key slots over raw and
	// refreshes hdr_crc before sealing
	if (dc_encrypt_header(header, m->hdr_len, key, &enc_header) == ST_OK)
	{
		if (m_pIO->DiskWrite(enc_header, m->hdr_len, 0))
			ret = ERR_OK;
		else
			ret = ERR_IO_FAILED;

		secure_free(enc_header);
	}

	burn(&own_key, sizeof(own_key));
	return ret;
}

int CCryptoIO::ChangePassword(const SPassword* new_pass)
{
	dc_header* header = NULL;
	xts_key* hdr_key = NULL;		// the key the container was opened with
	xts_key* new_key = NULL;		// the key to seal with, NULL asks for a new salt
	dc_pass* new_password = NULL;
	int head_len = 0;
	int slot = 0;
	int ret = m_pIO ? m_pIO->Init() : ERR_UNKNOWN_TYPE;
	if (ret != ERR_OK)
		return ret;

	if (!m->password)
		return ERR_KEY_REQUIRED;

	// an empty password is only meaningful for a slot, where it means erase
	if (new_pass->size == 0 && new_pass->slot == 0) {
		DbgPrint(L"An empty header password is not allowed\n");
		return ERR_INVALID_PARAM;
	}

	// the new password is derived, never searched for, so it must name a KDF
	if (new_pass->kdf < 0 || new_pass->kdf > KDF_ARGON_MAX) {
		DbgPrint(L"Invalid KDF %d\n", new_pass->kdf);
		return ERR_INVALID_PARAM;
	}

	ret = ReadHeader(m_pIO, m->password, &header, &hdr_key, &head_len, NULL, &slot);
	if (ret != ERR_OK)
		return ret;

	m->hdr_len = head_len;

	// the library reads more of a dc_pass than SPassword carries
	if ((new_password = (dc_pass*)secure_alloc(sizeof(dc_pass))) == NULL) {
		DbgPrint(L"Malloc Failed\n");
		ret = ERR_MALLOC_ERROR;
		goto finish;
	}
	memcpy(new_password, new_pass, sizeof(SPassword));

	//
	// Both operations below wrap or re-wrap the header key with a key derived
	// from the *header password*. Opened through a key slot we hold the header
	// key but not that password, and one cannot be turned back into the other -
	// so, exactly as dc_change_pass does, this is refused.
	//
	if (slot != 0) {
		DbgPrint(L"Key slots can only be changed with the header password\n");
		ret = ERR_INVALID_PARAM;
		goto finish;
	}

	if (new_password->slot != 0)
	{
		//
		// Set or erase a single key slot.
		//
		// The header key itself does not change, so the salt, the other slots
		// and the volume data are all untouched and the header goes back under
		// the key it was opened with.
		//
		if (!cp_has_key_slots(header)) {
			DbgPrint(L"Volume has no key slots\n");
			ret = ERR_INVALID_PARAM;
			goto finish;
		}

		if (new_password->slot < 1 || new_password->slot > header->key_slot_count) {
			DbgPrint(L"Invalid key slot %d, the volume has %d\n", new_password->slot, header->key_slot_count);
			ret = ERR_INVALID_PARAM;
			goto finish;
		}

		if (new_password->size == 0)
		{
			// Erase: the descriptor is zeroed and the slot filled with random
			// bytes. Zeroing it instead would announce which slots are unused.
			DbgPrint(L"Erasing key slot %d\n", new_password->slot);

			ret = st_to_err(cp_set_slot(header, new_password->slot - 1, CP_SLOT_CLEAR, CP_SLOT_RANDOM, 0));
		}
		else
		{
			// Wraps the header key under the new slot password and refreshes
			// the slot CRC. Both derivations use the header salt.
			DbgPrint(L"Setting the password of key slot %d\n", new_password->slot);

			ret = st_to_err(dc_change_slot_pass(header, m->password, new_password, NULL));
		}

		if (ret != ERR_OK) {
			DbgPrint(L"Failed to update key slot %d\n", new_password->slot);
			goto finish;
		}

		new_key = hdr_key;
	}
	else
	{
		//
		// Change the header password.
		//
		if (cp_has_key_slots(header))
		{
			//
			// Every active slot holds the header key wrapped under its own
			// password, and both sides of that wrapping derive from the header
			// salt. Re-wrapping them onto the new header key is therefore only
			// possible over the same salt - which is why the salt is kept
			// whenever key slots are in use, and why every slot password keeps
			// working afterwards without its holder being present.
			//
			// dc_update_key_slots leaves new_key NULL when no slot turned out
			// to be active. Then there is nothing bound to the salt, and the
			// header is better off with a fresh one.
			//
			ret = st_to_err(dc_update_key_slots(header, &new_key, m->password, new_password, NULL));
			if (ret != ERR_OK) {
				DbgPrint(L"Failed to re-wrap the key slots\n");
				goto finish;
			}

			if (new_key == NULL)
				DbgPrint(L"No key slot in use, re-sealing with a new salt\n");
		}

		if (header->version >= DC_HDR_VERSION_2)
			header->head_kdf = (u8)new_password->kdf;

		// WriteHeader derives from m->password when it has to make a new salt
		memcpy(m->password, new_password, sizeof(dc_pass));
	}

	ret = WriteHeader(header, new_key);

finish:
	if (new_key != NULL && new_key != hdr_key) secure_free(new_key);
	if (hdr_key) secure_free(hdr_key);
	if (new_password) secure_free(new_password);
	if (header) secure_free(header);
	return ret;
}

bool CCryptoIO::DiskWrite(void* buf, int size, __int64 offset)
{
#ifdef _DEBUG
	if ((offset & 0x1FF) || (size & 0x1FF))
		DbgPrint(L"DiskWrite not full sector\n");
#endif

	xts_encrypt((BYTE*)buf, (BYTE*)buf, size, offset, &m->benc_k);

	bool ret = m_pIO->DiskWrite(buf, size, offset + m->hdr_len);

	//xts_decrypt((BYTE*)buf, (BYTE*)buf, size, offset, &m->benc_k); // restore buffer - not needed

	return ret;
}

bool CCryptoIO::DiskRead(void* buf, int size, __int64 offset)
{
#ifdef _DEBUG
	if ((offset & 0x1FF) || (size & 0x1FF))
		DbgPrint(L"DiskRead not full sector\n");
#endif

	bool ret = m_pIO->DiskRead(buf, size, offset + m->hdr_len);

	if (ret)
		xts_decrypt((BYTE*)buf, (BYTE*)buf, size, offset, &m->benc_k);

	return ret;
}

void CCryptoIO::TrimProcess(DEVICE_DATA_SET_RANGE* range, int n)
{
	for (DEVICE_DATA_SET_RANGE* range2 = range; range2 < range + n; range2++) {
		range2->StartingOffset += m->hdr_len;
#ifdef _DEBUG
		if (range2->StartingOffset & 0x1FF || range2->LengthInBytes & 0x1FF)
			DbgPrint(L"TrimProcess not full sector\n");
#endif
	}

	m_pIO->TrimProcess(range, n);
}

int CCryptoIO::BackupHeader(CAbstractIO* pIO, const std::wstring& Path, const SPassword* pass)
{
	dc_header* header = NULL;
	xts_key* hdr_key = NULL;
	xts_key* bak_key = NULL;
	u8* sealed = NULL;
	dc_pass* password = NULL;
	int head_len = 0;
	int slot = 0;
	int ret = pIO->Init();
	if (ret != ERR_OK)
		return ret;

	if ((password = (dc_pass*)secure_alloc(sizeof(dc_pass))) == NULL) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}
	memcpy(password, pass, sizeof(SPassword));

	ret = ReadHeader(pIO, password, &header, &hdr_key, &head_len, NULL, &slot);
	if (ret != ERR_OK)
		goto finish;

	if (cp_has_key_slots(header))
	{
		//
		// Keep the salt, and with it the key.
		//
		// The slot ciphertext is copied into the backup as it stands, and each
		// slot password unwraps it with a key derived from the header salt. A
		// backup under a new salt would carry slots nothing could open.
		//
		DbgPrint(L"Backing up a header with %d key slots, keeping the salt\n", header->key_slot_count);

		bak_key = hdr_key;
		hdr_key = NULL;
	}
	else
	{
		//
		// Nothing here is bound to the salt, so the backup gets one of its own
		// and a key derived from the password over it. The two copies of the
		// same header then share no ciphertext at all.
		//
		if (slot != 0) { // unreachable: without key slots there is only the header password
			ret = ERR_INVALID_PARAM;
			goto finish;
		}

		DbgPrint(L"Backing up a header without key slots, re-salting\n");

		if ((bak_key = (xts_key*)secure_alloc(sizeof(xts_key))) == NULL) {
			DbgPrint(L"Malloc Failed\n");
			ret = ERR_MALLOC_ERROR;
			goto finish;
		}

		make_rand(header->salt, HEADER_SALT_SIZE);

		if (!cp_set_header_key(bak_key, header->salt, header->alg_1, password, NULL)) {
			ret = ERR_INTERNAL;
			goto finish;
		}
	}

	// not into a buffer the caller still needs: the salt stays in the clear and
	// the key slots are carried over raw, both inside dc_encrypt_header
	if (dc_encrypt_header(header, head_len, bak_key, &sealed) != ST_OK) {
		ret = ERR_INTERNAL;
		goto finish;
	}

	ret = WriteHeaderFile(Path, sealed, head_len);

finish:
	if (sealed) secure_free(sealed);
	if (password) secure_free(password);
	if (header) secure_free(header);
	if (hdr_key) secure_free(hdr_key);
	if (bak_key) secure_free(bak_key);
	return ret;
}

int CCryptoIO::RestoreHeader(CAbstractIO* pIO, const std::wstring& Path, const SPassword* pass)
{
	dc_header* header = NULL;
	u8* enc_header = NULL;
	u8* sealed = NULL;
	xts_key* bak_key = NULL;	// the key the backup file was sealed with
	xts_key* hdr_key = NULL;	// the key the container is written with
	xts_key own_key;
	dc_pass* password = NULL;
	int enc_len = 0;
	int head_len = 0;
	int kdf = -1;
	int slot = 0;
	int ret = pIO->Init();
	if (ret != ERR_OK)
		return ret;

	burn(&own_key, sizeof(own_key));

	if ((password = (dc_pass*)secure_alloc(sizeof(dc_pass))) == NULL) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}
	memcpy(password, pass, sizeof(SPassword));

	if ((ret = ReadHeaderFile(Path, &enc_header, &enc_len)) != ERR_OK)
		goto finish;

	// carries the key slots over from the blob as well, so long as the blob is
	// long enough to hold them - which is checked against head_len below
	ret = dc_decrypt_header(enc_header, enc_len, password, &header, &bak_key, NULL, &kdf, &slot) == ST_OK ? ERR_OK : ERR_WRONG_PASSWORD;
	if (ret != ERR_OK) {
		DbgPrint(L"Wrong Password\n");
		goto finish;
	}
	password->kdf = kdf;

	if ((ret = get_header_len(header, &head_len)) != ERR_OK) {
		DbgPrint(L"Invalid header length\n");
		goto finish;
	}

	if (head_len > enc_len) {
		DbgPrint(L"Header backup is truncated\n");
		ret = ERR_INTERNAL;
		goto finish;
	}

	if ((ULONG64)head_len >= pIO->GetDiskSize()) {
		DbgPrint(L"Header does not fit into the container\n");
		ret = ERR_INTERNAL;
		goto finish;
	}

	if (cp_has_key_slots(header))
	{
		// the slot ciphertext just recovered is wrapped against the backup's
		// salt, so both go onto the container together or every slot password
		// is lost
		DbgPrint(L"Restoring a header with %d key slots, keeping the salt\n", header->key_slot_count);

		hdr_key = bak_key;
	}
	else
	{
		if (slot != 0) { // unreachable: without key slots there is only the header password
			ret = ERR_INVALID_PARAM;
			goto finish;
		}

		DbgPrint(L"Restoring a header without key slots, re-salting\n");

		make_rand(header->salt, HEADER_SALT_SIZE);

		if (!cp_set_header_key(&own_key, header->salt, header->alg_1, password, NULL)) {
			ret = ERR_INTERNAL;
			goto finish;
		}
		hdr_key = &own_key;
	}

	if (dc_encrypt_header(header, head_len, hdr_key, &sealed) != ST_OK) {
		ret = ERR_INTERNAL;
		goto finish;
	}

	ret = pIO->DiskWrite(sealed, head_len, 0) ? ERR_OK : ERR_IO_FAILED;

finish:
	burn(&own_key, sizeof(own_key));
	if (sealed) secure_free(sealed);
	if (password) secure_free(password);
	if (header) secure_free(header);
	if (enc_header) secure_free(enc_header);
	if (bak_key) secure_free(bak_key);
	return ret;
}

//
// Put the decrypted volume header into the shared section.
//
// The plaintext header carries key_1, the volume's master key. Handing that to
// the caller is the point rather than an oversight: a key slot holds the header
// key wrapped under a slot password, so an editor that cannot see the key
// cannot add a slot - and whoever supplied a working password could have
// recovered all of it anyway. What matters is where it lands, and that is a
// section the caller created page-locked and wipes when it is done.
//
int CCryptoIO::GetHeader(CAbstractIO* pIO, SSection* pSection, ULONG uSecSize, const SPassword* pass)
{
	dc_header* header = NULL;
	dc_pass* password = NULL;
	SHeaderData* pData;
	int head_len = 0;
	int kdf = -1;
	int slot = 0;
	int ret;

	if (pSection == NULL || uSecSize < sizeof(SSection) + sizeof(SHeaderData)) {
		DbgPrint(L"No section for the header\n");
		return ERR_INVALID_PARAM;
	}

	if ((ret = pIO->Init()) != ERR_OK)
		return ret;

	if ((password = (dc_pass*)secure_alloc(sizeof(dc_pass))) == NULL) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}
	memcpy(password, pass, sizeof(SPassword));

	ret = ReadHeader(pIO, password, &header, NULL, &head_len, &kdf, &slot);
	if (ret != ERR_OK)
		goto finish;

	//
	// Reported before the section is measured, not after.
	//
	// A caller cannot know head_len until the header is open, so it has to
	// guess at the section size - and the only way out of that is for a section
	// which turned out too small to still come back saying how big it should
	// have been. out.info and SHeaderData.head_len are filled in either way;
	// only the header itself is withheld.
	//
	// out.info shares storage with in.pw, so the caller's password has already
	// been copied out and burned by the time this runs.
	//
	pData = SECTION_HEADER_DATA(pSection);
	pData->size     = 0;
	pData->head_len = (ULONG)head_len;
	pData->flags    = 0;
	pData->reserved = 0;

	FillCryptInfo(&pSection->out.info, header, head_len, kdf, slot);

	if (uSecSize < SECTION_SIZE_FOR_HEADER((ULONG)head_len)) {
		DbgPrint(L"Section holds %u bytes, the header needs %d\n", uSecSize, head_len);
		ret = ERR_BUFFER_TO_SMALL;
		goto finish;
	}

	memcpy(pData->data, header, head_len);
	pData->size = (ULONG)head_len;

	pSection->id = SECTION_PARAM_ID_HEADER;
	pSection->size = 0;

	ret = ERR_OK;

finish:
	if (password) secure_free(password);
	if (header) secure_free(header);
	return ret;
}

//
// Take an edited header back and write it.
//
// This is dc_update_header from driver\misc_volume.c, without the parts that
// only mean something for a mounted volume. The shape of it is the same and so
// are the rules, because they are what keeps an editor from turning a container
// into an unreadable one:
//
//   - the update arrives sealed under the volume's *current* header key, and
//     that is the whole authentication. Nothing but that key produces a blob
//     which comes out of the decrypt as a valid header;
//   - the salt on the volume wins. The update is written under the same key it
//     was sealed with, so every key slot keeps working;
//   - the header length may not change. Moving it would move the volume data;
//   - the cipher and the master key may not change. Either one would make every
//     byte already written unreadable;
//   - only named fields are taken from the update, chosen by HF_UPDATE_*.
//     Everything else on the volume is left exactly as it was.
//
int CCryptoIO::SetHeader(CAbstractIO* pIO, SSection* pSection, ULONG uSecSize, const SPassword* pass)
{
	dc_header* header = NULL;		// what is on the volume now
	dc_header* upd = NULL;			// what the editor wants there
	dc_ext_header* ext_hdr = NULL;
	xts_key* hdr_key = NULL;
	dc_pass* password = NULL;
	SHeaderData* pData;
	u8* blob;
	u32 flags;
	u32 offset;
	u32 old_slots_len = 0;
	int head_len = 0;
	int slot = 0;
	int ret;

	if (pSection == NULL || uSecSize < sizeof(SSection) + sizeof(SHeaderData)) {
		DbgPrint(L"No section for the header\n");
		return ERR_INVALID_PARAM;
	}

	pData = SECTION_HEADER_DATA(pSection);
	blob = pData->data;
	flags = pData->flags;

	// the driver's flag clean-up: a partial update that names nothing is a full
	// one, and clearing the slots means writing them
	flags &= (HF_UPDATE_ALL | HF_CLEAR_SLOTS);
	if (flags & HF_CLEAR_SLOTS)
		flags |= HF_UPDATE_SLOTS;
	if ((flags & HF_UPDATE_ALL) == 0)
		flags |= HF_UPDATE_ALL;

	if ((ret = pIO->Init()) != ERR_OK)
		return ret;

	if ((password = (dc_pass*)secure_alloc(sizeof(dc_pass))) == NULL) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}
	memcpy(password, pass, sizeof(SPassword));

	ret = ReadHeader(pIO, password, &header, &hdr_key, &head_len, NULL, &slot);
	if (ret != ERR_OK)
		goto finish;

	//
	// An update reaches every key slot in the header, and re-wrapping one needs
	// the key the header password derives to. Opened through a slot we do not
	// have it - the same reason dc_update_header refuses password->slot != 0.
	//
	if (slot != 0) {
		DbgPrint(L"The header can only be updated with the header password\n");
		ret = ERR_INVALID_PARAM;
		goto finish;
	}

	if (pData->size < (ULONG)head_len || uSecSize < SECTION_SIZE_FOR_HEADER((ULONG)head_len)) {
		DbgPrint(L"Update is %u bytes, the header is %d\n", pData->size, head_len);
		ret = ERR_INVALID_PARAM;
		goto finish;
	}

	if ((upd = (dc_header*)secure_alloc(head_len)) == NULL) {
		DbgPrint(L"Malloc Failed\n");
		ret = ERR_MALLOC_ERROR;
		goto finish;
	}

	//
	// Unseal the base with the volume's own key. The salt was never encrypted,
	// and the one that counts is the one already on the volume, so it is put
	// back before the header is checked - which is also what makes a blob sealed
	// against some other salt fail here rather than later.
	//
	memcpy(upd, blob, DC_AREA_SIZE);
	xts_decrypt((u8*)upd, (u8*)upd, DC_AREA_SIZE, 0, hdr_key);
	memcpy(upd->salt, header->salt, HEADER_SALT_SIZE);

	if (!is_volume_header_correct(upd)) {
		DbgPrint(L"Update is not a valid header, or was sealed with another key\n");
		ret = ERR_INTERNAL;
		goto finish;
	}

	// changing the header length moves the volume data; that is a different
	// operation and this is not it
	if (head_len != ((upd->version >= DC_HDR_VERSION_2) ? (int)upd->head_len : DC_AREA_SIZE)) {
		DbgPrint(L"Update changes the header length\n");
		ret = ERR_INVALID_PARAM;
		goto finish;
	}

	// neither of these may move, whatever else the update says
	if (upd->alg_1 != header->alg_1 || memcmp(upd->key_1, header->key_1, DISKKEY_SIZE) != 0) {
		DbgPrint(L"Update changes the cipher or the master key\n");
		ret = ERR_INVALID_PARAM;
		goto finish;
	}
	if (upd->tmp_size != header->tmp_size) {
		DbgPrint(L"Update changes the re-encryption position\n");
		ret = ERR_INVALID_PARAM;
		goto finish;
	}

	//
	// Switching the header version replaces the whole thing rather than merging
	// fields, because the layout itself differs.
	//
	if (upd->version != header->version)
	{
		//
		// A v1 header has no head_len: the data offset is fixed at DC_AREA_SIZE.
		// Downgrading a container whose data starts further in would leave every
		// byte of it at the wrong offset, so it is only allowed where the two
		// already agree. DiskCryptor's editor greys the choice out on the same
		// test.
		//
		if (upd->version < DC_HDR_VERSION_2 && head_len != DC_AREA_SIZE) {
			DbgPrint(L"Cannot downgrade a header longer than %d bytes\n", DC_AREA_SIZE);
			ret = ERR_INVALID_PARAM;
			goto finish;
		}

		if (upd->version >= DC_HDR_VERSION_2) {
			upd->head_len = head_len;
			upd->stor_len = head_len;
		} else {
			upd->head_len = 0;
			upd->stor_len = 0;
		}

		DbgPrint(L"Header version %d -> %d\n", header->version, upd->version);

		upd->hdr_crc = calculate_header_crc(upd);
		memcpy(header, upd, head_len);
		goto write;
	}

	//
	// v2 - take the named fields and leave the rest alone.
	//
	if (upd->version >= DC_HDR_VERSION_2)
	{
		// the tail, which the base decrypt above did not reach
		if (head_len > DC_AREA_SIZE)
			xts_decrypt(blob + DC_AREA_SIZE, ((u8*)upd) + DC_AREA_SIZE, head_len - DC_AREA_SIZE, DC_AREA_SIZE, hdr_key);

		// key slots are wrapped under their own keys, never under the header
		// key, so they come across raw
		if (upd->feature_flags & FF_KEY_SLOTS)
			cp_copy_keylots(upd, blob, (u8*)upd);

		// the slot area and its descriptors have to fit inside the header
		offset = DC_BASE_SIZE;
		if (upd->feature_flags & FF_KEY_SLOTS)
			offset += upd->slot_area_len + (u32)upd->key_slot_count * upd->slot_info_size;
		if (offset > (u32)head_len) {
			DbgPrint(L"Update's key slot area does not fit the header\n");
			ret = ERR_INVALID_PARAM;
			goto finish;
		}

		if (upd->ext_hdr_off != 0 && get_ext_header(upd, &ext_hdr) != ST_OK) {
			DbgPrint(L"Update's extended header is invalid\n");
			ret = ERR_INVALID_PARAM;
			goto finish;
		}

		if (flags & HF_UPDATE_BASE)
		{
			header->feature_flags = upd->feature_flags;

			if (header->slot_area_len != upd->slot_area_len ||
				header->key_slot_count != upd->key_slot_count ||
				header->slot_info_size != upd->slot_info_size)
			{
				//
				// The slot layout is changing. Everything the old layout
				// occupied is overwritten with random bytes first, so that a
				// shrink cannot leave old wrapped keys or old descriptors
				// behind it - the driver randomises only the payload area,
				// which is enough for it because it writes a partial range;
				// this rewrites the whole header, so the descriptors would be
				// republished as they stood.
				//
				old_slots_len = header->slot_area_len + (u32)header->key_slot_count * header->slot_info_size;
				if ((u32)DC_BASE_SIZE + old_slots_len <= (u32)head_len)
					make_rand(((u8*)header) + DC_BASE_SIZE, old_slots_len);

				header->slot_area_len  = upd->slot_area_len;
				header->key_slot_count = upd->key_slot_count;
				header->slot_info_size = upd->slot_info_size;

				// the new area has to be written, or the random just laid down
				// would be all that is there
				flags |= HF_UPDATE_SLOTS;
			}

			header->ext_hdr_off = upd->ext_hdr_off;
			if (ext_hdr != NULL)
				flags |= HF_UPDATE_EXT;

			header->flags = upd->flags;
			header->disk_id = upd->disk_id;
			header->footer_cnt = upd->footer_cnt;
		}

		if ((flags & HF_UPDATE_SLOTS) && (header->feature_flags & FF_KEY_SLOTS))
		{
			if (flags & HF_CLEAR_SLOTS) {
				make_rand(((u8*)header) + DC_BASE_SIZE, header->slot_area_len);
				memset(((u8*)header) + DC_BASE_SIZE + header->slot_area_len, 0,
				       (size_t)header->key_slot_count * header->slot_info_size);
			} else {
				memcpy(((u8*)header) + DC_BASE_SIZE, ((u8*)upd) + DC_BASE_SIZE,
				       header->slot_area_len + (size_t)header->key_slot_count * header->slot_info_size);
			}
		}

		if ((flags & HF_UPDATE_EXT) && ext_hdr != NULL && header->ext_hdr_off != 0)
			memcpy(((u8*)header) + header->ext_hdr_off, ext_hdr, ext_hdr->size);

		header->hdr_crc = calculate_header_crc(header);
	}

write:
	//
	// Sealed under the key the volume was opened with, which keeps the salt -
	// and with it every key slot - exactly as it was.
	//
	{
		u8* enc_header = NULL;

		if (dc_encrypt_header(header, head_len, hdr_key, &enc_header) != ST_OK) {
			ret = ERR_INTERNAL;
			goto finish;
		}

		ret = pIO->DiskWrite(enc_header, head_len, 0) ? ERR_OK : ERR_IO_FAILED;
		secure_free(enc_header);
	}

finish:
	if (password) secure_free(password);
	if (upd) secure_free(upd);
	if (header) secure_free(header);
	if (hdr_key) secure_free(hdr_key);
	return ret;
}

void CCryptoIO::SetDataSection(SSection* pSection)
{
	m->section = pSection;
}

//int CCryptoIO::SetData(const UCHAR* pData, SIZE_T uSize)
//{
//	int ret = m_pIO ? m_pIO->Init() : ERR_UNKNOWN_TYPE;
//
//	if (ret != ERR_OK)
//		return ret;
//
//	SSecureBuffer<dc_header> header;
//	if (!header) {
//		DbgPrint(L"Malloc Failed\n");
//		return ERR_MALLOC_ERROR;
//	}
//
//	m_pIO->DiskRead(header.ptr, sizeof(dc_header), 0);
//
//	ret = dc_decrypt_header(header.ptr, m->password.ptr) ? ERR_OK : ERR_WRONG_PASSWORD;
//	if (ret != ERR_OK)
//		return ret;
//
//	if(uSize> sizeof(header.ptr->info_data))
//		return ERR_DATA_TO_LONG;
//
//	header.ptr->info_magic = DC_INFO_MAGIC;
//	header.ptr->info_reserved = 0;
//	header.ptr->info_size = (u16)uSize;
//	memcpy(header.ptr->info_data, pData, uSize);
//
//	ret = WriteHeader(header.ptr);
//
//	return ret;
//}
//
//int CCryptoIO::GetData(UCHAR* pData, SIZE_T* pSize)
//{
//	int ret = m_pIO ? m_pIO->Init() : ERR_UNKNOWN_TYPE;
//
//	if (ret != ERR_OK)
//		return ret;
//
//	SSecureBuffer<dc_header> header;
//	if (!header) {
//		DbgPrint(L"Malloc Failed\n");
//		return ERR_MALLOC_ERROR;
//	}
//
//	m_pIO->DiskRead(header.ptr, sizeof(dc_header), 0);
//
//	ret = dc_decrypt_header(header.ptr, m->password.ptr) ? ERR_OK : ERR_WRONG_PASSWORD;
//	if (ret != ERR_OK)
//		return ret;
//
//	if (header.ptr->info_magic != DC_INFO_MAGIC)
//		return ERR_DATA_NOT_FOUND;
//
//	//if (*pSize < header.ptr->info_size)
//	//	return ERR_BUFFER_TO_SMALL;
//
//	*pSize = header.ptr->info_size;
//	memcpy(pData, header.ptr->info_data, *pSize);
//
//	return ERR_OK;
//}
