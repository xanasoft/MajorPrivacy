#pragma once
#include "SharedCrypto.h"

class LIBRARY_EXPORT CPublicKey: public CCryptoBase
{
public:
#ifdef KERNEL_MODE
	CPublicKey(FW::AbstractMemPool* pMemPool);
#else
	CPublicKey(FW::AbstractMemPool* pMemPool = nullptr);
#endif
	virtual ~CPublicKey();

	NTSTATUS SetPublicKey(const CBuffer& PublicKey);
	NTSTATUS GetPublicKey(CBuffer& PublicKey) const;

	NTSTATUS InitCrypto();

	NTSTATUS Verify(const CBuffer& Data, const CBuffer& Signature) const;

protected:
	struct SPrivateCrypto* m;
};