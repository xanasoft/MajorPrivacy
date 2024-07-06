#include "pch.h"
#include "SharedCrypto.h"

CCryptoBase::CCryptoBase(FW::AbstractMemPool* pMemPool) 
	: FW::AbstractContainer(pMemPool) 
{
}

void* CCryptoBase::Alloc(size_t size)
{
	if (m_pMem)
		return m_pMem->Alloc(size);
	else
#ifdef NO_CRT
		return NULL;
#else
		return malloc(size);
#endif
}

void CCryptoBase::Free(void* ptr)
{
	if(m_pMem)
		m_pMem->Free(ptr);
#ifndef NO_CRT
	else
		free(ptr);
#endif
}