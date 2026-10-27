/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "pch.h"
#include "MokManager.h"
#include "EfiVars.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

//
// EFI_SIGNATURE_LIST, which is what every one of these variables contains:
//
//   GUID  SignatureType
//   UINT32 SignatureListSize      - the whole list, header included
//   UINT32 SignatureHeaderSize    - a per-type header, zero for everything here
//   UINT32 SignatureSize          - one entry, owner GUID included
//   ... SignatureHeaderSize bytes ...
//   ... entries, each: GUID SignatureOwner, then the data ...
//
#define SIGLIST_HEADER_SIZE		28
#define SIGLIST_OWNER_SIZE		16

static const GUID EFI_CERT_X509_GUID =
	{ 0xa5c059a1, 0x94e4, 0x4aa7, { 0x87, 0xb5, 0xab, 0x15, 0x5c, 0x2b, 0xf0, 0x72 } };
static const GUID EFI_CERT_SHA256_GUID =
	{ 0xc1c41626, 0x504c, 0x4092, { 0xac, 0xa9, 0x41, 0xf9, 0x36, 0x93, 0x43, 0x28 } };
static const GUID EFI_CERT_SHA1_GUID =
	{ 0x826ca512, 0xcf10, 0x4ac9, { 0xb1, 0x87, 0xbe, 0x01, 0x49, 0x66, 0x31, 0xbd } };
static const GUID EFI_CERT_SHA384_GUID =
	{ 0xff3e5307, 0x9fd0, 0x48c9, { 0x85, 0xf1, 0x8a, 0xd5, 0x6c, 0x70, 0x1e, 0x01 } };
static const GUID EFI_CERT_SHA512_GUID =
	{ 0x093e0fae, 0xa6c4, 0x4f50, { 0x9f, 0x1b, 0xd4, 0x1e, 0x2b, 0x89, 0xc1, 0x9a } };

// the owner shim stamps on entries enrolled through it
static const GUID SHIM_OWNER_GUID =
	{ 0x605dab50, 0xe046, 0x4300, { 0xab, 0xb6, 0x3d, 0xd8, 0x10, 0xdd, 0x8b, 0x23 } };

//
// MOK variables are non-volatile and must survive into runtime, or shim would
// not find them at the next boot.
//
#define MOK_VAR_ATTRIBUTES	(0x01 | 0x02 | 0x04)	// NV | BS | RT

#define SB_PASSWORD_MAX		16
#define MOK_SALT_SIZE		16
#define SHA512_DIGEST_SIZE	64
#define SHA512_CRYPT_ROUNDS	5000
#define PW_CRYPT_METHOD_SHA512	4
#define PW_CRYPT_SALT_MAX	32
#define PW_CRYPT_HASH_MAX	128

#pragma pack(push, 1)
struct SMokToggleVar
{
	unsigned long	State;
	unsigned long	PWLen;
	unsigned short	Password[SB_PASSWORD_MAX];
};

struct SMokPasswordCrypt
{
	unsigned short	method;
	uint64			iter_count;
	unsigned short	salt_size;
	unsigned char	salt[PW_CRYPT_SALT_MAX];
	unsigned char	hash[PW_CRYPT_HASH_MAX];
};

struct SMokTimeoutVar
{
	int				Timeout;
};
#pragma pack(pop)

/* ---------------------------------------------------------------------------
 * Hashing
 * ------------------------------------------------------------------------ */

//
// One BCrypt hash, opened and closed around a single digest. sha512-crypt runs
// twenty thousand of these; keeping the algorithm provider open across them all
// is what makes that take milliseconds rather than seconds.
//
class CBCryptHash
{
public:
	CBCryptHash(LPCWSTR Algorithm) : m_hAlg(NULL)
	{
		BCryptOpenAlgorithmProvider(&m_hAlg, Algorithm, NULL, 0);
	}
	~CBCryptHash()
	{
		if (m_hAlg) BCryptCloseAlgorithmProvider(m_hAlg, 0);
	}

	bool IsValid() const { return m_hAlg != NULL; }

	bool Begin()
	{
		if (!m_hAlg) return false;
		m_hHash = NULL;
		return BCryptCreateHash(m_hAlg, &m_hHash, NULL, 0, NULL, 0, 0) >= 0;
	}
	void Update(const void* pData, size_t uSize)
	{
		if (m_hHash && uSize) BCryptHashData(m_hHash, (PUCHAR)pData, (ULONG)uSize, 0);
	}
	bool Finish(unsigned char* pOut, size_t uSize)
	{
		if (!m_hHash) return false;
		bool bOk = BCryptFinishHash(m_hHash, pOut, (ULONG)uSize, 0) >= 0;
		BCryptDestroyHash(m_hHash);
		m_hHash = NULL;
		return bOk;
	}

protected:
	BCRYPT_ALG_HANDLE	m_hAlg;
	BCRYPT_HASH_HANDLE	m_hHash = NULL;
};

