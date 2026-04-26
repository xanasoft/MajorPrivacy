#include "framework.h"
#include <bcrypt.h>
#include "CryptoIO.h"
#include "ImBox.h"
#include "Common\helpers.h"

extern "C" {
#include ".\dc\include\dc_header.h"
#include ".\dc\crypto_fast\crc32.h"
#include ".\dc\crypto_fast\sha512_pkcs5_2.h"
#include ".\dc\crypto\Argon2\argon2.h"

struct mem_block
{
	SIZE_T size;
	PVOID data;
};

PVOID secure_alloc(SIZE_T size)
{
	mem_block* mem;
	SIZE_T full_size = sizeof(mem_block) + size;
	// on 32 bit system xts_key must be located in executable memory
	// x64 does not require this
#ifdef _M_IX86
	mem = (mem_block*)VirtualAlloc(NULL, full_size, MEM_COMMIT + MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
	mem = (mem_block*)VirtualAlloc(NULL, full_size, MEM_COMMIT + MEM_RESERVE, PAGE_READWRITE);
#endif
	if (!mem)
		return NULL;
	mem->data = ((char*)mem) + sizeof(mem_block);
	mem->size = size;
	RtlSecureZeroMemory(mem->data, mem->size);
	VirtualLock(mem, full_size);
	return mem->data;
}

void secure_free(PVOID ptr)
{
	if (!ptr) 
		return;
	mem_block* mem = (mem_block*)((BYTE*)ptr - sizeof(mem_block));
	if (mem->data != ptr) { // sanity check, should never happen
#ifdef _DEBUG
		DebugBreak();
#endif
		return;
	}
	SIZE_T full_size = sizeof(mem_block) + mem->size;
	RtlSecureZeroMemory(mem->data, mem->size);
	VirtualUnlock(mem, full_size);
	VirtualFree(mem, 0, MEM_RELEASE);
}

void burn(PVOID ptr, SIZE_T size)
{
	RtlSecureZeroMemory(ptr, size);
}

void make_rand(void* ptr, size_t size)
{
	BCryptGenRandom(NULL, (BYTE*)ptr, (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

}

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
	bool AllowFormat = false;
	int hdr_len = DC_AREA_SIZE;

	dc_pass* password = NULL;
	xts_key benc_k;

	SSection* section;
};

CCryptoIO::CCryptoIO(CAbstractIO* pIO, const SPassword* pass, const std::wstring& Cipher)
{
	m = new SCryptoIO;
	m->Cipher = Cipher;
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

int CCryptoIO::InitCrypto()
{
	if (!m->password)
		return ERR_KEY_REQUIRED;

	int ret = ERR_OK;
	dc_header* header = NULL;
	u8* enc_header = NULL;
	int kdf = -1;

	m->AllowFormat = m_pIO->CanBeFormated();
	if (m->AllowFormat) {

		m->hdr_len = 8192;

		DbgPrint(L"Creating DCv2 header, size: %u\n", m->hdr_len);

		int cipher;
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

		header = (dc_header*)secure_alloc(m->hdr_len);
		if (!header) {
			DbgPrint(L"Malloc Failed\n");
			ret = ERR_MALLOC_ERROR;
			goto finish;
		}

		// create the volume header
		memset((BYTE*)header, 0, m->hdr_len);
	
		make_rand(&header->disk_id, sizeof(header->disk_id));
		make_rand(header->key_1, sizeof(header->key_1));

		header->sign     = DC_VOLUME_SIGN;
		header->version  = DC_HDR_VERSION_2;
		make_rand(&header->disk_id, sizeof(header->disk_id));
		header->flags    = VF_NO_REDIR; // no redirection
		header->alg_1    = cipher;
		header->head_kdf = m->password->kdf;
		header->data_off = m->hdr_len;
		header->head_len = m->hdr_len;
		header->hdr_crc  = crc32((const unsigned char*)&header->version, DC_CRC_AREA_SIZE_2);

		make_rand(header->salt, HEADER_SALT_SIZE);
		WriteHeader(header);

		//BYTE buffer[512];
		//memset(buffer, 0, sizeof(buffer));
		//m_pIO->DiskWrite(buffer, sizeof(buffer), m_pIO->GetDiskSize() - sizeof(buffer));

		secure_free(header);
		header = NULL;
	}

	DbgPrint(L"Trying to decrypt header...\n");

	m->hdr_len = dc_get_min_header_len(m->password);
	if (m->hdr_len > m_pIO->GetDiskSize())
		m->hdr_len = m_pIO->GetDiskSize();

	enc_header = (u8*)secure_alloc(m->hdr_len);
	if (!enc_header) {
		DbgPrint(L"Malloc Failed\n");
		ret = ERR_MALLOC_ERROR;
		goto finish;
	}

	if(!m_pIO->DiskRead(enc_header, m->hdr_len, 0)) {
		DbgPrint(L"DiskRead Failed\n");
		ret = ERR_IO_FAILED;
		goto finish;
	}

	ret = dc_decrypt_header(enc_header, m->hdr_len, m->password, &header, NULL, NULL, &kdf) == ST_OK ? ERR_OK : (m->AllowFormat ? ERR_INTERNAL : ERR_WRONG_PASSWORD);
	if (ret != ERR_OK) {
		DbgPrint(L"Wrong Password\n");
		goto finish;
	}

	xts_set_key(header->key_1, header->alg_1, &m->benc_k);

	if (header->version >= DC_HDR_VERSION_2)
		m->hdr_len = header->head_len;
	else
		m->hdr_len = DC_AREA_SIZE;

	if (m->hdr_len > m_pIO->GetDiskSize()) {
		DbgPrint(L"Invalid header length\n");
		ret = ERR_INTERNAL;
		goto finish;
	}
	
	m->section->out.info.cipher_id = header->alg_1;
	m->section->out.info.head_kdf = kdf;
	m->section->out.info.version = header->version;
	m->section->out.info.head_len = m->hdr_len;
	m->section->out.info.slot_count = header->key_slot_count;
	m->section->out.info.disk_id = header->disk_id;

	//if (m->section && header->info_magic == DC_INFO_MAGIC) {
	//	m->section->id = SECTION_PARAM_ID_DATA;
	//	m->section->size = header->info_size;
	//	memcpy(m->section->data, header->info_data, header->info_size);
	//}

	DbgPrint(L"Success\n");
	
finish:
	if (header) secure_free(header);
	if (enc_header) secure_free(enc_header);
	return ret;
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

int CCryptoIO::WriteHeader(struct _dc_header* header)
{
	int ret = ERR_INTERNAL;
	xts_key hdr_key;
	u8 *enc_header = NULL;

	if (dc_set_header_key(&hdr_key, header->salt, header->alg_1, m->password))
	{
		if (dc_encrypt_header(header, m->hdr_len, &hdr_key, &enc_header) == ST_OK)
		{
			// write volume header to output file
			if(m_pIO->DiskWrite(enc_header, m->hdr_len, 0))
				ret = ERR_OK;
			else
				ret = ERR_IO_FAILED;
		}
	}

	burn(&hdr_key, sizeof(hdr_key));
	if (enc_header) secure_free(enc_header);
	return ret;
}

int CCryptoIO::ChangePassword(const SPassword* new_pass)
{
	dc_header* header = NULL;
	u8* enc_header = NULL;
	xts_key* hdr_key = NULL;
	int hdr_len;
	int ret = m_pIO ? m_pIO->Init() : ERR_UNKNOWN_TYPE;
	if (ret != ERR_OK)
		return ret;

	hdr_len = dc_get_min_header_len(m->password);
	if (hdr_len > m_pIO->GetDiskSize())
		hdr_len = m_pIO->GetDiskSize();

redo:
	enc_header = (u8*)secure_alloc(hdr_len);
	if (!enc_header) {
		DbgPrint(L"Malloc Failed\n");
		ret = ERR_MALLOC_ERROR;
		goto finish;
	}

	if(!m_pIO->DiskRead(enc_header, hdr_len, 0)) {
		DbgPrint(L"DiskRead Failed\n");
		ret = ERR_IO_FAILED;
		goto finish;
	}

	header = (dc_header*)secure_alloc(hdr_len);
	if (!header) {
		DbgPrint(L"Malloc Failed\n");
		ret = ERR_MALLOC_ERROR;
		goto finish;
	}

	if (hdr_key)
	{
		if (!dc_decrypt_header_with_key(enc_header, hdr_len, header, hdr_key)) {
			ret = ERR_INTERNAL;
			goto finish;
		}
	}
	else
	{
		ret = dc_decrypt_header(enc_header, hdr_len, m->password, &header, &hdr_key, NULL, NULL) == ST_OK ? ERR_OK : ERR_WRONG_PASSWORD;
		if (ret != ERR_OK) {
			DbgPrint(L"Wrong Password\n");
			goto finish;
		}

		if (header->version >= DC_HDR_VERSION_2 && header->head_len > hdr_len) {
			hdr_len = header->head_len;
			secure_free(enc_header);
			secure_free(header);
			header = NULL;
			enc_header = NULL;
			goto redo;
		}
	}

	if (new_pass->slot == 0) 
		memcpy(m->password, new_pass, sizeof(SPassword));
	else
	{
		// todo add support for slots
		ret = ERR_INVALID_PARAM;
		goto finish;
	}

	if (header->version >= DC_HDR_VERSION_2)
	{
		header->head_kdf = new_pass->kdf;
	}

	//make_rand(header->salt, HEADER_SALT_SIZE); // keep salt

	ret = WriteHeader(header);

finish:
	if (header) secure_free(header);
	if (enc_header) secure_free(enc_header);
	if (hdr_key) secure_free(hdr_key);
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
	u8* enc_header = NULL;
	xts_key* hdr_key = NULL;
	int hdr_len;
	dc_pass* password = NULL;
	UCHAR salt[HEADER_SALT_SIZE];
	xts_key bak_key;
	int ret = pIO->Init();
	if (ret != ERR_OK)
		return ret;

	password = (dc_pass*)secure_alloc(sizeof(dc_pass));
	if (!password) {
		DbgPrint(L"Malloc Failed\n");
		ret = ERR_MALLOC_ERROR;
		goto finish;
	}

	memcpy(password, pass, sizeof(SPassword));

	hdr_len = dc_get_min_header_len(password);
	if (hdr_len > pIO->GetDiskSize())
		hdr_len = pIO->GetDiskSize();

redo:
	enc_header = (u8*)secure_alloc(hdr_len);
	if (!enc_header) {
		DbgPrint(L"Malloc Failed\n");
		ret = ERR_MALLOC_ERROR;
		goto finish;
	}

	if(!pIO->DiskRead(enc_header, hdr_len, 0)) {
		DbgPrint(L"DiskRead Failed\n");
		ret = ERR_IO_FAILED;
		goto finish;
	}

	header = (dc_header*)secure_alloc(hdr_len);
	if (!header) {
		DbgPrint(L"Malloc Failed\n");
		ret = ERR_MALLOC_ERROR;
		goto finish;
	}

	if (hdr_key)
	{
		if (!dc_decrypt_header_with_key(enc_header, hdr_len, header, hdr_key)) {
			ret = ERR_INTERNAL;
			goto finish;
		}
	}
	else
	{
		ret = dc_decrypt_header(enc_header, hdr_len, password, &header, &hdr_key, NULL, &password->kdf) == ST_OK ? ERR_OK : ERR_WRONG_PASSWORD;
		if (ret != ERR_OK) {
			DbgPrint(L"Wrong Password\n");
			goto finish;
		}

		if (header->version >= DC_HDR_VERSION_2 && header->head_len > hdr_len) {
			hdr_len = header->head_len;
			secure_free(enc_header);
			secure_free(header);
			header = NULL;
			enc_header = NULL;
			goto redo;
		}
	}

	if (header->version >= DC_HDR_VERSION_2)
		hdr_len = header->head_len;
	else
		hdr_len = DC_AREA_SIZE;

	memcpy(salt, header->salt, HEADER_SALT_SIZE);
	//make_rand(salt, HEADER_SALT_SIZE);

	ret = ERR_INTERNAL;
	if (dc_set_header_key(&bak_key, salt, header->alg_1, password))
	{
		if (dc_encrypt_header(header, hdr_len, &bak_key, &enc_header) == ST_OK)
		{
			ret = ERR_OK;
			HANDLE hFile = CreateFile(Path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
			if (hFile != INVALID_HANDLE_VALUE) {
				DWORD BytesWritten;
				if (!WriteFile(hFile, enc_header, hdr_len, &BytesWritten, NULL)) {
					ret = ERR_FILE_NOT_OPENED;
				}
				CloseHandle(hFile);
			} else
				ret = ERR_FILE_NOT_OPENED;
		}
	}

finish:
	if (password) secure_free(password);
	burn(salt, sizeof(salt));
	burn(&bak_key, sizeof(bak_key));
	if (header) secure_free(header);
	if (enc_header) secure_free(enc_header);
	if (hdr_key) secure_free(hdr_key);
	return ret;
}

int CCryptoIO::RestoreHeader(CAbstractIO* pIO, const std::wstring& Path, const SPassword* pass)
{
	dc_header* header = NULL;
	u8* enc_header = NULL;
	int hdr_len;
	dc_pass* password = NULL;
	UCHAR salt[HEADER_SALT_SIZE];
	xts_key bak_key;
	LARGE_INTEGER fileSize;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	int ret = pIO->Init();
	if (ret != ERR_OK)
		return ret;

	password = (dc_pass*)secure_alloc(sizeof(dc_pass));
	if (!password) {
		DbgPrint(L"Malloc Failed\n");
		ret = ERR_MALLOC_ERROR;
		goto finish;
	}

	memcpy(password, pass, sizeof(SPassword));

	hFile = CreateFile(Path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
	if (hFile != INVALID_HANDLE_VALUE) 
	{
		if(!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart < DC_AREA_SIZE || fileSize.QuadPart > DC_AREA_MAX_SIZE) {
			DbgPrint(L"Invalid header backup file\n");
			ret = ERR_FILE_NOT_OPENED;
			goto finish;
		}

		hdr_len = (int)fileSize.QuadPart;
		enc_header = (u8*)secure_alloc(hdr_len);
		if (!enc_header) {
			DbgPrint(L"Malloc Failed\n");
			ret = ERR_MALLOC_ERROR;
			goto finish;
		}

		DWORD BytesRead;
		if (!ReadFile(hFile, enc_header, hdr_len, &BytesRead, NULL)) {
			ret = ERR_FILE_NOT_OPENED;
		}
		CloseHandle(hFile);
		hFile = INVALID_HANDLE_VALUE;
	} else
		ret = ERR_FILE_NOT_OPENED;

	ret = dc_decrypt_header(enc_header, hdr_len, password, &header, NULL, NULL, &password->kdf) == ST_OK ? ERR_OK : ERR_WRONG_PASSWORD;
	if (ret != ERR_OK) {
		DbgPrint(L"Wrong Password\n");
		goto finish;
	}

	if (header->version >= DC_HDR_VERSION_2)
		hdr_len = header->head_len;
	else
		hdr_len = DC_AREA_SIZE;

	memcpy(salt, header->salt, HEADER_SALT_SIZE);
	//make_rand(salt, HEADER_SALT_SIZE);

	ret = ERR_INTERNAL;
	if (dc_set_header_key(&bak_key, salt, header->alg_1, password))
	{
		if (dc_encrypt_header(header, hdr_len, &bak_key, &enc_header) == ST_OK)
		{
			if (pIO->DiskWrite(enc_header, hdr_len, 0))
				ret = ERR_OK;
			else
				ret = ERR_IO_FAILED;
		}
	}

finish:
	if (password) secure_free(password);
	burn(salt, sizeof(salt));
	burn(&bak_key, sizeof(bak_key));
	if (header) secure_free(header);
	if (enc_header) secure_free(enc_header);
	if(hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
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