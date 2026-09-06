#include "pch.h"
#include "SecurePassword.h"
#include "Encryption.h"
#include "../API/PrivacyAPI.h"
#include <bcrypt.h>

struct mem_block
{
	SIZE_T size;
	PVOID data;
};

static PVOID secure_alloc(SIZE_T size)
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

static void secure_free(PVOID ptr)
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


struct SSecurePassword
{
	volatile LONG Refs;
	size_t      size;                           // password length in bytes without terminating null
	wchar_t     pass[MAX_SEC_PASSWORD + 1];     // password in UTF16-LE encoding (static size)
};

static SSecurePassword* AllocSecurePassword()
{
	SSecurePassword* ptr = (SSecurePassword*)secure_alloc(sizeof(SSecurePassword));
	if (ptr) {
		ptr->Refs = 0;
		ptr->size = 0;
		// pass is already zeroed by secure_alloc
	}
	return ptr;
}

static void FreeSecurePassword(SSecurePassword* ptr)
{
	secure_free(ptr);
}


CSecurePassword::CSecurePassword()
{
}

CSecurePassword::CSecurePassword(const wchar_t* pass, size_t size)
{
	if (pass) {
		if (size == (size_t)-1)
			size = wcslen(pass) * sizeof(wchar_t);
		SetPassword(pass, size);
	}
}

CSecurePassword::~CSecurePassword()
{
	DetachData();
}

CSecurePassword::CSecurePassword(const CSecurePassword& other)
{
	AttachData(other.m);
}

CSecurePassword& CSecurePassword::operator=(const CSecurePassword& other)
{
	if (m != other.m)
		AttachData(other.m);
	return *this;
}

CSecurePassword::CSecurePassword(CSecurePassword&& other) noexcept
	: m(other.m)
{
	other.m = nullptr;
}

CSecurePassword& CSecurePassword::operator=(CSecurePassword&& other) noexcept
{
	if (this != &other)
	{
		DetachData();
		m = other.m;
		other.m = nullptr;
	}
	return *this;
}

bool CSecurePassword::operator==(const CSecurePassword& other) const
{
	// Same shared data or both null
	if (m == other.m)
		return true;

	// One is null, the other is not
	if (!m || !other.m)
		return false;

	// Different sizes
	if (m->size != other.m->size)
		return false;

	// Compare contents
	if (m->size == 0)
		return true;

	return memcmp(m->pass, other.m->pass, m->size) == 0;
}

void CSecurePassword::AttachData(SSecurePassword* ptr)
{
	DetachData();

	if (ptr) {
		m = ptr;
		InterlockedIncrement(&m->Refs);
	}
}

void CSecurePassword::DetachData()
{
	if (m) {
		if (InterlockedDecrement(&m->Refs) == 0)
			FreeSecurePassword(m);
		m = nullptr;
	}
}

bool CSecurePassword::MakeExclusive()
{
	if (!m)
		return true; // nothing to make exclusive

	if (m->Refs > 1)
	{
		// Need to make a copy
		SSecurePassword* newData = AllocSecurePassword();
		if (!newData)
			return false;

		newData->size = m->size;
		memcpy(newData->pass, m->pass, m->size);
		newData->pass[m->size / sizeof(wchar_t)] = L'\0';

		AttachData(newData);
	}
	return true;
}

StVariant CSecurePassword::ToVariant(const SVarWriteOpt& Opts) const
{
	StVariant Data;
	if (m && m->size > 0)
		Data[API_V_DATA] = std::wstring(m->pass, m->size / sizeof(wchar_t));
	return Data;
}

bool CSecurePassword::FromVariant(const StVariant& Data)
{
	std::wstring pass = Data[API_V_DATA].AsStr();
	if (!pass.empty())
		SetPassword(pass.c_str(), pass.length() * sizeof(wchar_t));
	return true;
}

StVariant CSecurePassword::ToVariant(const CBuffer& SharedSecret, const SVarWriteOpt& Opts) const
{
	if(SharedSecret.GetSize() == 0)
		return ToVariant(Opts); // Fall back to unencrypted if no shared secret

	StVariant Data;
	if (!m || m->size == 0)
		return Data;

	// Generate random IV
	CBuffer iv;
	iv.SetSize(CEncryption::IV_SIZE, true);
	BCryptGenRandom(NULL, iv.GetBuffer(), (ULONG)iv.GetSize(), BCRYPT_USE_SYSTEM_PREFERRED_RNG);

	// Setup encryption
	CEncryption enc;
	if (!NT_SUCCESS(enc.SetSymetricKey(SharedSecret)))
		return Data;
	if (!NT_SUCCESS(enc.InitCrypto()))
		return Data;

	// Encrypt password
	CBuffer plaintext;
	plaintext.SetBuffer(m->pass, m->size, true, true);

	CBuffer ciphertext;
	if (!NT_SUCCESS(enc.Encrypt(plaintext, ciphertext, iv)))
		return Data;

	// Store encrypted password and IV
	Data[API_V_DATA] = ciphertext;
	Data[API_V_RAND] = iv;

	return Data;
}

bool CSecurePassword::FromVariant(const StVariant& Data, const CBuffer& SharedSecret)
{
	// Check if this is an encrypted variant (has IV field)
	if (!Data.Has(API_V_RAND))
		return FromVariant(Data); // Fall back to unencrypted

	CBuffer iv = Data[API_V_RAND].To<CBuffer>();
	if (iv.GetSize() != CEncryption::IV_SIZE)
		return false;

	CBuffer ciphertext = Data[API_V_DATA].To<CBuffer>();
	if (ciphertext.GetSize() == 0)
		return false;

	// Setup decryption
	CEncryption enc;
	if (!NT_SUCCESS(enc.SetSymetricKey(SharedSecret)))
		return false;
	if (!NT_SUCCESS(enc.InitCrypto()))
		return false;

	// Decrypt password
	CBuffer plaintext;
	if (!NT_SUCCESS(enc.Decrypt(ciphertext, plaintext, iv)))
		return false;

	// Set password from decrypted data
	if (plaintext.GetSize() == 0 || (plaintext.GetSize() % sizeof(wchar_t)) != 0)
		return false;

	return SetPassword((const wchar_t*)plaintext.GetBuffer(), plaintext.GetSize());
}

bool CSecurePassword::SetPassword(const wchar_t* pass, size_t size)
{
	DetachData();

	if (!pass || size == 0)
		return false;

	size_t charCount = size / sizeof(wchar_t);
	if (charCount < MIN_SEC_PASSWORD || charCount > MAX_SEC_PASSWORD)
		return false;

	m = AllocSecurePassword();
	if (!m)
		return false;
	m->Refs = 1;

	memcpy(m->pass, pass, size);
	m->pass[charCount] = L'\0';
	m->size = size;

	return true;
}

void CSecurePassword::ClearPassword()
{
	DetachData();
}

const wchar_t* CSecurePassword::GetPassword() const
{
	return m ? m->pass : nullptr;
}

size_t CSecurePassword::GetPasswordSize() const
{
	return m ? m->size : 0;
}