//
// The $6$ algorithm out of glibc's crypt(3), which is what shim's MokManager
// checks a password against. Reproduced step for step - the shape of it is
// strange but every part of it is load bearing, and a password hashed any other
// way simply will not be accepted at the boot screen.
//
static bool Sha512Crypt(const unsigned char* pPassword, size_t uPwLen,
						const unsigned char* pSalt, size_t uSaltLen,
						unsigned char* pOut)
{
	CBCryptHash Hash(BCRYPT_SHA512_ALGORITHM);
	if (!Hash.IsValid())
		return false;

	unsigned char DigestA[SHA512_DIGEST_SIZE];
	unsigned char DigestB[SHA512_DIGEST_SIZE];
	unsigned char PBytes[SHA512_DIGEST_SIZE];
	unsigned char SBytes[SHA512_DIGEST_SIZE];

	// B = H(password + salt + password)
	if (!Hash.Begin()) return false;
	Hash.Update(pPassword, uPwLen);
	Hash.Update(pSalt, uSaltLen);
	Hash.Update(pPassword, uPwLen);
	if (!Hash.Finish(DigestB, sizeof(DigestB))) return false;

	// A = H(password + salt + B repeated to the password's length + bits of the length)
	if (!Hash.Begin()) return false;
	Hash.Update(pPassword, uPwLen);
	Hash.Update(pSalt, uSaltLen);

	size_t i;
	for (i = uPwLen; i > SHA512_DIGEST_SIZE; i -= SHA512_DIGEST_SIZE)
		Hash.Update(DigestB, SHA512_DIGEST_SIZE);
	Hash.Update(DigestB, i);

	for (i = uPwLen; i > 0; i >>= 1) {
		if (i & 1) Hash.Update(DigestB, SHA512_DIGEST_SIZE);
		else       Hash.Update(pPassword, uPwLen);
	}
	if (!Hash.Finish(DigestA, sizeof(DigestA))) return false;

	// P = the password hashed once per character, stretched to its length
	if (!Hash.Begin()) return false;
	for (i = 0; i < uPwLen; i++)
		Hash.Update(pPassword, uPwLen);
	if (!Hash.Finish(PBytes, sizeof(PBytes))) return false;

	std::vector<unsigned char> PString(uPwLen);
	for (i = 0; i < uPwLen; i++)
		PString[i] = PBytes[i % SHA512_DIGEST_SIZE];

	// S = the salt hashed 16 + A[0] times, stretched to the salt's length
	if (!Hash.Begin()) return false;
	int iRounds = 16 + (int)DigestA[0];
	for (int j = 0; j < iRounds; j++)
		Hash.Update(pSalt, uSaltLen);
	if (!Hash.Finish(SBytes, sizeof(SBytes))) return false;

	std::vector<unsigned char> SString(uSaltLen);
	for (i = 0; i < uSaltLen; i++)
		SString[i] = SBytes[i % SHA512_DIGEST_SIZE];

	// and then five thousand rounds of stirring A together with both
	for (int r = 0; r < SHA512_CRYPT_ROUNDS; r++)
	{
		if (!Hash.Begin()) return false;

		if (r & 1) Hash.Update(PString.data(), PString.size());
		else       Hash.Update(DigestA, SHA512_DIGEST_SIZE);

		if (r % 3) Hash.Update(SString.data(), SString.size());
		if (r % 7) Hash.Update(PString.data(), PString.size());

		if (r & 1) Hash.Update(DigestA, SHA512_DIGEST_SIZE);
		else       Hash.Update(PString.data(), PString.size());

		if (!Hash.Finish(DigestA, sizeof(DigestA))) return false;
	}

	memcpy(pOut, DigestA, SHA512_DIGEST_SIZE);

	SecureZeroMemory(DigestB, sizeof(DigestB));
	SecureZeroMemory(PBytes, sizeof(PBytes));
	SecureZeroMemory(SBytes, sizeof(SBytes));
	if (!PString.empty()) SecureZeroMemory(PString.data(), PString.size());
	if (!SString.empty()) SecureZeroMemory(SString.data(), SString.size());

	return true;
}

/* ---------------------------------------------------------------------------
 * Signature lists
 * ------------------------------------------------------------------------ */

static unsigned long ReadU32(const std::vector<unsigned char>& Data, size_t uOffset)
{
	return (unsigned long)Data[uOffset]
		| ((unsigned long)Data[uOffset + 1] << 8)
		| ((unsigned long)Data[uOffset + 2] << 16)
		| ((unsigned long)Data[uOffset + 3] << 24);
}

static void WriteU32(std::vector<unsigned char>& Data, size_t uOffset, unsigned long uValue)
{
	for (int i = 0; i < 4; i++)
		Data[uOffset + i] = (unsigned char)((uValue >> (i * 8)) & 0xFF);
}

std::wstring SMokEntry::ToHex() const
{
	static const wchar_t* pDigits = L"0123456789ABCDEF";

	std::wstring Hex;
	Hex.reserve(Data.size() * 2);
	for (size_t i = 0; i < Data.size(); i++) {
		Hex += pDigits[Data[i] >> 4];
		Hex += pDigits[Data[i] & 0x0F];
	}
	return Hex;
}

//
// The subject line of a certificate, for the list. Without it a MOK list is a
// column of "X.509" and nothing else, which tells the user nothing about what
// their machine trusts.
//
static std::wstring GetCertSubject(const std::vector<unsigned char>& Cert)
{
	PCCERT_CONTEXT pContext = CertCreateCertificateContext(
		X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, Cert.data(), (DWORD)Cert.size());
	if (!pContext)
		return std::wstring();

	DWORD uLen = CertGetNameStringW(pContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0);

	std::wstring Name;
	if (uLen > 1) {
		Name.resize(uLen);
		CertGetNameStringW(pContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, &Name[0], uLen);
		Name.resize(uLen - 1);	// drop the terminator
	}

	CertFreeCertificateContext(pContext);
	return Name;
}

std::vector<SMokEntry> CMokManager::ParseSigList(const std::vector<unsigned char>& Data)
{
	std::vector<SMokEntry> Entries;

	size_t uOffset = 0;
	while (uOffset + SIGLIST_HEADER_SIZE <= Data.size())
	{
		GUID SigType;
		memcpy(&SigType, Data.data() + uOffset, sizeof(GUID));

		unsigned long uListSize = ReadU32(Data, uOffset + 16);
		unsigned long uHeaderSize = ReadU32(Data, uOffset + 20);
		unsigned long uSigSize = ReadU32(Data, uOffset + 24);

		//
		// Every one of these came out of firmware, so none of it is trusted to
		// be consistent. A list that claims to be longer than the buffer, or
		// shorter than its own header, ends the walk rather than being clamped.
		//
		if (uListSize < SIGLIST_HEADER_SIZE || uListSize > Data.size() - uOffset)
			break;

		bool bIsCert = false;
		int iHashSize = 0;
		const wchar_t* pTypeName = NULL;

		if (IsEqualGUID(SigType, EFI_CERT_X509_GUID))			{ bIsCert = true; pTypeName = L"X.509"; }
		else if (IsEqualGUID(SigType, EFI_CERT_SHA256_GUID))	{ iHashSize = 32; pTypeName = L"SHA-256"; }
		else if (IsEqualGUID(SigType, EFI_CERT_SHA1_GUID))		{ iHashSize = 20; pTypeName = L"SHA-1"; }
		else if (IsEqualGUID(SigType, EFI_CERT_SHA384_GUID))	{ iHashSize = 48; pTypeName = L"SHA-384"; }
		else if (IsEqualGUID(SigType, EFI_CERT_SHA512_GUID))	{ iHashSize = 64; pTypeName = L"SHA-512"; }
		else {
			uOffset += uListSize;	// a type we do not know; skip the list whole
			continue;
		}

		unsigned long uDataSize = uListSize - SIGLIST_HEADER_SIZE - uHeaderSize;
		if (uSigSize <= SIGLIST_OWNER_SIZE || uHeaderSize > uListSize - SIGLIST_HEADER_SIZE
			|| uSigSize > uDataSize || uDataSize % uSigSize) {
			uOffset += uListSize;
			continue;
		}

		size_t uSigAt = uOffset + SIGLIST_HEADER_SIZE + uHeaderSize;
		unsigned long uCount = uDataSize / uSigSize;

		for (unsigned long i = 0; i < uCount && uSigAt + uSigSize <= Data.size(); i++)
		{
			SMokEntry Entry;
			Entry.bIsCert = bIsCert;
			Entry.TypeName = pTypeName;

			const unsigned char* pStart = Data.data() + uSigAt + SIGLIST_OWNER_SIZE;
			Entry.Data.assign(pStart, pStart + (uSigSize - SIGLIST_OWNER_SIZE));

			if (bIsCert)
				Entry.Subject = GetCertSubject(Entry.Data);
			else if (Entry.Data.size() != (size_t)iHashSize) {
				// the list says one hash length and the entries another
				Entry.TypeName = L"(malformed)";
			}

			Entries.push_back(Entry);
			uSigAt += uSigSize;
		}

		uOffset += uListSize;
	}

	return Entries;
}

