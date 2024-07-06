#include "pch.h"
#include "Encryption.h"
#include "PrivateCrypto.h"

CEncryption::CEncryption(FW::AbstractMemPool* pMemPool)
 : CCryptoBase(pMemPool)
{
    m = New<SPrivateCrypto>(m_pMem);
}

CEncryption::~CEncryption()
{
    Delete(m);
}

NTSTATUS CEncryption::GetKeyFromPW(const CBuffer& password, CBuffer& key, ULONG Iterations)
{
    NTSTATUS status;

    /*const BYTE Salt [] =
    {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };*/
    const ULONGLONG IterationCount = Iterations;

    BCryptBuffer PBKDF2ParameterBuffers[] = {
        {
            sizeof (BCRYPT_SHA256_ALGORITHM),
            KDF_HASH_ALGORITHM,
            (PBYTE)BCRYPT_SHA256_ALGORITHM,
        },
        /*{
        sizeof (Salt),
        KDF_SALT,
        (PBYTE)Salt,
        },*/
        {
            sizeof (IterationCount),
            KDF_ITERATION_COUNT,
            (PBYTE)&IterationCount,
        }
    };

    BCryptBufferDesc PBKDF2Parameters = {
        BCRYPTBUFFER_VERSION,
        2, //3,
        PBKDF2ParameterBuffers
    };

    CScopedHandle<BCRYPT_ALG_HANDLE, void(*)(BCRYPT_ALG_HANDLE)> KdfAlgHandle(NULL, [](BCRYPT_ALG_HANDLE h) {BCryptCloseAlgorithmProvider(h, 0);});
    status = BCryptOpenAlgorithmProvider(
        &KdfAlgHandle,              // Alg Handle pointer
        BCRYPT_PBKDF2_ALGORITHM,    // Cryptographic Algorithm name (null terminated unicode string)
        NULL,                       // Provider name; if null, the default provider is loaded
        0);                         // Flags
    if( !NT_SUCCESS(status) )
        return status; //ERR(status, L"Unable to open the PBKDF2 algorithm provider");

    BCRYPT_KEY_HANDLE AesPasswordKeyHandle = NULL;
    status = BCryptGenerateSymmetricKey(
        KdfAlgHandle,               // Algorithm Handle 
        &AesPasswordKeyHandle,      // A pointer to a key handle
        NULL,                       // Buffer that recieves the key object;NULL implies memory is allocated and freed by the function
        0,                          // Size of the buffer in bytes
        (PBYTE)password.GetBuffer(),// Buffer that contains the key material
        (ULONG)password.GetSize(),  // Size of the buffer in bytes
        0);                         // Flags
    if( !NT_SUCCESS(status) )
        return status; //ERR(status, L"Unable to generate the symmetric key from the password");

    DWORD ResultLength = 0;
    status = BCryptKeyDerivation(
        AesPasswordKeyHandle,       // Handle to the password key
        &PBKDF2Parameters,          // Parameters to the KDF algorithm
        (PBYTE)key.GetBuffer(),  // Address of the buffer which recieves the derived bytes
        (ULONG)key.GetCapacity(),// Size of the buffer in bytes
        &ResultLength,              // Variable that recieves number of bytes copied to above buffer  
        0);                         // Flags
    if( !NT_SUCCESS(status) )
        return status; //ERR(status, L"Unable to derive the AES key from the password");

    key.SetSize(ResultLength, false);

    return status; // OK;
}

NTSTATUS CEncryption::SetPassword(const CBuffer& password)
{
    if(!m)
        return STATUS_DEVICE_NOT_READY;

    m->Key.SetSize(0, true, 256/8);

    NTSTATUS status = GetKeyFromPW(password, m->Key);
    if( !NT_SUCCESS(status) )
		return status; //ERR(status, L"Unable to derive the AES key from the password");

    return InitCrypto();
}

NTSTATUS CEncryption::SetSymetricKey(const CBuffer& SymetricKey)
{
    if(!m)
        return STATUS_DEVICE_NOT_READY;

	m->Key = SymetricKey;

    return InitCrypto();
}

NTSTATUS CEncryption::GetSymetricKey(CBuffer& SymetricKey) const
{
    if(!m)
        return STATUS_DEVICE_NOT_READY;

    // WARNING the buffer is only valid as long as m->Key exists and is not changed
    SymetricKey.SetBuffer(m->Key.GetBuffer(), m->Key.GetSize(), true);
    return STATUS_SUCCESS;
}

NTSTATUS CEncryption::InitCrypto()
{
    if(!m)
        return STATUS_DEVICE_NOT_READY;

    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(
        &m->AlgHandle,              // Alg Handle pointer
        BCRYPT_AES_ALGORITHM,       // Cryptographic Algorithm name (null terminated unicode string)
        NULL,                       // Provider name; if null, the default provider is loaded
        0);                         // Flags

    status = BCryptGenerateSymmetricKey(
        m->AlgHandle,               // Algorithm provider handle
        &m->KeyHandle,              // A pointer to key handle
        NULL,                       // A pointer to the buffer that recieves the key object;NULL implies memory is allocated and freed by the function
        0,                          // Size of the buffer in bytes
        (PBYTE)m->Key.GetBuffer(),  // A pointer to a buffer that contains the key material
        (ULONG)m->Key.GetSize(),    // Size of the buffer in bytes
        0);                         // Flags
    if( !NT_SUCCESS(status) )
        return status; //ERR(status, L"Unable to generate the symmetric key");

    PBYTE ChainingMode = (PBYTE)BCRYPT_CHAIN_MODE_CFB;
    DWORD ChainingModeLength = (DWORD)(wcslen((wchar_t*)ChainingMode) + 1) * sizeof(wchar_t);
    status = BCryptSetProperty( 
        m->KeyHandle,               // Handle to a CNG object          
        BCRYPT_CHAINING_MODE,       // Property name(null terminated unicode string)
        ChainingMode,               // Address of the buffer that contains the new property value 
        ChainingModeLength,         // Size of the buffer in bytes
        0);                         // Flags
    if( !NT_SUCCESS(status) )
        return status; //ERR(status, L"Unable to set the chaining mode on the key handle");

    return status; // OK;
}

