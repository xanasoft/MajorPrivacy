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


//

// secure zero helper (keeps your original)
static void SecureZero(void* p, SIZE_T n) {
    volatile BYTE* vp = (volatile BYTE*)p;
    while (n--) *vp++ = 0;
}

// Context that caches algorithm handle + allocated hashObject
struct HMAC_SHA256_CNG_CTX {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    ULONG objLen = 0;
    ULONG hashLen = 0;
    std::vector<BYTE> hashObject; // pre-allocated hash object buffer
    bool initialized = false;
};

// Initialize context (open provider, query sizes, allocate buffer)
static NTSTATUS HmacSha256_CNG_Init(HMAC_SHA256_CNG_CTX& ctx)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG resultLen = 0;

    if (ctx.initialized) return STATUS_SUCCESS;

    status = BCryptOpenAlgorithmProvider(&ctx.hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status)) return status;

    // Query object length
    status = BCryptGetProperty(ctx.hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&ctx.objLen, sizeof(ctx.objLen), &resultLen, 0);
    if (!BCRYPT_SUCCESS(status)) goto fail;

    // Query hash length
    status = BCryptGetProperty(ctx.hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&ctx.hashLen, sizeof(ctx.hashLen), &resultLen, 0);
    if (!BCRYPT_SUCCESS(status)) goto fail;

    if (ctx.objLen == 0 || ctx.hashLen == 0) { status = STATUS_UNSUCCESSFUL; goto fail; }

    // allocate hash object once
    try {
        ctx.hashObject.resize(ctx.objLen);
    } catch(...) {
        status = STATUS_NO_MEMORY;
        goto fail;
    }

    ctx.initialized = true;
    return STATUS_SUCCESS;

fail:
    if (ctx.hAlg) { BCryptCloseAlgorithmProvider(ctx.hAlg, 0); ctx.hAlg = NULL; }
    ctx.hashObject.clear();
    ctx.initialized = false;
    return status;
}

// Cleanup context (zero + free)
static void HmacSha256_CNG_Destroy(HMAC_SHA256_CNG_CTX& ctx)
{
    if (!ctx.initialized) return;
    if (!ctx.hashObject.empty()) {
        SecureZero(ctx.hashObject.data(), ctx.hashObject.size());
        ctx.hashObject.clear();
    }
    if (ctx.hAlg) {
        BCryptCloseAlgorithmProvider(ctx.hAlg, 0);
        ctx.hAlg = NULL;
    }
    ctx.objLen = ctx.hashLen = 0;
    ctx.initialized = false;
}

// Single-shot HMAC-SHA256 using cached ctx (fast path)
static NTSTATUS HmacSha256_CNG_Once(
    HMAC_SHA256_CNG_CTX& ctx,
    const BYTE* key, ULONG keyLen,
    const BYTE* data, ULONG dataLen,
    BYTE* out, ULONG outLen)
{
    if (!ctx.initialized) return STATUS_INVALID_PARAMETER;
    if (!key || keyLen == 0 || !data || dataLen == 0 || !out) return STATUS_INVALID_PARAMETER;
    if (outLen < ctx.hashLen) return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS status;
    BCRYPT_HASH_HANDLE hHash = NULL;

    status = BCryptCreateHash(ctx.hAlg, &hHash, ctx.hashObject.data(), ctx.objLen, (PUCHAR)key, keyLen, 0);
    if (!BCRYPT_SUCCESS(status)) goto cleanup;

    status = BCryptHashData(hHash, (PUCHAR)data, dataLen, 0);
    if (!BCRYPT_SUCCESS(status)) goto cleanup;

    status = BCryptFinishHash(hHash, out, ctx.hashLen, 0);

cleanup:
    if (hHash) BCryptDestroyHash(hHash);
    return status;
}

