#pragma once
#include "../lib_global.h"
#include "SharedCrypto.h"
#include "SecurePassword.h"

class LIBRARY_EXPORT CEncryption : public CCryptoBase
{
public:
#ifdef KERNEL_MODE
	CEncryption(FW::AbstractMemPool* pMemPool);
#else
	CEncryption(FW::AbstractMemPool* pMemPool = nullptr);
#endif
	virtual ~CEncryption();

	static NTSTATUS GetKeyFromPW(const CBuffer& password, const CBuffer& salt, CBuffer& key, int iKdf);
	static NTSTATUS GetKeyFromPWOld(const CBuffer& password, const CBuffer& salt, CBuffer& key, ULONG Iterations = 1024);
	NTSTATUS SetPassword(const CSecurePassword& password, const CBuffer& salt, int iKdf);

	NTSTATUS SetSymetricKey(const CBuffer& SymetricKey);
	NTSTATUS GetSymetricKey(CBuffer& SymetricKey) const;

	NTSTATUS InitCrypto();

	NTSTATUS Encrypt(const CBuffer& in, CBuffer& out, const CBuffer& iv = CBuffer());
	NTSTATUS Decrypt(const CBuffer& in, CBuffer& out, const CBuffer& iv = CBuffer());

	static const DWORD IV_SIZE = 128 / 8; // AES block size

protected:
	struct SPrivateCrypto* m;
};