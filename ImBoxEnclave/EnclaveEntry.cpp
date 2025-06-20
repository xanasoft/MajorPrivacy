// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "EnclaveEntry.h"

void assert(VOID** obj) {
    if (!obj) {
        OutputDebugString(L"assert failed\n");
        //We cannot use it in an enclave
        //__fastfail(1);
        TerminateThread(GetCurrentThread(), 0);
    }
}

const IMAGE_ENCLAVE_CONFIG __enclave_config = {
    sizeof(IMAGE_ENCLAVE_CONFIG),
    IMAGE_ENCLAVE_MINIMUM_CONFIG_SIZE,
    //IMAGE_ENCLAVE_POLICY_DEBUGGABLE,    // DO NOT SHIP DEBUGGABLE ENCLAVES TO PRODUCTION
    0,
    0,
    0,
    0,
    { 0xFE, 0xFE },    // family id
    { 0x01, 0x01 },    // image id
    0,                 // version
    0,                 // SVN
    0x10000000,        // size
    16,                // number of threads
    IMAGE_ENCLAVE_FLAG_PRIMARY_IMAGE
};



template <class T>
struct SSecureBuffer
{
    SSecureBuffer() { alloc(sizeof(T)); }
    SSecureBuffer(ULONG length) { alloc(length); }
    ~SSecureBuffer() { free(); }

    void alloc(ULONG length)
    {
        // on 32 bit system xts_key must be located in executable memory
        // x64 does not require this
#ifdef _M_IX86
        ptr = (T*)VirtualAlloc(NULL, length, MEM_COMMIT + MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
        ptr = (T*)VirtualAlloc(NULL, length, MEM_COMMIT + MEM_RESERVE, PAGE_READWRITE);
#endif
        //We assume memory in the enclave will not be swapped anyway
        //if (ptr)
          //  VirtualLock(ptr, length);
    }

    void free()
    {
        if (!ptr)
            return;

        MEMORY_BASIC_INFORMATION mbi;
        if ((VirtualQuery(ptr, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.BaseAddress == ptr && mbi.AllocationBase == ptr))
        {
            //RtlSecureZeroMemory(ptr, mbi.RegionSize);
            //We have to use particular APIs in an enclave
            memset(ptr, 0, mbi.RegionSize);

            //VirtualUnlock(ptr, mbi.RegionSize);
            
        }
        VirtualFree(ptr, 0, MEM_RELEASE);

        ptr = NULL;
    }

    T* operator ->() { return ptr; }
    explicit operator bool() { return ptr != NULL; }

    T* ptr;
};



void*
CALLBACK
EnclaveSetKey(
    _In_ void* Context
)
{
    WCHAR String[32];
    swprintf_s(String, ARRAYSIZE(String), L"%s\n", L"CallEnclaveTest started");
    OutputDebugStringW(String);

    return (void*)((ULONG_PTR)(Context) ^ InitialCookie);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

