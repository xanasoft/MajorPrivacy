#include "pch.h"
#include "PublicKey.h"
#include "PrivateCrypto.h"

CPublicKey::CPublicKey(FW::AbstractMemPool* pMemPool)
	: CCryptoBase(pMemPool)
{
	m = New<SPrivateCrypto>(m_pMem);
}

CPublicKey::~CPublicKey()
{
   Delete(m);
}

NTSTATUS CPublicKey::SetPublicKey(const CBuffer& PublicKey)
{
	if(!m)
		return STATUS_DEVICE_NOT_READY;

    m->Key = PublicKey;

	return InitCrypto();
}

NTSTATUS CPublicKey::GetPublicKey(CBuffer& PublicKey) const
{
	if(!m)
		return STATUS_DEVICE_NOT_READY;

	// WARNING the buffer is only valid as long as m->Key exists and is not changed
	PublicKey.SetBuffer(m->Key.GetBuffer(), m->Key.GetSize(), true);
	return STATUS_SUCCESS;
}

NTSTATUS CPublicKey::InitCrypto()
{
	if(!m)
		return STATUS_DEVICE_NOT_READY;

	NTSTATUS status;

	status = BCryptOpenAlgorithmProvider(&m->AlgHandle, CRYPTO_SIGN_ALGORITHM, NULL, 0);
	if (!NT_SUCCESS(status))
		return status; //ERR(status, L"Unable to open the signing algorithm provider");

	status = BCryptImportKeyPair(m->AlgHandle, NULL, CRYPTO_BLOB_PUBLIC, &m->KeyHandle, m->Key.GetBuffer(), (ULONG)m->Key.GetSize(), 0);
	if (!NT_SUCCESS(status))
		return status; //ERR(status, L"Unable to import the key pair");

	return status; //OK;
}

NTSTATUS CPublicKey::Verify(const CBuffer& Data, const CBuffer& Signature) const
{
	if(!m)
		return STATUS_DEVICE_NOT_READY;

	NTSTATUS status;

	status = BCryptVerifySignature(m->KeyHandle, NULL, (PBYTE)Data.GetBuffer(), (ULONG)Data.GetSize(), (PBYTE)Signature.GetBuffer(), (ULONG)Signature.GetSize(), 0);
	if (!NT_SUCCESS(status))
		return status; //ERR(status, L"Unable to verify the signature");

	return status; //OK;
}