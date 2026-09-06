#pragma once
#include "SharedCrypto.h"

#define CRYPTO_ECDH_ALGORITHM BCRYPT_ECDH_P256_ALGORITHM
#define CRYPTO_ECDH_ALGORITHM_BITS 256

class LIBRARY_EXPORT CKeyExchange : public CCryptoBase
{
public:
#ifdef KERNEL_MODE
	CKeyExchange(FW::AbstractMemPool* pMemPool);
#else
	CKeyExchange(FW::AbstractMemPool* pMemPool = nullptr);
#endif
	virtual ~CKeyExchange();

	NTSTATUS GenerateKeyPair();

	NTSTATUS SetPrivateKey(const CBuffer& PrivateKey);
	NTSTATUS GetPrivateKey(CBuffer& PrivateKey) const;

	NTSTATUS GetPublicKey(CBuffer& PublicKey) const;

	NTSTATUS DeriveSharedSecret(const CBuffer& OtherPublicKey, CBuffer& SharedSecret);

protected:
	NTSTATUS InitCrypto();

	struct SKeyExchangeCrypto* m;
};
