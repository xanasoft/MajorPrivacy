#include "pch.h"
#include "framework.h"
#include <bcrypt.h>
#include "CryptoIO.h"
#include "..\ImBox\ImBox.h"
#include "..\ImBox\Common\helpers.h"

#pragma comment(lib,"Bcrypt.lib")

extern "C" {
#include "..\ImBox\dc\include\boot\dc_header.h"
#include ".\crypto_fast\crc32.h"
#include ".\crypto_fast\sha512_pkcs5_2.h"
}

void make_rand(void* ptr, size_t size)
{
	BCryptGenRandom(NULL, (BYTE*)ptr, size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

template <class T>
struct SSecureBuffer
{
	SSecureBuffer() { alloc(sizeof(T)); }
	SSecureBuffer(ULONG length) { alloc(length); }
	~SSecureBuffer() { free(); }

	void alloc(ULONG length)
	{
		// on 32 bit system xts_key must be located in executable memory
		// x64 does not require this
#ifdef _M_IX86
		ptr = (T*)VirtualAlloc(NULL, length, MEM_COMMIT + MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
		ptr = (T*)VirtualAlloc(NULL, length, MEM_COMMIT + MEM_RESERVE, PAGE_READWRITE);
#endif
		if(ptr)
			VirtualLock(ptr, length);
	}

	void free() 
	{
		if (!ptr)
			return;

		MEMORY_BASIC_INFORMATION mbi;
		if ( (VirtualQuery(ptr, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.BaseAddress == ptr && mbi.AllocationBase == ptr) )
		{
			RtlSecureZeroMemory(ptr, mbi.RegionSize);
			VirtualUnlock(ptr, mbi.RegionSize);
		}
		VirtualFree(ptr, 0, MEM_RELEASE);

		ptr = NULL;
	}

	T* operator ->() { return ptr; }
	explicit operator bool() { return ptr != NULL; }

	T* ptr;
};

struct SCryptoIO
{
	std::wstring Cipher;
	bool AllowFormat;

	SSecureBuffer<dc_pass> password;

	xts_key benc_k;

	SSection* section;
};

CCryptoIO::CCryptoIO(CAbstractIO* pIO, const WCHAR* pKey, const std::wstring& Cipher)
{
	m = new SCryptoIO;
	m->Cipher = Cipher;
	m->AllowFormat = false;
/*
#ifdef ENCLAVE_ENABLED
	if (!IsEnclaveTypeSupported(ENCLAVE_TYPE_VBS))
	{
		OutputDebugString(L"Enclave not supported!\n");
		ExitProcess(STATUS_NOT_SUPPORTED);
	}
#endif //  ENCLAVE_ENABLED*/

	if (m->password) {
		m->password->size = wcslen(pKey) * sizeof(wchar_t);
		if (m->password->size > MAX_PASSWORD * sizeof(wchar_t))
			m->password->size = MAX_PASSWORD * sizeof(wchar_t);
		memcpy(m->password->pass, pKey, m->password->size);
	}

	m->section = NULL;

	m_pIO = pIO;

	xts_init(1);
}

CCryptoIO::~CCryptoIO()
{
	delete m;
}

ULONG64 CCryptoIO::GetDiskSize() const 
{ 
	ULONG64 uSize = m_pIO->GetDiskSize();
	if (uSize < DC_AREA_SIZE)
		return 0;
	return uSize - DC_AREA_SIZE;
}

bool CCryptoIO::CanBeFormated() const
{
	return m->AllowFormat;
}

int CCryptoIO::InitCrypto()
{
	if (DC_AREA_SIZE != sizeof dc_header) {
		DbgPrint(L"dc_header struct invalid!\n");
		return ERR_INTERNAL;
	}

	if (!m->password)
		return ERR_KEY_REQUIRED;

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
		DbgPrint(L"Unknown Cipher.\n");
		return ERR_UNKNOWN_CIPHER;
	}

	m->AllowFormat = m_pIO->CanBeFormated();
	if (m->AllowFormat) {
		
		DbgPrint(L"Creating DC header\n");

		SSecureBuffer<dc_header> header;
		if (!header) {
			DbgPrint(L"Malloc Failed\n");
			return ERR_MALLOC_ERROR;
		}

		// create the volume header
		memset((BYTE*)header.ptr, 0, sizeof(dc_header));
	
		make_rand(&header->disk_id, sizeof(header->disk_id));
		make_rand(header->key_1, sizeof(header->key_1));

		header->sign     = DC_VOLUME_SIGN;
		header->version  = DC_HDR_VERSION;
		header->flags    = VF_NO_REDIR;
		header->alg_1    = cipher;
		header->data_off = sizeof(dc_header);
		header->hdr_crc  = crc32((const unsigned char*)&header->version, DC_CRC_AREA_SIZE);

		WriteHeader(header.ptr);
	}


	DbgPrint(L"Trying to decrypt header...");

	SSecureBuffer<dc_header> header;
	if (!header) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}

	m_pIO->DiskRead(header.ptr, sizeof(dc_header), 0);

	int ret = dc_decrypt_header(header.ptr, m->password.ptr) ? ERR_OK : (m->AllowFormat ? ERR_INTERNAL : ERR_WRONG_PASSWORD);
/*
#ifdef ENCLAVE_ENABLED
	// Create the enclave
	if (ret == ERR_OK) {
		constexpr ENCLAVE_CREATE_INFO_VBS CreateInfo
		{
			//ENCLAVE_VBS_FLAG_DEBUG, // Flags
			0,
			{ 0x10, 0x22, 0x30, 0x45, 0x41, 0x37, 0x21, 0x13 }, // OwnerID
		};
		Enclave = CreateEnclave(GetCurrentProcess(),
			nullptr, // Preferred base address
			0x10000000, // size
			0,
			ENCLAVE_TYPE_VBS,
			&CreateInfo,
			sizeof(ENCLAVE_CREATE_INFO_VBS),
			nullptr);
	}
		if (Enclave == NULL) {
			DbgPrint(L"CreateEnclave failed\n");
			ret = ERR_INTERNAL;
		}
		if (ret == ERR_OK)
			if (LoadEnclaveImageW(Enclave, L"ImBoxEnclave.dll") == FALSE)
				ret = ERR_INTERNAL;
		if (ret == ERR_OK) {
			ENCLAVE_INIT_INFO_VBS InitInfo{};

			InitInfo.Length = sizeof(ENCLAVE_INIT_INFO_VBS);
			InitInfo.ThreadCount = 1;
			if (InitializeEnclave(GetCurrentProcess(),
				Enclave,
				&InitInfo,
				InitInfo.Length,
				nullptr) == 0) {
				ret= ERR_INTERNAL;
			}
		}


#endif // ENCLAVE_ENABLED
*/
	if (ret == ERR_OK) {
//#ifndef ENCLAVE_ENABLED
		xts_set_key(header->key_1, header->alg_1, &m->benc_k);
//#else
		/*
		PENCLAVE_ROUTINE EnclaveSetKey = reinterpret_cast<PENCLAVE_ROUTINE>(GetProcAddress(reinterpret_cast<HMODULE>(Enclave), "EnclaveSetKey"));
		KeySetArgs args;
		memset(&args,0,sizeof(args));
		args.alg = header->alg_1;
		args.key= header->key_1;
		args.skey= &m->benc_k;
		LPVOID rtn = 0;//We ignore the return value
		if (CallEnclave(EnclaveSetKey, &args, 0, &rtn) == FALSE) {
			return ERR_INTERNAL;
		}
		SecureZeroMemory(&args,sizeof(args));
		SecureZeroMemory(rtn, sizeof(rtn));
#endif*/
		if (m->section && header->info_magic == DC_INFO_MAGIC) {
			m->section->magic = SECTION_MAGIC;
			m->section->id = SECTION_PARAM_ID_DATA;
			m->section->size = header->info_size;
			memcpy(m->section->data, header->info_data, header->info_size);
		}

		DbgPrint(L" SUCCESS.\n");
	}
	else
		DbgPrint(L" FAILED.\n");
	return ret;
}

int CCryptoIO::Init()
{
	int ret = m_pIO ? m_pIO->Init() : ERR_UNKNOWN_TYPE;

	if (ret == ERR_OK)
		ret = InitCrypto();
	
	m->password.free();
#ifdef ENCLAVE_ENABLED
	delete& m->benc_k;
	//delete& m->Cipher;
	//Clear key in the enternal thread
#endif


	return ret;
}

int CCryptoIO::WriteHeader(struct _dc_header* header)
{
	SSecureBuffer<xts_key> header_key;
	UCHAR salt[PKCS5_SALT_SIZE];
	SSecureBuffer<UCHAR> dk(DISKKEY_SIZE);

	// allocate required memory
	if (!header_key || !dk)
	{
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}
	
	make_rand(salt, PKCS5_SALT_SIZE);

	// derive the header key
	sha512_pkcs5_2(1000, m->password->pass, m->password->size, salt, PKCS5_SALT_SIZE, dk.ptr, PKCS_DERIVE_MAX);

	// initialize encryption keys
	xts_set_key(dk.ptr, header->alg_1, header_key.ptr);

	// encrypt the volume header
	xts_encrypt((const unsigned char*)header, (unsigned char*)header, sizeof(dc_header), 0, header_key.ptr);

	// save salt
	memcpy(header->salt, salt, PKCS5_SALT_SIZE);

	// write volume header to output file
	m_pIO->DiskWrite(header, sizeof(dc_header), 0);

	return ERR_OK;
}

int CCryptoIO::ChangePassword(const WCHAR* pNewKey)
{
	int ret = m_pIO ? m_pIO->Init() : ERR_UNKNOWN_TYPE;

	if (ret != ERR_OK)
		return ret;

	SSecureBuffer<dc_header> header;
	if (!header) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}

	m_pIO->DiskRead(header.ptr, sizeof(dc_header), 0);

	ret = dc_decrypt_header(header.ptr, m->password.ptr) ? ERR_OK : ERR_WRONG_PASSWORD;

	if (ret != ERR_OK)
		return ret;

	if (m->password) {
		m->password->size = wcslen(pNewKey) * sizeof(wchar_t);
		if (m->password->size > MAX_PASSWORD * sizeof(wchar_t))
			m->password->size = MAX_PASSWORD * sizeof(wchar_t);
		memcpy(m->password->pass, pNewKey, m->password->size);
	}

	ret = WriteHeader(header.ptr);

	return ret;
}

bool CCryptoIO::DiskWrite(void* buf, int size, __int64 offset)
{
#ifdef _DEBUG
	if ((offset & 0x1FF) || (size & 0x1FF))
		DbgPrint(L"DiskWrite not full sector\n");
#endif

	xts_encrypt((BYTE*)buf, (BYTE*)buf, size, offset, &m->benc_k);

	bool ret = m_pIO->DiskWrite(buf, size, offset + DC_AREA_SIZE);

	//xts_decrypt((BYTE*)buf, (BYTE*)buf, size, offset, &m->benc_k); // restore buffer - not needed

	return ret;
}

bool CCryptoIO::DiskRead(void* buf, int size, __int64 offset)
{
#ifdef _DEBUG
	if ((offset & 0x1FF) || (size & 0x1FF))
		DbgPrint(L"DiskRead not full sector\n");
#endif

	bool ret = m_pIO->DiskRead(buf, size, offset + DC_AREA_SIZE);

	if (ret)
		xts_decrypt((BYTE*)buf, (BYTE*)buf, size, offset, &m->benc_k);

	return ret;
}

void CCryptoIO::TrimProcess(DEVICE_DATA_SET_RANGE* range, int n)
{
	for (DEVICE_DATA_SET_RANGE* range2 = range; range2 < range + n; range2++) {
		range2->StartingOffset += DC_AREA_SIZE;
#ifdef _DEBUG
		if (range2->StartingOffset & 0x1FF || range2->LengthInBytes & 0x1FF)
			DbgPrint(L"TrimProcess not full sector\n");
#endif
	}

	m_pIO->TrimProcess(range, n);
}

int CCryptoIO::BackupHeader(CAbstractIO* pIO, const std::wstring& Path)
{
	int ret = pIO->Init();

	if (ret != ERR_OK)
		return ret;

	SSecureBuffer<dc_header> header;
	if (!header) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}

	if (!pIO->DiskRead(header.ptr, sizeof(dc_header), 0))
		ret = ERR_FILE_NOT_OPENED;

	if (ret != ERR_OK)
		return ret;

	HANDLE hFile = CreateFile(Path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD BytesWritten;
		if (!WriteFile(hFile, header.ptr, sizeof(dc_header), &BytesWritten, NULL)) {
			ret = ERR_FILE_NOT_OPENED;
		}
		CloseHandle(hFile);
	} else
		ret = ERR_FILE_NOT_OPENED;

	return ret;
}

