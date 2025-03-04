// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"


#include <string>
#include <list>
#include <vector>
#include <map>
//#include <mutex>
#include <memory>

class CTest0
{
public:
    CTest0()
    {
    }
    virtual ~CTest0()
    {
    }

    virtual int Func1() = 0;
};

class CTest1 : public CTest0
{
public:
    CTest1()
    {
    }
    virtual ~CTest1()
    {
    }
    
    virtual int Func1() { return 123; }
};

/*
void* my_new(size_t size)
{
    return LocalAlloc(0, size);
}

void my_delete(void* ptr)
{
    LocalFree(ptr);
}

void* operator new(size_t size) { return my_new(size); }
void operator delete(void* ptr) { my_delete(ptr); }
void* operator new(size_t size, UINT_PTR) noexcept { return my_new(size); }
void operator delete(void* ptr, UINT_PTR) noexcept { my_delete(ptr); }
void* operator new[](size_t size) { return my_new(size); }
void operator delete[](void* ptr) { my_delete(ptr); }
void* operator new[](size_t size, UINT_PTR) noexcept { return my_new(size); }
void operator delete[](void* ptr, UINT_PTR) noexcept { my_delete(ptr); }

int _purecall() { return 0; }

__declspec(noreturn)
_ACRTIMP void __cdecl _invoke_watson(
    _In_opt_z_ wchar_t const* _Expression,
    _In_opt_z_ wchar_t const* _FunctionName,
    _In_opt_z_ wchar_t const* _FileName,
    _In_       unsigned int _LineNo,
    _In_       uintptr_t _Reserved)
{
}

_ACRTIMP int __cdecl _CrtDbgReport(
    _In_       int         _ReportType,
    _In_opt_z_ char const* _FileName,
    _In_       int         _Linenumber,
    _In_opt_z_ char const* _ModuleName,
    _In_opt_z_ char const* _Format,
    ...)
{
    return 0;
}
*/

void Test()
{
    //CTest1 sTest;

    CTest0* pTest = new CTest1();

    int test1 = pTest->Func1();

    delete pTest;

    std::wstring testStr = L"test";

    /*std::mutex mutex;
    mutex.lock();

    std::wstring testStr = L"test";

    mutex.unlock();*/

    
    {
        std::shared_ptr<CTest0> ptr = std::make_shared<CTest1>();

        
        int test = ptr->Func1();

    }

    

    std::vector<std::wstring> testList;
    testList.push_back(L"1");
    testList.push_back(L"2");
    testList.push_back(L"3");

    for (auto I = testList.begin(); I != testList.end(); I++)
    {
        continue;
    }

    std::map<int, std::wstring> testMap;
    testMap[1] = L"1";
    testMap[2] = L"2";
    testMap[3] = L"3";

    for (auto I = testMap.begin(); I != testMap.end(); I++)
    {
        continue;
    }
    
}

#include "../LdrCode/ldrdata.h"

#ifndef _WIN64
#pragma comment(linker, "/EXPORT:InstallHooks=_InstallHooks@4")
#endif

extern "C" __declspec(dllexport) void __stdcall InstallHooks(INJECT_DATA* pInjectData)
{
    LDRCODE_DATA* pLdrData = (LDRCODE_DATA *)pInjectData->LdrData;

	VirtualFree((VOID*)pLdrData->ExtraData, 0, MEM_RELEASE);

    return;
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        //Test();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

