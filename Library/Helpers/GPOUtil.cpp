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

bool CGPO::Open(bool bWritable)
{
    if (!SUCCEEDED(CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER, IID_IGroupPolicyObject, (LPVOID*)&m->pGPO))) 
        return false;

    DWORD dwFlags = GPO_OPEN_LOAD_REGISTRY;
    if (!bWritable) 
        dwFlags = GPO_OPEN_READ_ONLY;
    if (!SUCCEEDED(m->pGPO->OpenLocalMachineGPO(dwFlags))) 
        return false;
    return true;
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

bool CGPO::Save()
{
    if (!m->pGPO)
        return false;

    GUID RegistryId = REGISTRY_EXTENSION_GUID;

    GUID MyRandomGuid =
    {
        0x385de1a5,
        0x7da4,
        0x4a4a,
        {0xb9, 0x07, 0x2e, 0xff, 0x63, 0x7a, 0x45, 0xdd}
    };

    HRESULT hr;
    bool bOk = true;
    hr = m->pGPO->Save(TRUE, TRUE, &RegistryId, &MyRandomGuid);
    if (!SUCCEEDED(hr)) bOk = false;
    hr = m->pGPO->Save(FALSE, TRUE, &RegistryId, &MyRandomGuid);
    if (!SUCCEEDED(hr)) bOk = false;
    return bOk;
}

void CGPO::Close()
{
    if (!m->pGPO)
        return;

    m->pGPO->Release();
    m->pGPO = NULL;
}
