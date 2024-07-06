#include "pch.h"
#include "HashFunction.h"
#include <bcrypt.h>

struct SHashFunction
{
    SHashFunction(CHashFunction* This) : hashObjectSize(0), hashSize(0), hashObject(NULL, [](BYTE* p, CHashFunction* This) {This->Free(p);}, This)
    {
        AlgHandle.Set(NULL, [](BCRYPT_ALG_HANDLE h) {BCryptCloseAlgorithmProvider(h, 0);});
        HashHandle.Set(NULL, BCryptDestroyHash);
    }
    ~SHashFunction() {}

	CScopedHandle<BCRYPT_ALG_HANDLE, void(*)(BCRYPT_ALG_HANDLE)> AlgHandle;
    ULONG hashObjectSize;
    ULONG hashSize;
    CScopedHandleEx<BYTE*, void(*)(BYTE* p, CHashFunction* This), CHashFunction*> hashObject;
	CScopedHandle<BCRYPT_HASH_HANDLE, NTSTATUS(WINAPI*)(BCRYPT_HASH_HANDLE)> HashHandle;
};

CHashFunction::CHashFunction(FW::AbstractMemPool* pMemPool)
    : CCryptoBase(pMemPool)
{
    m = New<SHashFunction>(this);
}

CHashFunction::~CHashFunction()
{
    Delete(m);
}

NTSTATUS CHashFunction::InitHash()
{
    if(!m)
        return STATUS_DEVICE_NOT_READY;

    NTSTATUS status;
    ULONG querySize;

    status = BCryptOpenAlgorithmProvider(&m->AlgHandle, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"BCryptOpenAlgorithmProvider failed");

    status = BCryptGetProperty(m->AlgHandle, BCRYPT_OBJECT_LENGTH, (PUCHAR)&m->hashObjectSize, sizeof(ULONG), &querySize, 0);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"BCryptGetProperty failed");

    status = BCryptGetProperty(m->AlgHandle, BCRYPT_HASH_LENGTH, (PUCHAR)&m->hashSize, sizeof(ULONG), &querySize, 0);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"BCryptGetProperty failed");

    return ResetHash();
}

NTSTATUS CHashFunction::ResetHash()
{
    if(!m || !m->AlgHandle)
        return STATUS_DEVICE_NOT_READY;

    NTSTATUS status;

    m->hashObject.Set((BYTE*)Alloc(m->hashObjectSize));
    if(!m->hashObject)
        return STATUS_INSUFFICIENT_RESOURCES; //ERR(STATUS_INSUFFICIENT_RESOURCES, L"Failed to allocate memory for hash object");
    status = BCryptCreateHash(m->AlgHandle, &m->HashHandle, m->hashObject, m->hashObjectSize, NULL, 0, 0);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"BCryptCreateHash failed");

    return status; //OK;
}

NTSTATUS CHashFunction::UpdateHash(const CBuffer& buffer)
{
    if(!m || !m->HashHandle)
        return STATUS_DEVICE_NOT_READY;

	NTSTATUS status;

	status = BCryptHashData(m->HashHandle, (PUCHAR)buffer.GetBuffer(), (ULONG)buffer.GetSize(), 0);
	if (!NT_SUCCESS(status))
        return status; //ERR(status, L"BCryptHashData failed");

	return status; //OK;
}

NTSTATUS CHashFunction::FinalizeHash(CBuffer& hash)
{
    if(!m || !m->HashHandle)
        return STATUS_DEVICE_NOT_READY;

    NTSTATUS status;

	hash.SetSize(m->hashSize, true);
	status = BCryptFinishHash(m->HashHandle, hash.GetBuffer(), m->hashSize, 0);
	if (!NT_SUCCESS(status))
        return status; //ERR(status, L"BCryptFinishHash failed");

	return status; //OK;
}

NTSTATUS CHashFunction::Hash(const CBuffer& buffer, CBuffer& hash, FW::AbstractMemPool* pMemPool)
{
    NTSTATUS status;
    CHashFunction hashFunction(pMemPool);

    status = hashFunction.InitHash();
    if (!NT_SUCCESS(status))
        return status;

    status = hashFunction.UpdateHash(buffer);
    if (!NT_SUCCESS(status))
        return status;

    status = hashFunction.FinalizeHash(hash);
    if (!NT_SUCCESS(status))
        return status;

    return status; //OK;
}