// Simple PBKDF2-HMAC-SHA256 fallback (password = HMAC key) — now uses the cached ctx
static NTSTATUS PBKDF2_HMAC_SHA256_Fallback(
    const BYTE* password, ULONG passwordLen,
    const BYTE* salt, ULONG saltLen,
    ULONGLONG iterations,
    BYTE* out, ULONG outLen)
{
    NTSTATUS status;
    HMAC_SHA256_CNG_CTX ctx;

    // Initialize once
    status = HmacSha256_CNG_Init(ctx);
    if (!BCRYPT_SUCCESS(status)) return status;

    ULONG hashLen = ctx.hashLen;
    ULONG L = (outLen + hashLen - 1) / hashLen;
    std::vector<BYTE> U(hashLen);
    std::vector<BYTE> T(hashLen);
    std::vector<BYTE> buf(saltLen + 4);

    if (salt && saltLen) memcpy(buf.data(), salt, saltLen);

    BYTE* dst = out;
    ULONG remaining = outLen;

    for (ULONG i = 1; i <= L; ++i) {
        // INT(i) BE
        buf[saltLen + 0] = (BYTE)((i >> 24) & 0xff);
        buf[saltLen + 1] = (BYTE)((i >> 16) & 0xff);
        buf[saltLen + 2] = (BYTE)((i >> 8) & 0xff);
        buf[saltLen + 3] = (BYTE)((i >> 0) & 0xff);

        // U1 = HMAC(password, salt || INT(i))
        status = HmacSha256_CNG_Once(ctx, password, passwordLen, buf.data(), (ULONG)(saltLen + 4), U.data(), hashLen);
        if (!BCRYPT_SUCCESS(status)) goto fail;

        memcpy(T.data(), U.data(), hashLen);

        for (ULONGLONG j = 1; j < iterations; ++j) {
            status = HmacSha256_CNG_Once(ctx, password, passwordLen, U.data(), hashLen, U.data(), hashLen);
            if (!BCRYPT_SUCCESS(status)) goto fail;
            for (ULONG k = 0; k < hashLen; ++k) T[k] ^= U[k];
        }

        ULONG copy = (remaining > hashLen) ? hashLen : remaining;
        memcpy(dst, T.data(), copy);
        dst += copy;
        remaining -= copy;
    }

    status = STATUS_SUCCESS;

fail:
    // zero sensitive buffers
    if (!U.empty()) SecureZero(U.data(), U.size());
    if (!T.empty()) SecureZero(T.data(), T.size());
    if (!buf.empty()) SecureZero(buf.data(), buf.size());

    // Destroy and zero hash object inside ctx
    HmacSha256_CNG_Destroy(ctx);

    return status;
}

//

NTSTATUS CEncryption::GetKeyFromPW(const CBuffer& password, CBuffer& key, ULONG Iterations)
{
    NTSTATUS status;
    DWORD ResultLength = 0;

    /*const BYTE Salt [] =
    {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };*/
    const BYTE* salt = nullptr;
    ULONG saltLen = 0;

    const ULONGLONG IterationCount = Iterations;

    typedef NTSTATUS (WINAPI *PFN_BCryptKeyDerivation)(BCRYPT_KEY_HANDLE, BCryptBufferDesc*, PUCHAR, ULONG, PULONG, ULONG);
    HMODULE hBcrypt = GetModuleHandleW(L"bcrypt.dll");
    PFN_BCryptKeyDerivation pfnKeyDerivation = hBcrypt ? (PFN_BCryptKeyDerivation)GetProcAddress(hBcrypt, "BCryptKeyDerivation") : NULL;
    if (pfnKeyDerivation) 
    {
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

        CScopedHandle<BCRYPT_KEY_HANDLE, void(*)(BCRYPT_ALG_HANDLE)> AesPasswordKeyHandle(NULL, [](BCRYPT_KEY_HANDLE h) {BCryptDestroyKey(h);});
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

        //status = BCryptKeyDerivation(
        status = pfnKeyDerivation(
            AesPasswordKeyHandle,       // Handle to the password key
            &PBKDF2Parameters,          // Parameters to the KDF algorithm
            (PBYTE)key.GetBuffer(),     // Address of the buffer which recieves the derived bytes
            (ULONG)key.GetCapacity(),   // Size of the buffer in bytes
            &ResultLength,              // Variable that recieves number of bytes copied to above buffer  
            0);                         // Flags
    }
    else // FALLBACK: PBKDF2 implementation (password used as HMAC key)
    {
        ResultLength = (ULONG)key.GetCapacity();
        status = PBKDF2_HMAC_SHA256_Fallback(
            (const BYTE*)password.GetBuffer(), (ULONG)password.GetSize(),
            salt, saltLen,
            IterationCount,
            key.GetBuffer(), 
            ResultLength);
    }

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

    PBYTE ChainingMode = (PBYTE)BCRYPT_CHAIN_MODE_CFB;
    DWORD ChainingModeLength = sizeof(BCRYPT_CHAIN_MODE_CFB);
    status = BCryptSetProperty( 
        m->AlgHandle,               // Handle to a CNG object          
        BCRYPT_CHAINING_MODE,       // Property name(null terminated unicode string)
        ChainingMode,               // Address of the buffer that contains the new property value 
        ChainingModeLength,         // Size of the buffer in bytes
        0);                         // Flags
    if( !NT_SUCCESS(status) )
        return status; //ERR(status, L"Unable to set the chaining mode on the key handle");

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
