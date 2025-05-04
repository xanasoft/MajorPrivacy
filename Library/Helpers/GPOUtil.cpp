#include "pch.h"
#include "GPOUtil.h"

#include <windows.h>
#include <Wtypes.h>
#include <InitGuid.h>
#include <prsht.h>
#include <gpedit.h>

struct SGPO
{
    IGroupPolicyObject* pGPO = NULL;
};

CGPO::CGPO()
{
    m = new SGPO;
}

CGPO::~CGPO()
{
    Close();
    delete m;
}

HRESULT CGPO::OpenGPO(bool bWritable)
{
    if (!SUCCEEDED(CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER, IID_IGroupPolicyObject, (LPVOID*)&m->pGPO))) 
        return S_FALSE;

    DWORD dwFlags = GPO_OPEN_LOAD_REGISTRY;
    if (!bWritable) 
        dwFlags = GPO_OPEN_READ_ONLY;
    return m->pGPO->OpenLocalMachineGPO(dwFlags);
}

HKEY CGPO::GetUserRoot()
{
    HKEY hKey;
    if (!m->pGPO || !SUCCEEDED(m->pGPO->GetRegistryKey(GPO_SECTION_USER, &hKey)))
        return NULL;
    return hKey;
}

HKEY CGPO::GetMachineRoot()
{
    HKEY hKey;
    if (!m->pGPO || !SUCCEEDED(m->pGPO->GetRegistryKey(GPO_SECTION_MACHINE, &hKey)))
        return NULL;
    return hKey;
}

HRESULT CGPO::SaveGPO()
{
    if (!m->pGPO)
        return S_FALSE;

    GUID RegistryId = REGISTRY_EXTENSION_GUID;

    GUID MyRandomGuid =
    {
        0x385de1a5,
        0x7da4,
        0x4a4a,
        {0xb9, 0x07, 0x2e, 0xff, 0x63, 0x7a, 0x45, 0xdd}
    };

    HRESULT hr;
    // machine
    for (int i = 0; i < 5; i++) {
        hr = m->pGPO->Save(TRUE, TRUE, &RegistryId, &MyRandomGuid);
        if (hr == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)) {
			Sleep(100 * (i+1));
			continue;
        }
        break;
    }
    if (!SUCCEEDED(hr)) 
        return hr;
    // user
    for (int i = 0; i < 5; i++) {
        hr = m->pGPO->Save(FALSE, TRUE, &RegistryId, &MyRandomGuid);
        if (hr == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)) {
            Sleep(100 * (i+1));
            continue;
        }
        break;
    }
    if (!SUCCEEDED(hr)) 
        return hr;
    // all ok
    return S_OK;
}

void CGPO::Close()
{
    if (!m->pGPO)
        return;

    m->pGPO->Release();
    m->pGPO = NULL;
}
