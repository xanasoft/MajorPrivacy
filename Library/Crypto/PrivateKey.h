#pragma once
#include "SharedCrypto.h"

class LIBRARY_EXPORT CPrivateKey: public CCryptoBase
{
public:
#ifdef KERNEL_MODE
	CPrivateKey(FW::AbstractMemPool* pMemPool);
#else
	CPrivateKey(FW::AbstractMemPool* pMemPool = nullptr);
#endif
	virtual ~CPrivateKey();

	NTSTATUS MakeKeyPair(class CPublicKey& PubKey);

	NTSTATUS SetPrivateKey(const CBuffer& PrivateKey);
	bool IsPrivateKeySet() const;
	NTSTATUS GetPrivateKey(CBuffer& PrivateKey) const;

	NTSTATUS InitCrypto();

	NTSTATUS Sign(const CBuffer& Data, CBuffer& Signature) const;

protected:
	struct SPrivateCrypto* m;
};