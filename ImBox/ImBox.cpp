/*
 * Copyright 2023 David Xanatos, xanasoft.com
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
        if (_wcsicmp(arguments[i].substr(0, name.length()).c_str(), name.c_str()) == 0) {
            arg.IsSet = true;
            if(arguments[i].length() > name.length() + 1)
                arg.Value = arguments[i].substr(name.length() + 1);
            return arg;
        }
    }
    return arg;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
	int nArgs = 0;
	LPWSTR* szArglist = CommandLineToArgvW(lpCmdLine, &nArgs);
	std::vector<std::wstring> arguments;
	for (int i = 0; i < nArgs; i++)
		arguments.push_back(szArglist[i]);
	LocalFree(szArglist);

    // type=image image="c:\temp\test.img" mount=Z: format=ntfs:Test key=test
    // image="c:\temp\test.img" key=test new_key=123

    //bool bCreate = HasFlag(arguments, L"create");
    //bool bUpdate = HasFlag(arguments, L"update");

    std::wstring mount = GetArgument(arguments, L"mount");
    UINT number = _wtoi(GetArgument(arguments, L"number").c_str());
    std::wstring type = GetArgument(arguments, L"type");
    std::wstring size = GetArgument(arguments, L"size");
    std::wstring image = GetArgument(arguments, L"image");
    std::wstring key = GetArgument(arguments, L"key");
    std::wstring cipher = GetArgument(arguments, L"cipher");
    std::wstring format = GetArgument(arguments, L"format");
    std::wstring params = GetArgument(arguments, L"params");

    SArgument new_key = GetArgumentEx(arguments, L"new_key");
    std::wstring backup = GetArgument(arguments, L"backup");
    std::wstring restore = GetArgument(arguments, L"restore");

    std::wstring proxy = GetArgument(arguments, L"proxy");
    std::wstring event = GetArgument(arguments, L"event");
    std::wstring section = GetArgument(arguments, L"section");
    std::wstring mem = GetArgument(arguments, L"mem");

    SArgument set_data = GetArgumentEx(arguments, L"set_data");
    SArgument get_data = GetArgumentEx(arguments, L"get_data");

    ULONG64 uSize = 0;
    if(size.empty())
        uSize = 2ull * (1024 * 1024 * 1024); // 2GB;
    else
        uSize = _wtoi64(size.c_str());

    //
    // prepare disk IO
    //

    CAbstractIO* pIO = NULL;
    if (_wcsicmp(type.c_str(), L"virtual") == 0 || _wcsicmp(type.c_str(), L"ram") == 0)
        pIO = new CVirtualMemoryIO(uSize);
    else if (_wcsicmp(type.c_str(), L"physical") == 0 || _wcsicmp(type.c_str(), L"awe") == 0)
        pIO = new CPhysicalMemoryIO(uSize);
    else if (_wcsicmp(type.c_str(), L"image") == 0 || _wcsicmp(type.c_str(), L"img") == 0)
        pIO = new CImageFileIO(image, uSize);
    else {
        DbgPrint(L"Invalid disk type.\n");
        return -1;
    }

    if (!backup.empty())
        return CCryptoIO::BackupHeader(pIO, backup);
    if (!restore.empty())
        return CCryptoIO::RestoreHeader(pIO, restore);

    HANDLE hMapping = NULL;
    SSection* pSection = NULL;
    if (!section.empty()) {
        HANDLE hMapping = OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, section.c_str());
        if (hMapping)
            pSection = (SSection*)MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, 0x1000);
    }
    if(!mem.empty()) {
		if (pSection)
			return ERR_INVALID_PARAM;
		pSection = (SSection*)wcstoull(mem.c_str()+2, NULL, 16);
	}

    if (!key.empty() || !section.empty()) {
#ifdef ENCLAVE_ENABLED
        if (!IsEnclaveTypeSupported(ENCLAVE_TYPE_VBS))
        {
            OutputDebugString(L"Enclave not supported!\n");
            ExitProcess(STATUS_NOT_SUPPORTED);
        }
#endif //  ENCLAVE_ENABLED
#ifdef ENCLAVE_ENABLED
        // Create the enclave
        DWORD ret = 0;
        if (ret == ERR_OK) {
            constexpr ENCLAVE_CREATE_INFO_VBS CreateInfo
            {
                //ENCLAVE_VBS_FLAG_DEBUG, // Flags
                0,
                { 0x10, 0x22, 0x30, 0x45, 0x41, 0x37, 0x21, 0x13 }, // OwnerID
            };
            Enclave = CreateEnclave(GetCurrentProcess(),
                nullptr, // Preferred base address
                0x10000000, // size
                0,
                ENCLAVE_TYPE_VBS,
                &CreateInfo,
                sizeof(ENCLAVE_CREATE_INFO_VBS),
                nullptr);
        }
        if (Enclave == NULL) {
            DbgPrint(L"CreateEnclave failed\n");
            ret = ERR_INTERNAL;
        }
        if (ret == ERR_OK)
            if (LoadEnclaveImageW(Enclave, L"ImBoxEnclave.dll") == FALSE)
                ret = ERR_INTERNAL;
        if (ret == ERR_OK) {
            ENCLAVE_INIT_INFO_VBS InitInfo{};

            InitInfo.Length = sizeof(ENCLAVE_INIT_INFO_VBS);
            InitInfo.ThreadCount = 1;
            if (InitializeEnclave(GetCurrentProcess(),
                Enclave,
                &InitInfo,
                InitInfo.Length,
                nullptr) == 0) {
                ret = ERR_INTERNAL;
            }
        }


#endif
        CCryptoIO* pCrypto;
        if (key.empty()) {
            if (!pSection)
                return ERR_KEY_REQUIRED;
            pCrypto = new CCryptoIO(pIO, pSection->in.pass, cipher);
            memset(pSection, 0, sizeof(pSection->buffer)); // clear
        }
        else
            pCrypto = new CCryptoIO(pIO, key.c_str(), cipher);
        pIO = pCrypto;

        if (new_key.IsSet) {
            if (!new_key.Value.empty())
                return pCrypto->ChangePassword(new_key.Value.c_str());
            else if (pSection && pSection->magic == SECTION_MAGIC && pSection->id == SECTION_PARAM_ID_KEY)
                return pCrypto->ChangePassword((const WCHAR*)pSection->data);
			else
				return ERR_INVALID_PARAM;
        }

        if (set_data.IsSet) 
        {
            const UCHAR* pData = NULL;
            SIZE_T uSize = 0;

            if (!set_data.Value.empty()) {
                pData = (const UCHAR*)set_data.Value.c_str();
                uSize = set_data.Value.length() * sizeof(WCHAR);
            } else if (pSection && pSection->magic == SECTION_MAGIC && pSection->id == SECTION_PARAM_ID_DATA) {
                pData = pSection->data;
				uSize = pSection->size;
			} else
				return ERR_INVALID_PARAM;
            
            return pCrypto->SetData(pData, uSize);
        }
        else if(get_data.IsSet) 
        {
			UCHAR pData[1024];
			SIZE_T uSize = 0;

			int ret = pCrypto->GetData(pData, &uSize);
			if (ret)
				return ret;

			if (pSection) {
                pSection->magic = SECTION_MAGIC;
                pSection->id = SECTION_PARAM_ID_DATA;
				pSection->size = (USHORT)uSize;
				memcpy(pSection->data, pData, uSize);
			} else
				wprintf(L"\n%.*s\n", (int)(uSize / sizeof(WCHAR)), (WCHAR*)pData);

			return ERR_OK;
		}

        if (pSection)
            pCrypto->SetDataSection(pSection);
    }

    int ret = pIO ? pIO->Init() : ERR_UNKNOWN_TYPE;
    if (ret)
        return ret;


    CImDiskIO* pImDisk = new CImDiskIO(pIO, mount, number, format, params);

    if (!proxy.empty())
        pImDisk->SetProxyName(proxy);

    if (!event.empty()) {
        HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, event.c_str());
        if(hEvent) 
            pImDisk->SetMountEvent(hEvent);
    }

    if (pSection)
        pImDisk->SetMountSection(hMapping, pSection);

    //
    // define shutdown behaviour
    // 

    SetProcessShutdownParameters(0x100, 0);

    //
    // start processing IO Requests
    //

    return pImDisk->DoComm();
}


#if 0
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_IMBOX, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_IMBOX));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IMBOX));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_IMBOX);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
#endif