std::vector<unsigned char> CMokManager::BuildCertSigList(const std::vector<unsigned char>& Cert)
{
	unsigned long uSigSize = SIGLIST_OWNER_SIZE + (unsigned long)Cert.size();
	unsigned long uListSize = SIGLIST_HEADER_SIZE + uSigSize;

	std::vector<unsigned char> Data(uListSize, 0);
	memcpy(Data.data(), &EFI_CERT_X509_GUID, 16);
	WriteU32(Data, 16, uListSize);
	WriteU32(Data, 20, 0);			// no per-type header
	WriteU32(Data, 24, uSigSize);
	memcpy(Data.data() + SIGLIST_HEADER_SIZE, &SHIM_OWNER_GUID, 16);
	memcpy(Data.data() + SIGLIST_HEADER_SIZE + SIGLIST_OWNER_SIZE, Cert.data(), Cert.size());

	return Data;
}

std::vector<unsigned char> CMokManager::BuildHashSigList(const std::vector<unsigned char>& Hash)
{
	const GUID* pType = NULL;
	switch (Hash.size()) {
	case 20: pType = &EFI_CERT_SHA1_GUID; break;
	case 32: pType = &EFI_CERT_SHA256_GUID; break;
	case 48: pType = &EFI_CERT_SHA384_GUID; break;
	case 64: pType = &EFI_CERT_SHA512_GUID; break;
	default: return std::vector<unsigned char>();
	}

	unsigned long uSigSize = SIGLIST_OWNER_SIZE + (unsigned long)Hash.size();
	unsigned long uListSize = SIGLIST_HEADER_SIZE + uSigSize;

	std::vector<unsigned char> Data(uListSize, 0);
	memcpy(Data.data(), pType, 16);
	WriteU32(Data, 16, uListSize);
	WriteU32(Data, 20, 0);
	WriteU32(Data, 24, uSigSize);
	memcpy(Data.data() + SIGLIST_HEADER_SIZE, &SHIM_OWNER_GUID, 16);
	memcpy(Data.data() + SIGLIST_HEADER_SIZE + SIGLIST_OWNER_SIZE, Hash.data(), Hash.size());

	return Data;
}

/* ---------------------------------------------------------------------------
 * Requests
 * ------------------------------------------------------------------------ */

//
// What MokManager checks the typed password against: SHA-256 over the request
// followed by the password as UTF-16 with no terminator. Binding the request
// into the hash is what stops anything replacing MokNew after the fact and
// riding on an authorisation given for something else.
//
std::vector<unsigned char> CMokManager::BuildAuth(const std::vector<unsigned char>& Data, const CSecurePassword& Password)
{
	CBCryptHash Hash(BCRYPT_SHA256_ALGORITHM);
	if (!Hash.IsValid() || !Hash.Begin())
		return std::vector<unsigned char>();

	if (!Data.empty())
		Hash.Update(Data.data(), Data.size());
	if (!Password.IsEmpty())
		Hash.Update(Password.GetPassword(), Password.GetPasswordSize());

	std::vector<unsigned char> Auth(32);
	if (!Hash.Finish(Auth.data(), Auth.size()))
		return std::vector<unsigned char>();

	return Auth;
}

static STATUS SetMokVar(const std::wstring& Name, const std::vector<unsigned char>& Data)
{
	return CEfiVars::Set(Name, EFI_SHIM_VAR_GUID, Data, MOK_VAR_ATTRIBUTES);
}

static STATUS DelMokVar(const std::wstring& Name)
{
	return CEfiVars::Set(Name, EFI_SHIM_VAR_GUID, NULL, 0, MOK_VAR_ATTRIBUTES);
}

static std::vector<unsigned char> GetMokVar(const std::wstring& Name, bool* pExists = NULL)
{
	return CEfiVars::Get(Name, EFI_SHIM_VAR_GUID, pExists);
}

STATUS CMokManager::SetRequest(const std::wstring& DataVar, const std::wstring& AuthVar,
							   const std::vector<unsigned char>& SigList, const CSecurePassword& Password)
{
	if (SigList.empty())
		return ERR(STATUS_INVALID_PARAMETER, L"There is nothing to enroll.");

	STATUS Status = SetMokVar(DataVar, SigList);
	if (Status.IsError())
		return Status;

	std::vector<unsigned char> Auth = BuildAuth(SigList, Password);
	if (Auth.empty()) {
		DelMokVar(DataVar);
		return ERR(STATUS_UNSUCCESSFUL, L"Could not compute the authorization hash.");
	}

	Status = SetMokVar(AuthVar, Auth);
	if (Status.IsError()) {
		//
		// A request without its authorisation would sit there unusable, and
		// MokManager would prompt about it at every boot. Taken back out again.
		//
		DelMokVar(DataVar);
		return Status;
	}

	return OK;
}

/* ---------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------ */

