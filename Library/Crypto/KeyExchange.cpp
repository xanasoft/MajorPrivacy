#include "pch.h"
#include "KeyExchange.h"
#include <bcrypt.h>

struct SKeyExchangeCrypto
{
	SKeyExchangeCrypto(FW::AbstractMemPool* pMemPool) : Key(pMemPool), PubKey(pMemPool)
	{
		AlgHandle.Set(NULL, [](BCRYPT_ALG_HANDLE h) {BCryptCloseAlgorithmProvider(h, 0);});
		KeyHandle.Set(NULL, BCryptDestroyKey);
	}
	~SKeyExchangeCrypto() {}

	static NTSTATUS ExportKey(BCRYPT_KEY_HANDLE keyHandle, LPCWSTR blobType, CBuffer& buffer)
	{
		NTSTATUS status;

		DWORD keySize;
		if (!NT_SUCCESS(status = BCryptExportKey(keyHandle, NULL, blobType, NULL, 0, &keySize, 0)))
			return status;

		buffer.AllocBuffer(keySize);
		if (!NT_SUCCESS(status = BCryptExportKey(keyHandle, NULL, blobType, buffer.GetBuffer(), (ULONG)buffer.GetCapacity(), &keySize, 0)))
			return status;
		buffer.SetSize(keySize);

		return status;
	}

	CScopedHandle<BCRYPT_ALG_HANDLE, void(*)(BCRYPT_ALG_HANDLE)> AlgHandle;
	CScopedHandle<BCRYPT_KEY_HANDLE, NTSTATUS(WINAPI*)(BCRYPT_KEY_HANDLE)> KeyHandle;
	CBuffer Key;
	CBuffer PubKey;
};

CKeyExchange::CKeyExchange(FW::AbstractMemPool* pMemPool)
	: CCryptoBase(pMemPool)
{
	m = New<SKeyExchangeCrypto>(m_pMem);
}

CKeyExchange::~CKeyExchange()
{
	Delete(m);
}

NTSTATUS CKeyExchange::GenerateKeyPair()
{
	NTSTATUS status;
	CScopedHandle<BCRYPT_ALG_HANDLE, void(*)(BCRYPT_ALG_HANDLE)> algHandle(NULL, [](BCRYPT_ALG_HANDLE h) {BCryptCloseAlgorithmProvider(h, 0);});
	CScopedHandle<BCRYPT_KEY_HANDLE, NTSTATUS(WINAPI*)(BCRYPT_KEY_HANDLE)> keyHandle((BCRYPT_KEY_HANDLE)0, BCryptDestroyKey);

	status = BCryptOpenAlgorithmProvider(&algHandle, CRYPTO_ECDH_ALGORITHM, NULL, 0);
	if (!NT_SUCCESS(status))
		return status;

	status = BCryptGenerateKeyPair(algHandle, &keyHandle, CRYPTO_ECDH_ALGORITHM_BITS, 0);
	if (!NT_SUCCESS(status))
		return status;

	status = BCryptFinalizeKeyPair(keyHandle, 0);
	if (!NT_SUCCESS(status))
		return status;

	CBuffer privKey;
	status = SKeyExchangeCrypto::ExportKey(keyHandle, BCRYPT_ECCPRIVATE_BLOB, privKey);
	if (!NT_SUCCESS(status))
		return status;

	return SetPrivateKey(privKey);
}

NTSTATUS CKeyExchange::SetPrivateKey(const CBuffer& PrivateKey)
{
	if(!m)
		return STATUS_DEVICE_NOT_READY;

	m->Key = PrivateKey;

	return InitCrypto();
}

NTSTATUS CKeyExchange::GetPrivateKey(CBuffer& PrivateKey) const
{
	if(!m)
		return STATUS_DEVICE_NOT_READY;

	PrivateKey.SetBuffer(m->Key.GetBuffer(), m->Key.GetSize(), true);
	return STATUS_SUCCESS;
}

NTSTATUS CKeyExchange::GetPublicKey(CBuffer& PublicKey) const
{
	if(!m)
		return STATUS_DEVICE_NOT_READY;

	PublicKey.SetBuffer(m->PubKey.GetBuffer(), m->PubKey.GetSize(), true);
	return STATUS_SUCCESS;
}

NTSTATUS CKeyExchange::InitCrypto()
{
	if(!m)
		return STATUS_DEVICE_NOT_READY;

	NTSTATUS status;

	status = BCryptOpenAlgorithmProvider(&m->AlgHandle, CRYPTO_ECDH_ALGORITHM, NULL, 0);
	if (!NT_SUCCESS(status))
		return status;

	status = BCryptImportKeyPair(m->AlgHandle, NULL, BCRYPT_ECCPRIVATE_BLOB, &m->KeyHandle, m->Key.GetBuffer(), (ULONG)m->Key.GetSize(), 0);
	if (!NT_SUCCESS(status))
		return status;

	status = SKeyExchangeCrypto::ExportKey(m->KeyHandle, BCRYPT_ECCPUBLIC_BLOB, m->PubKey);
	if (!NT_SUCCESS(status))
		return status;

	return status;
}

NTSTATUS CKeyExchange::DeriveSharedSecret(const CBuffer& OtherPublicKey, CBuffer& SharedSecret)
{
	if(!m)
		return STATUS_DEVICE_NOT_READY;

	NTSTATUS status;
	CScopedHandle<BCRYPT_KEY_HANDLE, NTSTATUS(WINAPI*)(BCRYPT_KEY_HANDLE)> otherKeyHandle((BCRYPT_KEY_HANDLE)0, BCryptDestroyKey);
	CScopedHandle<BCRYPT_SECRET_HANDLE, NTSTATUS(WINAPI*)(BCRYPT_SECRET_HANDLE)> secretHandle((BCRYPT_SECRET_HANDLE)0, BCryptDestroySecret);

	status = BCryptImportKeyPair(m->AlgHandle, NULL, BCRYPT_ECCPUBLIC_BLOB, &otherKeyHandle, (PUCHAR)OtherPublicKey.GetBuffer(), (ULONG)OtherPublicKey.GetSize(), 0);
	if (!NT_SUCCESS(status))
		return status;

	status = BCryptSecretAgreement(m->KeyHandle, otherKeyHandle, &secretHandle, 0);
	if (!NT_SUCCESS(status))
		return status;

	BCryptBuffer paramBuffers[] = {
		{
			sizeof(BCRYPT_SHA256_ALGORITHM),
			KDF_HASH_ALGORITHM,
			(PBYTE)BCRYPT_SHA256_ALGORITHM,
		}
	};

	BCryptBufferDesc params = {
		BCRYPTBUFFER_VERSION,
		1,
		paramBuffers
	};

	DWORD secretSize;
	status = BCryptDeriveKey(secretHandle, BCRYPT_KDF_HASH, &params, NULL, 0, &secretSize, 0);
	if (!NT_SUCCESS(status))
		return status;

	SharedSecret.SetSize(0, true, secretSize);
	status = BCryptDeriveKey(secretHandle, BCRYPT_KDF_HASH, &params, SharedSecret.GetBuffer(), (ULONG)SharedSecret.GetCapacity(), &secretSize, 0);
	if (!NT_SUCCESS(status))
		return status;
	SharedSecret.SetSize(secretSize);

	return status;
}
