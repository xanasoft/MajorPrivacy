/*
 * Copyright 2023-2026 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "framework.h"
#include <shellapi.h>
#include "ImBox.h"
#include "ImDiskIO.h"
#include "VirtualMemoryIO.h"
#include "PhysicalMemoryIO.h"
#include "ImageFileIO.h"
#include "CryptoIO.h"
#include "Common\helpers.h"

extern "C" {
/* dc\include and DiskCryptor's crypto_lib / volume_lib are on the include path.
   Only dc_init_crypto is used here; the crypto headers this used to pull in
   were never referenced. */
#include "dc_header.h"
}


bool HasFlag(const std::vector<std::wstring>& arguments, std::wstring name)
{
	return std::find(arguments.begin(), arguments.end(), name) != arguments.end();
}

std::wstring GetArgument(const std::vector<std::wstring>& arguments, std::wstring name) 
{
	std::wstring prefix = name + L"=";
	for (size_t i = 0; i < arguments.size(); i++) {
		if (_wcsicmp(arguments[i].substr(0, prefix.length()).c_str(), prefix.c_str()) == 0) {
			return arguments[i].substr(prefix.length());
		}
	}
	return L"";
}

struct SArgument
{
    bool IsSet = false;
    std::wstring Value;
};

SArgument GetArgumentEx(const std::vector<std::wstring>& arguments, std::wstring name)
{
    SArgument arg;
    for (size_t i = 0; i < arguments.size(); i++) {
        //
        // The name has to be the whole argument or be followed by '='. Matching
        // on the prefix alone made "slot" answer for "slots=4" - and hand back
        // "=4" as its value, which reads as slot 0.
        //
        if (_wcsnicmp(arguments[i].c_str(), name.c_str(), name.length()) != 0)
            continue;
        if (arguments[i].length() == name.length()) {
            arg.IsSet = true;
            return arg;
        }
        if (arguments[i][name.length()] != L'=')
            continue;
        arg.IsSet = true;
        arg.Value = arguments[i].substr(name.length() + 1);
        return arg;
    }
    return arg;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    int ret = 0;

	int nArgs = 0;
	LPWSTR* szArglist = CommandLineToArgvW(lpCmdLine, &nArgs);
	std::vector<std::wstring> arguments;
	for (int i = 0; i < nArgs; i++)
		arguments.push_back(szArglist[i]);
	LocalFree(szArglist);

    // type=image image="c:\temp\test.img" mount=Z: format=ntfs:Test key=test
    // image="c:\temp\test.img" key=test new_key=123

    //
    // Key slots. "key" always names the credential that opens the container,
    // "new_key" / "new_slot" say what to do with the slots:
    //
    //   slots=N                                   key slots to lay out when creating (default 4, 0 for none)
    //   key=<header pw> new_key=<pw> new_slot=N   set the password of key slot N
    //   key=<header pw> new_key= new_slot=N       erase key slot N
    //   key=<header pw> new_key=<pw>              change the header password; every
    //                                             slot is re-wrapped and keeps working
    //   key=<slot pw>                             mount through a key slot
    //
    // Header editor (unmounted containers, section= required):
    //   key=<header pw> get_header sec_size=<n>   decrypted header into the section
    //   key=<header pw> set_header sec_size=<n>   sealed header out of the section
    //
    // The password for either may be a raw derived key - kdf=-1 with a 256 byte
    // dc_pass - which is how the editor avoids paying for Argon2 twice, exactly
    // as DiskCryptor's GUI does when it talks to its driver.
    //
    // Editing slots needs the header password - a slot password cannot re-wrap
    // another slot. slot=N restricts the search on the way in, slot is -100 by
    // default, which tries the header password and then every slot.
    //

    //bool bCreate = HasFlag(arguments, L"create");
    //bool bUpdate = HasFlag(arguments, L"update");

    std::wstring mount = GetArgument(arguments, L"mount");
    LONG number = _wtoi(GetArgument(arguments, L"number").c_str());
    std::wstring type = GetArgument(arguments, L"type");
    std::wstring size = GetArgument(arguments, L"size");
    std::wstring image = GetArgument(arguments, L"image");
    SArgument key = GetArgumentEx(arguments, L"key");
    std::wstring cipher = GetArgument(arguments, L"cipher");
    //int iCost = _wtoi(GetArgument(arguments, L"cost").c_str());
    //int iNewCost = _wtoi(GetArgument(arguments, L"new_cost").c_str());
    int iKdf = _wtoi(GetArgument(arguments, L"kdf").c_str());
    int iNewKdf = _wtoi(GetArgument(arguments, L"new_kdf").c_str());
    SArgument slot = GetArgumentEx(arguments, L"slot");
    SArgument new_slot = GetArgumentEx(arguments, L"new_slot");
    SArgument slots = GetArgumentEx(arguments, L"slots");
    int iSlots = slots.IsSet ? _wtoi(slots.Value.c_str()) : CRYPTO_SLOTS_DEFAULT;
    std::wstring format = GetArgument(arguments, L"format");
    std::wstring params = GetArgument(arguments, L"params");

    SArgument new_key = GetArgumentEx(arguments, L"new_key");
    std::wstring backup = GetArgument(arguments, L"backup");
    std::wstring restore = GetArgument(arguments, L"restore");

    // header editor: read the volume header into the section, or take one back
    bool bGetHeader = HasFlag(arguments, L"get_header");
    bool bSetHeader = HasFlag(arguments, L"set_header");

    std::wstring proxy = GetArgument(arguments, L"proxy");
    std::wstring event = GetArgument(arguments, L"event");
    std::wstring section = GetArgument(arguments, L"section");
    std::wstring mem = GetArgument(arguments, L"mem");

    //SArgument set_data = GetArgumentEx(arguments, L"set_data");
    //SArgument get_data = GetArgumentEx(arguments, L"get_data");

    ULONG64 uSize = 0;
    if(size.empty())
        uSize = 2ull * (1024 * 1024 * 1024); // 2GB;
    else
        uSize = _wtoi64(size.c_str());

    SSection* pSection = NULL;
    if(!mem.empty()) {
        pSection = (SSection*)wcstoull(mem.c_str()+2, NULL, 16);
    }

    //
    // How much of the mapping to map. A view does not know its own size, and
    // the header commands need one that is sizeof(SSection) plus a whole
    // volume header - see SHeaderData in ImBox.h. Everything else still wants
    // exactly the 4 KiB it always did.
    //
    std::wstring sec_size = GetArgument(arguments, L"sec_size");
    ULONG uSecSize = sizeof(SSection);
    if (!sec_size.empty()) {
        uSecSize = (ULONG)wcstoul(sec_size.c_str(), NULL, 0);
        if (uSecSize < sizeof(SSection))
            return ERR_INVALID_PARAM;
    }

    HANDLE hMapping = NULL;
    if (!section.empty()) {
        if (pSection)
            return ERR_INVALID_PARAM;
        if(section.substr(0, 2) == L"0x")
            hMapping = (SSection*)wcstoull(section.c_str() + 2, NULL, 16);
        else
            hMapping = OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, section.c_str());
        if (hMapping)
            pSection = (SSection*)MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, uSecSize);
    }

    SPassword* pass = NULL;

    //
    // Prepare Crypto
    //

    dc_init_crypto();

    //
    // prepare disk IO
    //

    CAbstractIO* pIO = NULL;
    if (_wcsicmp(type.c_str(), L"virtual") == 0 || _wcsicmp(type.c_str(), L"ram") == 0)
        pIO = new CVirtualMemoryIO(uSize);
    else if (_wcsicmp(type.c_str(), L"physical") == 0 || _wcsicmp(type.c_str(), L"awe") == 0)
        pIO = new CPhysicalMemoryIO(uSize);
    else if(!image.empty()) // else if (_wcsicmp(type.c_str(), L"image") == 0 || _wcsicmp(type.c_str(), L"img") == 0)
        pIO = new CImageFileIO(image, uSize);
    else {
        ret = ERR_UNKNOWN_TYPE;
        goto cleanup;
    }

    if (key.IsSet && !key.Value.empty())
    {
        pass = (SPassword*)secure_alloc(sizeof(SPassword));
        pass->size = (int)(key.Value.length() * sizeof(WCHAR));
        wcsncpy(pass->pass, key.Value.c_str(), DC_MAX_PASSWORD);
        pass->kdf = iKdf;
		pass->slot = slot.IsSet ? _wtoi(slot.Value.c_str()) : -KEY_SLOT_MAX;
    }
    else if(pSection)
    {
        pass = (SPassword*)secure_alloc(sizeof(SPassword));
		memcpy(pass, &pSection->in.pw, sizeof(SPassword));
		burn(&pSection->in.pw, sizeof(SPassword));
    }

    if (!backup.empty()) {
        ret = CCryptoIO::BackupHeader(pIO, backup, pass);
        goto cleanup;
    }
    if (!restore.empty()) {
        ret = CCryptoIO::RestoreHeader(pIO, restore, pass);
        goto cleanup;
    }

    //
    // The header editor's two halves. Both need the section - the header does
    // not fit on a command line and has no business on one either.
    //
    if (bGetHeader || bSetHeader) {
        if (!pSection || !pass) {
            ret = ERR_INVALID_PARAM;
            goto cleanup;
        }
        ret = bGetHeader ? CCryptoIO::GetHeader(pIO, pSection, uSecSize, pass)
                         : CCryptoIO::SetHeader(pIO, pSection, uSecSize, pass);
        goto cleanup;
    }

    if (pass) 
    {
        CCryptoIO* pCrypto = new CCryptoIO(pIO, pass, cipher, iSlots);    
        pIO = pCrypto;

		secure_free(pass);
        pass = NULL;

        if (new_key.IsSet) 
        {
			SPassword* new_pass = NULL;

            //
            // An empty new_key is not nothing when a new_slot comes with it:
            // a key slot is erased by handing ChangePassword a zero length
            // password for that slot.
            //
            if (!new_key.Value.empty() || (new_slot.IsSet && _wtoi(new_slot.Value.c_str()) != 0))
            {
                new_pass = (SPassword*)secure_alloc(sizeof(SPassword));
				new_pass->size = (int)(new_key.Value.length() * sizeof(WCHAR));
				wcsncpy(new_pass->pass, new_key.Value.c_str(), DC_MAX_PASSWORD);
				new_pass->kdf = iNewKdf;
                new_pass->slot = new_slot.IsSet ? _wtoi(new_slot.Value.c_str()) : 0;
            }
            else if (pSection && pSection->id == SECTION_PARAM_ID_NEW_PASS) {
                new_pass = (SPassword*)secure_alloc(sizeof(SPassword));
				memcpy(new_pass, pSection->data, sizeof(SPassword));
				burn(pSection->data, sizeof(SPassword));
            }
            
            if(!new_pass) {
                ret = ERR_INVALID_PARAM;
                goto cleanup;
            }

            ret = pCrypto->ChangePassword(new_pass);

			secure_free(new_pass);

            goto cleanup;
        }

  //      if (set_data.IsSet) 
  //      {
  //          const UCHAR* pData = NULL;
  //          SIZE_T uSize = 0;

  //          if (!set_data.Value.empty()) {
  //              pData = (const UCHAR*)set_data.Value.c_str();
  //              uSize = set_data.Value.length() * sizeof(WCHAR);
  //          } else if (pSection && pSection->id == SECTION_PARAM_ID_DATA) {
  //              pData = pSection->data;
		//		uSize = pSection->size;
		//	} else
		//		return ERR_INVALID_PARAM;
  //          
  //          return pCrypto->SetData(pData, uSize);
  //      }
  //      else if(get_data.IsSet) 
  //      {
		//	UCHAR pData[1024];
		//	SIZE_T uSize = 0;

		//	int ret = pCrypto->GetData(pData, &uSize);
		//	if (ret)
		//		return ret;

		//	if (pSection) {
  //              pSection->id = SECTION_PARAM_ID_DATA;
		//		pSection->size = (USHORT)uSize;
		//		memcpy(pSection->data, pData, uSize);
		//	} else
		//		wprintf(L"\n%.*s\n", (int)(uSize / sizeof(WCHAR)), (WCHAR*)pData);

		//	return ERR_OK;
		//}

        if (pSection)
            pCrypto->SetDataSection(pSection);
    }

    ret = pIO->Init();
    if (ret == ERR_OK)
    {
        CImDiskIO* pImDisk = new CImDiskIO(pIO, mount, number, format, params);

        if (!proxy.empty())
            pImDisk->SetProxyName(proxy);

        if (!event.empty()) {
            HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, event.c_str());
            if (hEvent)
                pImDisk->SetMountEvent(hEvent);
        }

        if (pSection) {
            pImDisk->SetMountSection(hMapping, pSection);
            hMapping = NULL;
            pSection = NULL;
        }

        //
        // define shutdown behaviour
        // 

        SetProcessShutdownParameters(0x100, 0);

        //
        // start processing IO Requests
        //

        ret = pImDisk->DoComm();
    }

cleanup:
    if (pass) secure_free(pass);
    if (pSection) UnmapViewOfFile(pSection);
    if(hMapping) CloseHandle(hMapping);
    return ret;
}