SSecureBootState CMokManager::GetState()
{
	SSecureBootState State;

	State.bUefi = CEfiVars::IsUefiBoot();
	if (!State.bUefi)
		return State;

	State.bSecureBoot = CEfiVars::IsSecureBoot();
	State.bSetupMode = CEfiVars::IsSetupMode();

	bool bExists = false;
	std::vector<unsigned char> MokList = GetMokVar(L"MokListRT", &bExists);
	State.bShimPresent = bExists;
	State.iEnrolledCount = (int)ParseSigList(MokList).size();

	State.iDeniedCount = (int)ParseSigList(GetMokVar(L"MokListXRT")).size();

	std::vector<unsigned char> SbState = GetMokVar(L"MokSBStateRT", &bExists);
	State.iShimValidation = bExists && !SbState.empty() ? (SbState[0] == 1 ? 1 : 0) : -1;

	std::vector<unsigned char> DbState = GetMokVar(L"MokIgnoreDB", &bExists);
	State.iDbIgnored = bExists && !DbState.empty() ? (DbState[0] == 1 ? 1 : 0) : -1;

	//
	// A password is set when MokPW holds something that is not all zeroes -
	// clearing it writes a zeroed structure rather than deleting the variable.
	//
	std::vector<unsigned char> Pw = GetMokVar(L"MokPW");
	for (size_t i = 0; i < Pw.size(); i++) {
		if (Pw[i]) { State.bPasswordSet = true; break; }
	}

	//
	// What is waiting for the next boot. An auth with no data is a reset
	// request, which is how MokManager tells the two apart - so the order of
	// these tests matters.
	//
	bool bNew = CEfiVars::Exists(L"MokNew", EFI_SHIM_VAR_GUID);
	bool bAuth = CEfiVars::Exists(L"MokAuth", EFI_SHIM_VAR_GUID);
	State.bPendingEnroll = bNew && bAuth;
	State.bPendingReset = !bNew && bAuth;
	State.bPendingDelete = CEfiVars::Exists(L"MokDel", EFI_SHIM_VAR_GUID);

	State.bPendingEnrollDeny = CEfiVars::Exists(L"MokXNew", EFI_SHIM_VAR_GUID);
	State.bPendingDeleteDeny = CEfiVars::Exists(L"MokXDel", EFI_SHIM_VAR_GUID);

	return State;
}

std::vector<SMokEntry> CMokManager::List(EMokList List)
{
	return ParseSigList(GetMokVar(List == eMokDeny ? L"MokListXRT" : L"MokListRT"));
}

std::vector<SMokEntry> CMokManager::ListFirmwareDb()
{
	// db and dbx live in the security namespace, not shim's
	return ParseSigList(CEfiVars::Get(L"db", EFI_SECURITY_VAR_GUID));
}

std::vector<SMokEntry> CMokManager::ListFirmwareDbx()
{
	return ParseSigList(CEfiVars::Get(L"dbx", EFI_SECURITY_VAR_GUID));
}

//
// PK and KEK are not in the image security namespace with db and dbx - they
// are global variables, which is the one thing easy to get wrong here.
//
std::vector<SMokEntry> CMokManager::ListFirmwarePk()
{
	return ParseSigList(CEfiVars::Get(L"PK", EFI_GLOBAL_VAR_GUID));
}

std::vector<SMokEntry> CMokManager::ListFirmwareKek()
{
	return ParseSigList(CEfiVars::Get(L"KEK", EFI_GLOBAL_VAR_GUID));
}

/* ---------------------------------------------------------------------------
 * Certificates
 * ------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------------
 * Reading a certificate
 * ------------------------------------------------------------------------ */

static std::wstring NameToString(PCERT_NAME_BLOB pName, DWORD uFlags)
{
	DWORD uLen = CertNameToStrW(X509_ASN_ENCODING, pName, uFlags, NULL, 0);
	if (uLen <= 1)
		return std::wstring();

	std::wstring Text;
	Text.resize(uLen);
	CertNameToStrW(X509_ASN_ENCODING, pName, uFlags, &Text[0], uLen);

	// CertNameToStr counts the terminator, std::wstring does not
	if (!Text.empty() && Text.back() == L'\0')
		Text.pop_back();

	return Text;
}

static std::wstring BytesToHex(const unsigned char* pData, size_t uSize, bool bReverse = false)
{
	static const wchar_t Digits[] = L"0123456789abcdef";

	std::wstring Hex;
	Hex.reserve(uSize * 2);

	for (size_t i = 0; i < uSize; i++) {
		unsigned char c = pData[bReverse ? uSize - 1 - i : i];
		Hex += Digits[c >> 4];
		Hex += Digits[c & 0x0F];
	}

	return Hex;
}