int CCryptoIO::RestoreHeader(CAbstractIO* pIO, const std::wstring& Path)
{
	int ret = pIO->Init();

	if (ret != ERR_OK)
		return ret;

	SSecureBuffer<dc_header> header;
	if (!header) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}

	HANDLE hFile = CreateFile(Path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD BytesRead;
		if (!ReadFile(hFile, header.ptr, sizeof(dc_header), &BytesRead, NULL)) {
			ret = ERR_FILE_NOT_OPENED;
		}
		CloseHandle(hFile);
	} else
		ret = ERR_FILE_NOT_OPENED;

	if (ret != ERR_OK)
		return ret;

	if (!pIO->DiskWrite(header.ptr, sizeof(dc_header), 0))
		ret = ERR_FILE_NOT_OPENED;

	return ret;
}

void CCryptoIO::SetDataSection(SSection* pSection)
{
	m->section = pSection;
}

int CCryptoIO::SetData(const UCHAR* pData, SIZE_T uSize)
{
	int ret = m_pIO ? m_pIO->Init() : ERR_UNKNOWN_TYPE;

	if (ret != ERR_OK)
		return ret;

	SSecureBuffer<dc_header> header;
	if (!header) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}

	m_pIO->DiskRead(header.ptr, sizeof(dc_header), 0);

	ret = dc_decrypt_header(header.ptr, m->password.ptr) ? ERR_OK : ERR_WRONG_PASSWORD;

	if (ret != ERR_OK)
		return ret;

	if(uSize> sizeof(header.ptr->info_data))
		return ERR_DATA_TO_LONG;

	header.ptr->info_magic = DC_INFO_MAGIC;
	header.ptr->info_reserved = 0;
	header.ptr->info_size = uSize;
	memcpy(header.ptr->info_data, pData, uSize);

	ret = WriteHeader(header.ptr);

	return ret;
}

int CCryptoIO::GetData(UCHAR* pData, SIZE_T* pSize)
{
	int ret = m_pIO ? m_pIO->Init() : ERR_UNKNOWN_TYPE;

	if (ret != ERR_OK)
		return ret;

	SSecureBuffer<dc_header> header;
	if (!header) {
		DbgPrint(L"Malloc Failed\n");
		return ERR_MALLOC_ERROR;
	}

	m_pIO->DiskRead(header.ptr, sizeof(dc_header), 0);

	ret = dc_decrypt_header(header.ptr, m->password.ptr) ? ERR_OK : ERR_WRONG_PASSWORD;

	if (ret != ERR_OK)
		return ret;

	if (header.ptr->info_magic != DC_INFO_MAGIC)
		return ERR_DATA_NOT_FOUND;

	//if (*pSize < header.ptr->info_size)
	//	return ERR_BUFFER_TO_SMALL;

	*pSize = header.ptr->info_size;
	memcpy(pData, header.ptr->info_data, *pSize);

	return ERR_OK;
}