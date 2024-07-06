#pragma once
#include "SharedCrypto.h"

class LIBRARY_EXPORT CHashFunction: public CCryptoBase
{
public:
#ifdef KERNEL_MODE
	CHashFunction(FW::AbstractMemPool* pMemPool);
#else
	CHashFunction(FW::AbstractMemPool* pMemPool = nullptr);
#endif
	virtual ~CHashFunction();

	NTSTATUS InitHash();
	NTSTATUS ResetHash();
	NTSTATUS UpdateHash(const CBuffer& buffer);
	NTSTATUS FinalizeHash(CBuffer& hash);

#ifdef KERNEL_MODE
	static NTSTATUS Hash(const CBuffer& buffer, CBuffer& hash, FW::AbstractMemPool* pMemPool);
#else
	static NTSTATUS Hash(const CBuffer& buffer, CBuffer& hash, FW::AbstractMemPool* pMemPool = nullptr);
#endif

protected:
	friend struct SHashFunction;
	struct SHashFunction* m;
};