NTSTATUS CEncryption::Encrypt(const CBuffer& in, CBuffer& out)
{
    if(!m)
        return STATUS_DEVICE_NOT_READY;

    NTSTATUS status;

    DWORD TempInitVectorLength = 128/8;
    CScopedHandleEx<BYTE*, void(*)(BYTE* p, CEncryption* This), CEncryption*> TempInitVector((BYTE*)Alloc(TempInitVectorLength), [](BYTE* p, CEncryption* This) {This->Free(p);}, this);
    memset(TempInitVector, 0, TempInitVectorLength);

    DWORD CipherTextLength = 0;
    status = BCryptEncrypt(
        m->KeyHandle,               // Handle to a key which is used to encrypt 
        (PUCHAR)in.GetBuffer(),     // Address of the buffer that contains the plaintext
        (ULONG)in.GetSize(),        // Size of the buffer in bytes
        NULL,                       // A pointer to padding info, used with asymmetric and authenticated encryption; else set to NULL
        TempInitVector,             // Address of the buffer that contains the IV. 
        TempInitVectorLength,       // Size of the IV buffer in bytes
        NULL,                       // Address of the buffer the recieves the ciphertext
        0,                          // Size of the buffer in bytes
        &CipherTextLength,          // Variable that recieves number of bytes copied to ciphertext buffer 
        BCRYPT_BLOCK_PADDING);      // Flags; Block padding allows to pad data to the next block size
    if( !NT_SUCCESS(status) )
        return status; //ERR(status, L"Unable to get the length of the cipher text");

    out.SetSize(0, true, CipherTextLength);

    DWORD ResultLength = 0;
    status = BCryptEncrypt(
        m->KeyHandle,               // Handle to a key which is used to encrypt 
        (PUCHAR)in.GetBuffer(),     // Address of the buffer that contains the plaintext
        (ULONG)in.GetSize(),        // Size of the buffer in bytes
        NULL,                       // A pointer to padding info, used with asymmetric and authenticated encryption; else set to NULL
        TempInitVector,             // Address of the buffer that contains the IV. 
        TempInitVectorLength,       // Size of the IV buffer in bytes
        (PUCHAR)out.GetBuffer(),    // Address of the buffer the recieves the ciphertext
        CipherTextLength,           // Size of the buffer in bytes
        &ResultLength,              // Variable that recieves number of bytes copied to ciphertext buffer 
        BCRYPT_BLOCK_PADDING);      // Flags; Block padding allows to pad data to the next block size
    if( !NT_SUCCESS(status) )
        return status; //ERR(status, L"Unable to encrypt the data");

    out.SetSize(ResultLength, false);

    return status; //OK;
}

NTSTATUS CEncryption::Decrypt(const CBuffer& in, CBuffer& out)
{
    NTSTATUS status;

    DWORD TempInitVectorLength = 128/8;
    CScopedHandleEx<BYTE*, void(*)(BYTE* p, CEncryption* This), CEncryption*> TempInitVector((BYTE*)Alloc(TempInitVectorLength), [](BYTE* p, CEncryption* This) {This->Free(p);}, this);
    memset(TempInitVector, 0, TempInitVectorLength);

    DWORD PlainTextLength = 0;
    status = BCryptDecrypt(
        m->KeyHandle,               // Handle to a key which is used to encrypt 
        (PUCHAR)in.GetBuffer(),     // Address of the buffer that contains the plaintext
        (ULONG)in.GetSize(),        // Size of the buffer in bytes
        NULL,                       // A pointer to padding info, used with asymmetric and authenticated encryption; else set to NULL
        TempInitVector,             // Address of the buffer that contains the IV. 
        TempInitVectorLength,       // Size of the IV buffer in bytes
        NULL,                       // Address of the buffer the recieves the ciphertext
        0,                          // Size of the buffer in bytes
        &PlainTextLength,           // Variable that recieves number of bytes copied to ciphertext buffer 
        BCRYPT_BLOCK_PADDING);      // Flags; Block padding allows to pad data to the next block size
    if( !NT_SUCCESS(status) )
        return status; //ERR(status, L"Unable to get the length of the plain text");

    out.SetSize(0, true, PlainTextLength);

    DWORD ResultLength = 0;
    status = BCryptDecrypt(
        m->KeyHandle,               // Handle to a key which is used to encrypt 
        (PUCHAR)in.GetBuffer(),     // Address of the buffer that contains the plaintext
        (ULONG)in.GetSize(),        // Size of the buffer in bytes
        NULL,                       // A pointer to padding info, used with asymmetric and authenticated encryption; else set to NULL
        TempInitVector,             // Address of the buffer that contains the IV. 
        TempInitVectorLength,       // Size of the IV buffer in bytes
        (PUCHAR)out.GetBuffer(),    // Address of the buffer the recieves the ciphertext
        PlainTextLength,            // Size of the buffer in bytes
        &ResultLength,              // Variable that recieves number of bytes copied to ciphertext buffer 
        BCRYPT_BLOCK_PADDING);      // Flags; Block padding allows to pad data to the next block size
    if( !NT_SUCCESS(status) )
        return status; //ERR(status, L"Unable to decrypt the data");

    out.SetSize(ResultLength, false);

    return status; //OK;
}
