#pragma once

#include "../Library/Common/Buffer.h"
#include "../Library/Helpers/ScopedHandle.h"
#include "../Framework/SafeRef.h"

#define CRYPTO_SIGN_ALGORITHM BCRYPT_ECDSA_P256_ALGORITHM
#define CRYPTO_SIGN_ALGORITHM_BITS 256
#define CRYPTO_HASH_ALGORITHM BCRYPT_SHA256_ALGORITHM
#define CRYPTO_BLOB_PRIVATE BCRYPT_ECCPRIVATE_BLOB
#define CRYPTO_BLOB_PUBLIC BCRYPT_ECCPUBLIC_BLOB

class LIBRARY_EXPORT CCryptoBase : public FW::AbstractContainer
{
public:
#ifdef KERNEL_MODE
	CCryptoBase(FW::AbstractMemPool* pMemPool);
#else
	CCryptoBase(FW::AbstractMemPool* pMemPool = nullptr);
#endif

protected:

	template<typename T, class... Types>
	T*				New(Types&&... Args) { void* ptr = Alloc(sizeof(T)); if(!ptr) return NULL; return new (ptr) T(Args...); }
	template<typename T>
	void 			Delete(T* ptr) { if(!ptr) return; ptr->~T(); Free(ptr); }

	void*			Alloc(size_t size);
	void			Free(void* ptr);
};