static std::wstring FileTimeToString(const FILETIME& Time)
{
	SYSTEMTIME st = { 0 };
	if (!FileTimeToSystemTime(&Time, &st))
		return std::wstring();

	wchar_t Text[64];
	swprintf_s(Text, L"%04u-%02u-%02u %02u:%02u:%02u UTC",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	return Text;
}

//
// The readable name of an OID, falling back to the OID itself. An algorithm
// nothing has a name for is still worth showing - it is the interesting case.
//
static std::wstring OidName(const char* pszOid, DWORD uGroup)
{
	if (!pszOid || !*pszOid)
		return std::wstring();

	PCCRYPT_OID_INFO pInfo = CryptFindOIDInfo(CRYPT_OID_INFO_OID_KEY, (void*)pszOid, uGroup);
	if (pInfo && pInfo->pwszName)
		return std::wstring(pInfo->pwszName) + L" (" + std::wstring(pszOid, pszOid + strlen(pszOid)) + L")";

	return std::wstring(pszOid, pszOid + strlen(pszOid));
}

static std::wstring HashHex(const std::vector<unsigned char>& Data, LPCWSTR Algorithm, size_t uSize)
{
	CBCryptHash Hash(Algorithm);
	if (!Hash.IsValid() || !Hash.Begin())
		return std::wstring();

	Hash.Update(Data.data(), Data.size());

	std::vector<unsigned char> Digest(uSize);
	if (!Hash.Finish(Digest.data(), Digest.size()))
		return std::wstring();

	return BytesToHex(Digest.data(), Digest.size());
}

RESULT(SMokCertInfo) CMokManager::GetCertInfo(const std::vector<unsigned char>& Cert)
{
	if (Cert.empty())
		return ERR(STATUS_INVALID_PARAMETER, L"There is no certificate to read.");

	PCCERT_CONTEXT pContext = CertCreateCertificateContext(
		X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, Cert.data(), (DWORD)Cert.size());
	if (!pContext)
		return ERR(STATUS_INVALID_IMAGE_FORMAT, L"This is not a readable X.509 certificate.");

	SMokCertInfo Info;
	const CERT_INFO* pInfo = pContext->pCertInfo;

	Info.iVersion = (int)pInfo->dwVersion + 1;		// dwVersion 0 is "v1"

	Info.Subject = NameToString((PCERT_NAME_BLOB)&pInfo->Subject,
		CERT_X500_NAME_STR | CERT_NAME_STR_CRLF_FLAG);
	Info.Issuer = NameToString((PCERT_NAME_BLOB)&pInfo->Issuer,
		CERT_X500_NAME_STR | CERT_NAME_STR_CRLF_FLAG);

	Info.bSelfSigned = !Info.Subject.empty() && Info.Subject == Info.Issuer;

	DWORD uLen = CertGetNameStringW(pContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0);
	if (uLen > 1) {
		Info.SubjectName.resize(uLen);
		CertGetNameStringW(pContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, &Info.SubjectName[0], uLen);
		Info.SubjectName.resize(wcslen(Info.SubjectName.c_str()));
	}

	uLen = CertGetNameStringW(pContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, NULL, NULL, 0);
	if (uLen > 1) {
		Info.IssuerName.resize(uLen);
		CertGetNameStringW(pContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, NULL, &Info.IssuerName[0], uLen);
		Info.IssuerName.resize(wcslen(Info.IssuerName.c_str()));
	}

	// the serial arrives least significant byte first
	Info.Serial = BytesToHex(pInfo->SerialNumber.pbData, pInfo->SerialNumber.cbData, true);

	Info.NotBefore = FileTimeToString(pInfo->NotBefore);
	Info.NotAfter = FileTimeToString(pInfo->NotAfter);

	Info.SignatureAlgorithm = OidName(pInfo->SignatureAlgorithm.pszObjId, CRYPT_SIGN_ALG_OID_GROUP_ID);
	Info.PublicKeyAlgorithm = OidName(pInfo->SubjectPublicKeyInfo.Algorithm.pszObjId, CRYPT_PUBKEY_ALG_OID_GROUP_ID);
	Info.iPublicKeyBits = (int)CertGetPublicKeyLength(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
		(PCERT_PUBLIC_KEY_INFO)&pInfo->SubjectPublicKeyInfo);

	CertFreeCertificateContext(pContext);

	//
	// Thumbprints over the DER as it was handed in - which is what every other
	// tool prints, and what makes it possible to match this against one.
	//
	Info.Sha1 = HashHex(Cert, BCRYPT_SHA1_ALGORITHM, 20);
	Info.Sha256 = HashHex(Cert, BCRYPT_SHA256_ALGORITHM, 32);

	return CResult<SMokCertInfo>(0, Info);
}

RESULT(std::vector<unsigned char>) CMokManager::LoadCertFile(const std::wstring& Path)
{
	HANDLE hFile = CreateFileW(Path.c_str(), GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return ERR(STATUS_NOT_FOUND, L"Could not open " + Path + L".");

	LARGE_INTEGER Size = { 0 };
	if (!GetFileSizeEx(hFile, &Size) || Size.QuadPart == 0 || Size.QuadPart > 1024 * 1024) {
		CloseHandle(hFile);
		return ERR(STATUS_FILE_TOO_LARGE, Path + L" is empty or far too large to be a certificate.");
	}

	std::vector<unsigned char> Data((size_t)Size.QuadPart);
	DWORD uRead = 0;
	BOOL bOk = ReadFile(hFile, Data.data(), (DWORD)Data.size(), &uRead, NULL);
	CloseHandle(hFile);

	if (!bOk || uRead != Data.size())
		return ERR(STATUS_UNSUCCESSFUL, L"Could not read " + Path + L".");

	//
	// PEM is base64 between two markers; DER is the raw thing. Told apart by
	// looking for the marker rather than by the file extension, which is not
	// reliably either one.
	//
	std::string Text((const char*)Data.data(), Data.size() < 4096 ? Data.size() : 4096);
	if (Text.find("-----BEGIN CERTIFICATE-----") != std::string::npos)
	{
		DWORD uBinSize = 0;
		if (!CryptStringToBinaryA((LPCSTR)Data.data(), (DWORD)Data.size(),
			CRYPT_STRING_BASE64HEADER, NULL, &uBinSize, NULL, NULL))
			return ERR(STATUS_UNSUCCESSFUL, Path + L" looks like PEM but could not be decoded.");

		std::vector<unsigned char> Binary(uBinSize);
		if (!CryptStringToBinaryA((LPCSTR)Data.data(), (DWORD)Data.size(),
			CRYPT_STRING_BASE64HEADER, Binary.data(), &uBinSize, NULL, NULL))
			return ERR(STATUS_UNSUCCESSFUL, Path + L" looks like PEM but could not be decoded.");

		Binary.resize(uBinSize);
		Data = Binary;
	}

	//
	// Whatever it is now, it has to parse as a certificate. Enrolling something
	// that is not one would leave a MOK entry nothing can ever match.
	//
	PCCERT_CONTEXT pContext = CertCreateCertificateContext(
		X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, Data.data(), (DWORD)Data.size());
	if (!pContext)
		return ERR(STATUS_INVALID_PARAMETER, Path + L" is not an X.509 certificate.");
	CertFreeCertificateContext(pContext);

	return CResult<std::vector<unsigned char>>(0, Data);
}

/* ---------------------------------------------------------------------------
 * Enrolment
 * ------------------------------------------------------------------------ */

STATUS CMokManager::EnrollCert(const std::vector<unsigned char>& Cert, const CSecurePassword& Password, EMokList List)
{
	return SetRequest(List == eMokDeny ? L"MokXNew" : L"MokNew",
					  List == eMokDeny ? L"MokXAuth" : L"MokAuth",
					  BuildCertSigList(Cert), Password);
}

STATUS CMokManager::EnrollCertFiles(const std::vector<std::wstring>& Files, const CSecurePassword& Password, EMokList List)
{
	if (Files.empty())
		return ERR(STATUS_INVALID_PARAMETER, L"No certificate was given.");

	//
	// Every file is read and checked before anything is written, so that a bad
	// one in the middle of a list does not leave half an enrolment queued.
	//
	std::vector<std::vector<unsigned char>> Certs;
	for (size_t i = 0; i < Files.size(); i++)
	{
		RESULT(std::vector<unsigned char>) CertRes = LoadCertFile(Files[i]);
		if (CertRes.IsError())
			return CertRes;

		Certs.push_back(CertRes.GetValue());
	}

	return EnrollCerts(Certs, Password, List);
}

//
// The same thing for certificates that are already in hand - one read out of the
// shipped package, say, which has no file name to give.
//
STATUS CMokManager::EnrollCerts(const std::vector<std::vector<unsigned char>>& Certs, const CSecurePassword& Password, EMokList List)
{
	if (Certs.empty())
		return ERR(STATUS_INVALID_PARAMETER, L"No certificate was given.");

	std::vector<unsigned char> Combined;
	for (size_t i = 0; i < Certs.size(); i++)
	{
		std::vector<unsigned char> SigList = BuildCertSigList(Certs[i]);
		if (SigList.empty())
			return ERR(STATUS_INVALID_PARAMETER, L"A certificate could not be read.");

		Combined.insert(Combined.end(), SigList.begin(), SigList.end());
	}

	return SetRequest(List == eMokDeny ? L"MokXNew" : L"MokNew",
					  List == eMokDeny ? L"MokXAuth" : L"MokAuth",
					  Combined, Password);
}

STATUS CMokManager::EnrollHash(const std::vector<unsigned char>& Hash, const CSecurePassword& Password, EMokList List)
{
	std::vector<unsigned char> SigList = BuildHashSigList(Hash);
	if (SigList.empty())
		return ERR(STATUS_INVALID_PARAMETER, L"A hash must be 20, 32, 48 or 64 bytes long.");

	return SetRequest(List == eMokDeny ? L"MokXNew" : L"MokNew",
					  List == eMokDeny ? L"MokXAuth" : L"MokAuth",
					  SigList, Password);
}

STATUS CMokManager::DeleteCert(const std::vector<unsigned char>& Cert, const CSecurePassword& Password, EMokList List)
{
	return SetRequest(List == eMokDeny ? L"MokXDel" : L"MokDel",
					  List == eMokDeny ? L"MokXDelAuth" : L"MokDelAuth",
					  BuildCertSigList(Cert), Password);
}

//
// The same request with a hash signature list rather than a certificate one -
// MokDel takes whatever MokNew took, so a list entry can be removed whichever
// of the two it is.
//
STATUS CMokManager::DeleteHash(const std::vector<unsigned char>& Hash, const CSecurePassword& Password, EMokList List)
{
	std::vector<unsigned char> SigList = BuildHashSigList(Hash);
	if (SigList.empty())
		return ERR(STATUS_INVALID_PARAMETER, L"A hash must be 20, 32, 48 or 64 bytes long.");

	return SetRequest(List == eMokDeny ? L"MokXDel" : L"MokDel",
					  List == eMokDeny ? L"MokXDelAuth" : L"MokDelAuth",
					  SigList, Password);
}

STATUS CMokManager::DeleteCertFiles(const std::vector<std::wstring>& Files, const CSecurePassword& Password, EMokList List)
{
	if (Files.empty())
		return ERR(STATUS_INVALID_PARAMETER, L"No certificate was given.");

	std::vector<unsigned char> Combined;
	for (size_t i = 0; i < Files.size(); i++)
	{
		RESULT(std::vector<unsigned char>) CertRes = LoadCertFile(Files[i]);
		if (CertRes.IsError())
			return CertRes;

		std::vector<unsigned char> SigList = BuildCertSigList(CertRes.GetValue());
		Combined.insert(Combined.end(), SigList.begin(), SigList.end());
	}

	return SetRequest(List == eMokDeny ? L"MokXDel" : L"MokDel",
					  List == eMokDeny ? L"MokXDelAuth" : L"MokDelAuth",
					  Combined, Password);
}

STATUS CMokManager::RevokeEnroll(EMokList List)
{
	DelMokVar(List == eMokDeny ? L"MokXNew" : L"MokNew");
	DelMokVar(List == eMokDeny ? L"MokXAuth" : L"MokAuth");
	return OK;
}

STATUS CMokManager::RevokeDelete(EMokList List)
{
	DelMokVar(List == eMokDeny ? L"MokXDel" : L"MokDel");
	DelMokVar(List == eMokDeny ? L"MokXDelAuth" : L"MokDelAuth");
	return OK;
}

STATUS CMokManager::Reset(const CSecurePassword& Password, EMokList List)
{
	std::wstring NewVar = List == eMokDeny ? L"MokXNew" : L"MokNew";
	std::wstring AuthVar = List == eMokDeny ? L"MokXAuth" : L"MokAuth";

	//
	// An authorisation with nothing to enrol. MokManager reads that as "empty
	// the list" and offers Reset MOK rather than Enroll MOK, so the data
	// variable has to be gone before the auth is written.
	//
	DelMokVar(NewVar);

	std::vector<unsigned char> Auth = BuildAuth(std::vector<unsigned char>(), Password);
	if (Auth.empty())
		return ERR(STATUS_UNSUCCESSFUL, L"Could not compute the authorization hash.");

	STATUS Status = SetMokVar(AuthVar, Auth);
	if (Status.IsError())
		DelMokVar(AuthVar);
	return Status;
}

bool CMokManager::IsCertEnrolled(const std::vector<unsigned char>& Cert)
{
	std::vector<SMokEntry> Entries = List(eMokAllow);
	for (size_t i = 0; i < Entries.size(); i++) {
		if (Entries[i].bIsCert && Entries[i].Data == Cert)
			return true;
	}
	return false;
}

/* ---------------------------------------------------------------------------
 * Settings
 * ------------------------------------------------------------------------ */

STATUS CMokManager::SetPassword(const CSecurePassword& Password)
{
	unsigned char Salt[MOK_SALT_SIZE];
	if (BCryptGenRandom(NULL, Salt, sizeof(Salt), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
		return ERR(STATUS_UNSUCCESSFUL, L"Could not generate a salt.");

	//
	// Hashed over the UTF-8 form. That is what MokManager does with what is
	// typed at the boot screen, so anything else would never match.
	//
	char Utf8[512] = { 0 };
	int iLen = Password.IsEmpty() ? 0 : WideCharToMultiByte(CP_UTF8, 0,
		Password.GetPassword(), (int)Password.GetPasswordLength(),
		Utf8, sizeof(Utf8), NULL, NULL);
	if (iLen <= 0)
		return ERR(STATUS_INVALID_PARAMETER, L"The password could not be encoded.");

	unsigned char Hash[SHA512_DIGEST_SIZE];
	bool bOk = Sha512Crypt((const unsigned char*)Utf8, iLen, Salt, sizeof(Salt), Hash);
	SecureZeroMemory(Utf8, sizeof(Utf8));

	if (!bOk)
		return ERR(STATUS_UNSUCCESSFUL, L"Could not hash the password.");

	SMokPasswordCrypt Pw;
	memset(&Pw, 0, sizeof(Pw));
	Pw.method = PW_CRYPT_METHOD_SHA512;
	Pw.iter_count = SHA512_CRYPT_ROUNDS;
	Pw.salt_size = MOK_SALT_SIZE;
	memcpy(Pw.salt, Salt, MOK_SALT_SIZE);
	memcpy(Pw.hash, Hash, SHA512_DIGEST_SIZE);

	std::vector<unsigned char> Data((unsigned char*)&Pw, (unsigned char*)&Pw + sizeof(Pw));
	STATUS Status = SetMokVar(L"MokPW", Data);

	SecureZeroMemory(&Pw, sizeof(Pw));
	SecureZeroMemory(Hash, sizeof(Hash));
	SecureZeroMemory(Salt, sizeof(Salt));
	if (!Data.empty()) SecureZeroMemory(Data.data(), Data.size());

	return Status;
}

STATUS CMokManager::ClearPassword()
{
	// a zeroed structure, not a deleted variable - that is what shim reads as
	// "no password"
	SMokPasswordCrypt Pw;
	memset(&Pw, 0, sizeof(Pw));

	std::vector<unsigned char> Data((unsigned char*)&Pw, (unsigned char*)&Pw + sizeof(Pw));
	return SetMokVar(L"MokPW", Data);
}

STATUS CMokManager::SetToggle(const std::wstring& Name, unsigned long uState, const CSecurePassword& Password)
{
	SMokToggleVar Var;
	memset(&Var, 0, sizeof(Var));

	Var.State = uState;
	size_t uLen = Password.GetPasswordLength();
	Var.PWLen = (unsigned long)(uLen < SB_PASSWORD_MAX ? uLen : SB_PASSWORD_MAX);
	const wchar_t* pPassword = Password.GetPassword();
	for (unsigned long i = 0; i < Var.PWLen; i++)
		Var.Password[i] = (unsigned short)pPassword[i];

	std::vector<unsigned char> Data((unsigned char*)&Var, (unsigned char*)&Var + sizeof(Var));
	STATUS Status = SetMokVar(Name, Data);

	SecureZeroMemory(&Var, sizeof(Var));
	if (!Data.empty()) SecureZeroMemory(Data.data(), Data.size());

	return Status;
}

STATUS CMokManager::SetShimValidation(bool bEnable, const CSecurePassword& Password)
{
	//
	// The variable is a request to turn validation OFF, not a state: mokutil
	// --disable-validation writes 1, --enable-validation writes 0, and shim
	// reflects what it did in MokSBStateRT, where 1 likewise means disabled.
	// So the value is the opposite of what is being asked for here.
	//
	return SetToggle(L"MokSB", bEnable ? 0 : 1, Password);
}

STATUS CMokManager::SetDbUsage(bool bUseDb, const CSecurePassword& Password)
{
	// the same inversion - 1 is "ignore db", which is what MokIgnoreDB reports
	return SetToggle(L"MokDB", bUseDb ? 0 : 1, Password);
}

STATUS CMokManager::SetTimeout(int iSeconds)
{
	SMokTimeoutVar Var;
	Var.Timeout = iSeconds;

	std::vector<unsigned char> Data((unsigned char*)&Var, (unsigned char*)&Var + sizeof(Var));
	return SetMokVar(L"MokTimeout", Data);
}

STATUS CMokManager::SetVerbose(bool bVerbose)
{
	unsigned char Value = bVerbose ? 1 : 0;
	return CEfiVars::Set(L"SHIM_VERBOSE", EFI_SHIM_VAR_GUID, &Value, sizeof(Value), MOK_VAR_ATTRIBUTES);
}

/* ---------------------------------------------------------------------------
 * Export
 * ------------------------------------------------------------------------ */

RESULT(int) CMokManager::Export(const std::wstring& Directory, EMokList List)
{
	std::vector<SMokEntry> Entries = List == eMokDeny ? CMokManager::List(eMokDeny) : CMokManager::List(eMokAllow);
	if (Entries.empty())
		return ERR(STATUS_NOT_FOUND, L"That list is empty.");

	if (!CreateDirectoryW(Directory.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		return ERR(STATUS_ACCESS_DENIED, L"Could not create " + Directory + L".");

	int iWritten = 0;
	for (size_t i = 0; i < Entries.size(); i++)
	{
		wchar_t Name[64];
		swprintf_s(Name, L"\\MOK_%04d%s", (int)i, Entries[i].bIsCert ? L".der" : L".hash");

		std::wstring Path = Directory + Name;

		HANDLE hFile = CreateFileW(Path.c_str(), GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
			return ERR(STATUS_ACCESS_DENIED, L"Could not write " + Path + L".");

		DWORD uWritten = 0;
		BOOL bOk = WriteFile(hFile, Entries[i].Data.data(), (DWORD)Entries[i].Data.size(), &uWritten, NULL);
		CloseHandle(hFile);

		if (!bOk || uWritten != Entries[i].Data.size())
			return ERR(STATUS_UNSUCCESSFUL, L"Could not write " + Path + L".");

		iWritten++;
	}

	return CResult<int>(0, iWritten);
}

/* ---------------------------------------------------------------------------
 * Secure Boot db trust - ported from DiskCryptor dcapi/sb_sup.c
 * ------------------------------------------------------------------------ */

//
// The SHA-1 thumbprints of every X.509 certificate the firmware trusts in db.
// A hash-type db entry (EFI_CERT_SHA256 and the like) is not a signer and is
// skipped here - this list is only used to recognise a signer certificate.
//
std::vector<std::vector<unsigned char>> CMokManager::ListDbCertThumbprints()
{
	std::vector<std::vector<unsigned char>> Thumbs;

	std::vector<SMokEntry> Db = ListFirmwareDb();
	for (size_t i = 0; i < Db.size(); i++)
	{
		if (!Db[i].bIsCert || Db[i].Data.empty())
			continue;

		PCCERT_CONTEXT pCtx = CertCreateCertificateContext(
			X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
			Db[i].Data.data(), (DWORD)Db[i].Data.size());
		if (!pCtx)
			continue;

		unsigned char Sha1[20];
		DWORD uLen = sizeof(Sha1);
		if (CertGetCertificateContextProperty(pCtx, CERT_SHA1_HASH_PROP_ID, Sha1, &uLen) && uLen == 20)
			Thumbs.push_back(std::vector<unsigned char>(Sha1, Sha1 + 20));

		CertFreeCertificateContext(pCtx);
	}

	return Thumbs;
}

// Does a 20-byte SHA-1 thumbprint appear in the db list?
static bool CMokManager__ThumbInDb(const std::vector<std::vector<unsigned char>>& Db, const unsigned char Sha1[20])
{
	for (size_t i = 0; i < Db.size(); i++)
		if (Db[i].size() == 20 && memcmp(Db[i].data(), Sha1, 20) == 0)
			return true;
	return false;
}

//
// Whether the PE's Authenticode signer chains up to a certificate in db. We take
// the real signer certificate (the one named in the PKCS#7 signer info), then build
// its certificate chain using the certificates embedded in the signature as extra
// material, and check every element of that chain against db:
//   - a self-rolled db signs the loader directly, so the signer leaf is in db;
//   - a Microsoft-signed shim's leaf is not in db, but the intermediate CA it chains
//     to (e.g. "Microsoft Corporation UEFI CA 2011") is - which is exactly what the
//     firmware trusts, and why the shim boots.
// The chain builder links certificates cryptographically (issuer + signature), so a
// db certificate merely carried alongside an unrelated signature does not count.
//
//
// The shared half: the signature is already open, decide whether it chains to db.
//
static bool IsSignerInDb(HCERTSTORE hStore, HCRYPTMSG hMsg,
						 const std::vector<std::vector<unsigned char>>& DbThumbprints);

bool CMokManager::IsFileTrustedByDb(const std::wstring& Path, const std::vector<std::vector<unsigned char>>& DbThumbprints)
{
	if (DbThumbprints.empty())
		return false;

	HCERTSTORE hStore = NULL;
	HCRYPTMSG  hMsg = NULL;
	DWORD uEncoding = 0, uContentType = 0, uFormatType = 0;

	if (!CryptQueryObject(
			CERT_QUERY_OBJECT_FILE,
			Path.c_str(),
			CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
			CERT_QUERY_FORMAT_FLAG_BINARY,
			0,
			&uEncoding, &uContentType, &uFormatType,
			&hStore, &hMsg, NULL))
		return false;

	bool bResult = IsSignerInDb(hStore, hMsg, DbThumbprints);
	if (hMsg) CryptMsgClose(hMsg);
	if (hStore) CertCloseStore(hStore, 0);
	return bResult;
}

//
// The same, for an image held in memory. CryptQueryObject reads an embedded
// Authenticode signature out of a blob exactly as it does out of a file - same
// content type, same signer - so a package file can be judged where it is,
// without being written out anywhere first.
//
bool CMokManager::IsDataTrustedByDb(const std::vector<unsigned char>& Image,
									const std::vector<std::vector<unsigned char>>& DbThumbprints)
{
	if (DbThumbprints.empty() || Image.empty())
		return false;

	CRYPT_DATA_BLOB Blob;
	Blob.cbData = (DWORD)Image.size();
	Blob.pbData = const_cast<BYTE*>(Image.data());

	HCERTSTORE hStore = NULL;
	HCRYPTMSG  hMsg = NULL;
	DWORD uEncoding = 0, uContentType = 0, uFormatType = 0;

	if (!CryptQueryObject(
			CERT_QUERY_OBJECT_BLOB,
			&Blob,
			CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
			CERT_QUERY_FORMAT_FLAG_BINARY,
			0,
			&uEncoding, &uContentType, &uFormatType,
			&hStore, &hMsg, NULL))
		return false;

	bool bResult = IsSignerInDb(hStore, hMsg, DbThumbprints);
	if (hMsg) CryptMsgClose(hMsg);
	if (hStore) CertCloseStore(hStore, 0);
	return bResult;
}

static bool IsSignerInDb(HCERTSTORE hStore, HCRYPTMSG hMsg,
						 const std::vector<std::vector<unsigned char>>& DbThumbprints)
{

	bool bTrusted = false;
	PCCERT_CONTEXT pSigner = NULL;

	do {
		DWORD cb = 0;
		if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, NULL, &cb) || cb == 0)
			break;

		std::vector<unsigned char> Signer(cb);
		if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, Signer.data(), &cb))
			break;

		CMSG_SIGNER_INFO* pSignerInfo = (CMSG_SIGNER_INFO*)Signer.data();

		CERT_INFO CertInfo = { 0 };
		CertInfo.Issuer = pSignerInfo->Issuer;
		CertInfo.SerialNumber = pSignerInfo->SerialNumber;

		pSigner = CertFindCertificateInStore(
			hStore,
			X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
			0,
			CERT_FIND_SUBJECT_CERT,
			&CertInfo,
			NULL);
		if (!pSigner)
			break;

		//
		// Build the chain from the real signer, offering the signature's own
		// certificates as extra material. We do not need it to reach a trusted root -
		// only the certificates it is built from, to match against db - so revocation
		// and auth-root auto-update (both of which can touch the network) are off.
		//
		CERT_CHAIN_PARA ChainPara = { 0 };
		ChainPara.cbSize = sizeof(ChainPara);

		PCCERT_CHAIN_CONTEXT pChain = NULL;
		if (CertGetCertificateChain(NULL, pSigner, NULL, hStore, &ChainPara,
				CERT_CHAIN_CACHE_END_CERT | CERT_CHAIN_DISABLE_AUTH_ROOT_AUTO_UPDATE | CERT_CHAIN_DISABLE_AIA,
				NULL, &pChain)
			&& pChain && pChain->cChain > 0)
		{
			PCERT_SIMPLE_CHAIN pSimple = pChain->rgpChain[0];
			for (DWORD i = 0; i < pSimple->cElement && !bTrusted; i++)
			{
				unsigned char Sha1[20];
				DWORD uLen = sizeof(Sha1);
				if (CertGetCertificateContextProperty(pSimple->rgpElement[i]->pCertContext,
						CERT_SHA1_HASH_PROP_ID, Sha1, &uLen) && uLen == 20)
					bTrusted = CMokManager__ThumbInDb(DbThumbprints, Sha1);
			}
			CertFreeCertificateChain(pChain);
		}
		else
		{
			//
			// No chain could be built (e.g. only the leaf is present). Fall back to
			// matching the signer leaf itself - the self-rolled-db case.
			//
			unsigned char Sha1[20];
			DWORD uLen = sizeof(Sha1);
			if (CertGetCertificateContextProperty(pSigner, CERT_SHA1_HASH_PROP_ID, Sha1, &uLen) && uLen == 20)
				bTrusted = CMokManager__ThumbInDb(DbThumbprints, Sha1);
		}
	} while (0);

	if (pSigner) CertFreeCertificateContext(pSigner);

	// the store and the message stay open - whoever queried for them closes them
	return bTrusted;
}
