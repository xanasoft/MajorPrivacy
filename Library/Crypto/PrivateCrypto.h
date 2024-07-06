#pragma once

#include <bcrypt.h>

struct SPrivateCrypto
{
    SPrivateCrypto(FW::AbstractMemPool* pMemPool) : Key(pMemPool)
    {
        AlgHandle.Set(NULL, [](BCRYPT_ALG_HANDLE h) {BCryptCloseAlgorithmProvider(h, 0);});
        KeyHandle.Set(NULL, BCryptDestroyKey);
    }
    ~SPrivateCrypto() {}

    static NTSTATUS ExportKey(BCRYPT_KEY_HANDLE keyHandle, LPCWSTR blobType, CBuffer& buffer)
    {
        NTSTATUS status;

        DWORD keySize;
        if (!NT_SUCCESS(status = BCryptExportKey(keyHandle, NULL, blobType, NULL, 0, &keySize, 0)))
            return status; //ERR(status, L"Unable to get the key size");

        buffer.AllocBuffer(keySize);
        if (!NT_SUCCESS(status = BCryptExportKey(keyHandle, NULL, blobType, buffer.GetBuffer(), (ULONG)buffer.GetCapacity(), &keySize, 0)))
            return status; //ERR(status, L"Unable to export the key");
        buffer.SetSize(keySize);

        return status; //OK;
    }

    CScopedHandle<BCRYPT_ALG_HANDLE, void(*)(BCRYPT_ALG_HANDLE)> AlgHandle;
    CScopedHandle<BCRYPT_KEY_HANDLE, NTSTATUS(WINAPI*)(BCRYPT_KEY_HANDLE)> KeyHandle;
    CBuffer Key;
};