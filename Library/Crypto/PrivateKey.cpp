#include "pch.h"
#include "PrivateKey.h"
#include "PublicKey.h"
#include "PrivateCrypto.h"

CPrivateKey::CPrivateKey(FW::AbstractMemPool* pMemPool)
    : CCryptoBase(pMemPool)
{
    m = New<SPrivateCrypto>(m_pMem);
}

CPrivateKey::~CPrivateKey()
{
   Delete(m);
}

NTSTATUS CPrivateKey::MakeKeyPair(class CPublicKey& PubKey)
{
    NTSTATUS status;
    CScopedHandle<BCRYPT_ALG_HANDLE, void(*)(BCRYPT_ALG_HANDLE)> signAlgHandle(NULL, [](BCRYPT_ALG_HANDLE h) {BCryptCloseAlgorithmProvider(h, 0);});
    CScopedHandle keyHandle((BCRYPT_KEY_HANDLE)0, BCryptDestroyKey);

    status = BCryptOpenAlgorithmProvider(&signAlgHandle, CRYPTO_SIGN_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"Unable to open the signing algorithm provider");

    status = BCryptGenerateKeyPair(signAlgHandle, &keyHandle, CRYPTO_SIGN_ALGORITHM_BITS, 0);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"Unable to generate the key pair");

    status = BCryptFinalizeKeyPair(keyHandle, 0);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"Unable to finalize the key");

    CBuffer privKey;
    status = m->ExportKey(keyHandle, CRYPTO_BLOB_PRIVATE, privKey);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"Unable to export the private key");
    SetPrivateKey(privKey);

	CBuffer pubKey;
    status = m->ExportKey(keyHandle, CRYPTO_BLOB_PUBLIC, pubKey);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"Unable to export the public key");
    
    return PubKey.SetPublicKey(pubKey);
}

NTSTATUS CPrivateKey::SetPrivateKey(const CBuffer& PrivateKey)
{
    m->Key = PrivateKey;

    return InitCrypto();
}

bool CPrivateKey::IsPrivateKeySet() const
{
	return m && m->Key.GetSize() > 0;
}

NTSTATUS CPrivateKey::GetPrivateKey(CBuffer& SymetricKey) const
{
    if(!m)
        return STATUS_DEVICE_NOT_READY;

    // WARNING the buffer is only valid as long as m->Key exists and is not changed
    SymetricKey.SetBuffer(m->Key.GetBuffer(), m->Key.GetSize(), true);
    return STATUS_SUCCESS;
}

NTSTATUS CPrivateKey::InitCrypto()
{
    if(!m)
        return STATUS_DEVICE_NOT_READY;

    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&m->AlgHandle, CRYPTO_SIGN_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"Unable to open the signing algorithm provider");

    status = BCryptImportKeyPair(m->AlgHandle, NULL, CRYPTO_BLOB_PRIVATE, &m->KeyHandle, m->Key.GetBuffer(), (ULONG)m->Key.GetSize(), 0);
    if (!NT_SUCCESS(status))
        return status; //ERR(status, L"Unable to import the key pair");

    return status; //OK;
}

NTSTATUS CPrivateKey::Sign(const CBuffer& Data, CBuffer& Signature) const
{
    if(!m)
        return STATUS_DEVICE_NOT_READY;

    NTSTATUS status;

	DWORD signatureSize;
	status = BCryptSignHash(m->KeyHandle, NULL, (PBYTE)Data.GetBuffer(), (ULONG)Data.GetSize(), NULL, 0, &signatureSize, 0);
	if (!NT_SUCCESS(status))
        return status; //ERR(status, L"Unable to get the signature size");

	Signature.SetSize(0, true, signatureSize);
	status = BCryptSignHash(m->KeyHandle, NULL, (PBYTE)Data.GetBuffer(), (ULONG)Data.GetSize(), Signature.GetBuffer(), (ULONG)Signature.GetCapacity(), &signatureSize, 0);
	if (!NT_SUCCESS(status))
        return status; //ERR(status, L"Unable to sign the data");
    Signature.SetSize(signatureSize);

    return status; //OK;
}