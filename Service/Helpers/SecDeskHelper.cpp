/*
* Copyright 2025 David Xanatos, xanasoft.com
*
* This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU Lesser General Public
*   License as published by the Free Software Foundation; either
*   version 3 of the License, or (at your option) any later version.
* 
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*   Lesser General Public License for more details.
* 
*   You should have received a copy of the GNU Lesser General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "pch.h"
#include "SecDeskHelper.h"

#include <wincodec.h> // WIC for PNG loading

#pragma comment(lib, "windowscodecs.lib")

struct MessageBoxParams
{
    std::wstring text;
    std::wstring title;
    UINT uType;
    int result;
    std::wstring backgroundImagePath;
};

struct DialogParams
{
    int(*func)(HWND hWnd, void* param);
    void* param;
    int result;
    std::wstring backgroundImagePath;
};

class CSecureDesktop
{
public:
    CSecureDesktop(const std::wstring& backgroundImagePath)
    {
        // Load the PNG as HBITMAP
        if (!backgroundImagePath.empty())
        {
            m_hBitmap = LoadBitmapFromImage(backgroundImagePath.c_str(), &m_BitmapWidth, &m_BitmapHeight);
        }
    }
    ~CSecureDesktop()
    {
        SwitchToOriginalDesktop();

        if (m_hBitmap)
        {
            DeleteObject(m_hBitmap);
            m_hBitmap = NULL;
        }
    }

    HWND GetBackgroundWnd() const { return m_BackgroundWnd; }

    BOOLEAN SwitchToSecureDesktop()
    {
        if (m_hWinlogonDesktop)
            return TRUE;

        // Save current desktop
        m_hOriginalDesktop = GetThreadDesktop(GetCurrentThreadId());

        // Open Winlogon desktop
        m_hWinlogonDesktop = OpenDesktopW(L"Winlogon", 0, FALSE, GENERIC_ALL);
        if (!m_hWinlogonDesktop)
            return FALSE;

        if (!SetThreadDesktop(m_hWinlogonDesktop) || !SwitchDesktop(m_hWinlogonDesktop))
        {
            CloseDesktop(m_hWinlogonDesktop);
            m_hWinlogonDesktop = NULL;
            return FALSE;
        }

        // Register window class
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = BackgroundWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"BackgroundWindow";

        RegisterClassW(&wc);

        // Create fullscreen window
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        m_BackgroundWnd = CreateWindowExW(
            0, //WS_EX_TOPMOST,
            wc.lpszClassName,
            L"",
            WS_POPUP,
            0, 0, screenW, screenH,
            NULL, NULL, wc.hInstance, this
        );

        if (m_BackgroundWnd)
        {
            ShowWindow(m_BackgroundWnd, SW_SHOW);
            UpdateWindow(m_BackgroundWnd);
        }

        return TRUE;
    }

    VOID SwitchToOriginalDesktop()
    {
        // Cleanup
        if (m_BackgroundWnd) {
            DestroyWindow(m_BackgroundWnd);
            m_BackgroundWnd = NULL;
        }

        if (m_hWinlogonDesktop) {
            SwitchDesktop(m_hOriginalDesktop);
            CloseDesktop(m_hWinlogonDesktop);
            m_hWinlogonDesktop = NULL;
        }
    }

protected:
    // Load PNG file into HBITMAP
    HBITMAP LoadBitmapFromImage(LPCWSTR filename, UINT* outWidth, UINT* outHeight)
    {
        IWICImagingFactory* pFactory = NULL;
        IWICBitmapDecoder* pDecoder = NULL;
        IWICBitmapFrameDecode* pFrame = NULL;
        IWICFormatConverter* pConverter = NULL;
        HBITMAP hBitmap = NULL;

        if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
            return NULL;

        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory))))
        {
            CoUninitialize();
            return NULL;
        }

        if (SUCCEEDED(pFactory->CreateDecoderFromFilename(filename, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder)))
        {
            if (SUCCEEDED(pDecoder->GetFrame(0, &pFrame)))
            {
                if (SUCCEEDED(pFactory->CreateFormatConverter(&pConverter)))
                {
                    if (SUCCEEDED(pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA,
                        WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom)))
                    {
                        HDC hdcScreen = GetDC(NULL);
                        UINT width = 0, height = 0;
                        pConverter->GetSize(&width, &height);

                        if (outWidth) *outWidth = width;
                        if (outHeight) *outHeight = height;

                        BITMAPINFO bmi = {};
                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth = width;
                        bmi.bmiHeader.biHeight = -(LONG)height;
                        bmi.bmiHeader.biPlanes = 1;
                        bmi.bmiHeader.biBitCount = 32;
                        bmi.bmiHeader.biCompression = BI_RGB;

                        void* pvImageBits = NULL;
                        hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pvImageBits, NULL, 0);
                        if (hBitmap && pvImageBits)
                        {
                            UINT stride = width * 4;
                            pConverter->CopyPixels(NULL, stride, stride * height, (BYTE*)pvImageBits);
                        }
                        ReleaseDC(NULL, hdcScreen);
                    }
                    pConverter->Release();
                }
                pFrame->Release();
            }
            pDecoder->Release();
        }
        pFactory->Release();
        CoUninitialize();
        return hBitmap;
    }

    // Background Window Procedure
    static LRESULT CALLBACK BackgroundWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        CSecureDesktop* This = (CSecureDesktop*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

        switch (msg)
        {
        case WM_CREATE: 
        {
            LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
            This = (CSecureDesktop*)pcs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)This);
            break;
        }

        case WM_PAINT:
        {
            if (!This)
                break;

            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Fill the background with a solid color
            HBRUSH brush = CreateSolidBrush(RGB(30, 30, 30)); // Example: dark gray-blue
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);

            if (This->m_hBitmap)
            {
                HDC hMemDC = CreateCompatibleDC(hdc);
                HBITMAP oldBitmap = (HBITMAP)SelectObject(hMemDC, This->m_hBitmap);

                int bmpX = (rc.right - (int)This->m_BitmapWidth) / 2;
                int bmpY = (rc.bottom - (int)This->m_BitmapHeight) / 2;

                BitBlt(
                    hdc,
                    bmpX, bmpY,
                    This->m_BitmapWidth, This->m_BitmapHeight,
                    hMemDC,
                    0, 0,
                    SRCCOPY
                );

                SelectObject(hMemDC, oldBitmap);
                DeleteDC(hMemDC);
            }

            EndPaint(hwnd, &ps);
            break;
        }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

protected:
    HBITMAP m_hBitmap = NULL;
    UINT m_BitmapWidth = 0;
    UINT m_BitmapHeight = 0;
    HWND m_BackgroundWnd = NULL;
    HDESK m_hOriginalDesktop = NULL;
    HDESK m_hWinlogonDesktop = NULL;
};

// Thread to show background + MessageBox
DWORD WINAPI MessageBoxThreadProc(LPVOID lpParam)
{
    MessageBoxParams* params = (MessageBoxParams*)lpParam;

    CSecureDesktop SecureDesktop(params->backgroundImagePath);
    if (SecureDesktop.SwitchToSecureDesktop()) 
        params->result = MessageBoxW(SecureDesktop.GetBackgroundWnd(), params->text.c_str(), params->title.c_str(), params->uType);

    return 0;
}

// Public API
int ShowSecureMessageBox(const std::wstring& text, const std::wstring& title, UINT uType, const std::wstring& backgroundImagePath)
{
    MessageBoxParams params = {};
    params.text = text;
    params.title = title;
    params.uType = uType;
    params.result = -1;
    params.backgroundImagePath = backgroundImagePath;

    HANDLE hThread = CreateThread(NULL, 0, MessageBoxThreadProc, &params, 0, NULL);
    if (!hThread)
        return -1;

    // Wait for the MessageBox to finish
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    return params.result;
}

// Thread to show background + Dialog
DWORD WINAPI DialogThreadProc(LPVOID lpParam)
{
    DialogParams* params = (DialogParams*)lpParam;

    CSecureDesktop SecureDesktop(params->backgroundImagePath);
    if (SecureDesktop.SwitchToSecureDesktop())
        params->result = params->func(SecureDesktop.GetBackgroundWnd(), params->param);

    return 0;
}

int ShowSecureDialog(int(*func)(HWND hWnd, void* param), void* param, const std::wstring& backgroundImagePath)
{
    DialogParams params = {};
    params.func = func;
    params.param = param;
    params.result = -1;
    params.backgroundImagePath = backgroundImagePath;

    HANDLE hThread = CreateThread(NULL, 0,DialogThreadProc, &params, 0, NULL );
    if (!hThread)
        return -1;

    // Wait for the Dialog to finish
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    return params.result;
}

DWORD GetPromptOnSecureDesktop()
{
    HKEY hKey;
    DWORD value = 0;
    DWORD dataSize = sizeof(DWORD);
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, KEY_READ, &hKey );
    if (result == ERROR_SUCCESS)
    {
        result = RegQueryValueExW(hKey, L"PromptOnSecureDesktop", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &dataSize);
        RegCloseKey(hKey);
    }

    if (result != ERROR_SUCCESS)
    {
        // Handle error or return default value (e.g., 1 for secure desktop enabled)
        value = 1;
    }

    return value;
}

/////////////////////////////////////////////////////////////////////////////
// Password Dialog
//

#define IDC_PW_PROMPT       101
#define IDC_PW_EDIT1        102
#define IDC_PW_EDIT2        103
#define IDC_PW_LABEL1       104
#define IDC_PW_LABEL2       105
#define IDC_PW_ERROR        106
#define IDC_PW_SHOW         107

// Helper to get localized string with fallback
static const WCHAR* GetLocalizedStr(const WCHAR* szLocalized, const WCHAR* szDefault)
{
    return (szLocalized && szLocalized[0]) ? szLocalized : szDefault;
}

struct PasswordDlgData
{
    SPasswordPromptSection* pSection;
    HFONT hFont;
    HFONT hFontBold;
    HFONT hFontSymbol;
    bool bShowPassword;

    // Cached localized strings (with fallbacks applied)
    const WCHAR* szPassword;
    const WCHAR* szConfirm;
    const WCHAR* szOk;
    const WCHAR* szCancel;
    const WCHAR* szMismatch;
    const WCHAR* szToLong;
};

// Timer ID for desktop monitoring
#define IDT_CHECK_DESKTOP 1

static INT_PTR CALLBACK PasswordDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PasswordDlgData* pData = (PasswordDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        pData = (PasswordDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)pData);

        // Start timer to periodically check if we're still on the active desktop
        SetTimer(hDlg, IDT_CHECK_DESKTOP, 500, NULL);

        // Initialize localized strings with fallbacks
        pData->szPassword = GetLocalizedStr(pData->pSection->in.szLblPassword, L"Password:");
        pData->szConfirm = GetLocalizedStr(pData->pSection->in.szLblConfirm, L"Confirm:");
        pData->szOk = GetLocalizedStr(pData->pSection->in.szBtnOk, L"OK");
        pData->szCancel = GetLocalizedStr(pData->pSection->in.szBtnCancel, L"Cancel");
        pData->szMismatch = GetLocalizedStr(pData->pSection->in.szErrMismatch, L"Passwords do not match!");
        pData->szToLong = GetLocalizedStr(pData->pSection->in.szErrToLong, L"Password is too long!");

        // Set title and prompt
        SetWindowTextW(hDlg, pData->pSection->in.szTitle);
        SetDlgItemTextW(hDlg, IDC_PW_PROMPT, pData->pSection->in.szPrompt);

        // Set localized labels and buttons
        SetDlgItemTextW(hDlg, IDC_PW_LABEL1, pData->szPassword);
        SetDlgItemTextW(hDlg, IDC_PW_LABEL2, pData->szConfirm);
        SetDlgItemTextW(hDlg, IDOK, pData->szOk);
        SetDlgItemTextW(hDlg, IDCANCEL, pData->szCancel);

        // Get DPI for proper font scaling
        int dpi = 96;
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32)
        {
            // Try GetDpiForWindow (Windows 10 1607+), fall back to GetDpiForSystem
            typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
            typedef UINT(WINAPI* PFN_GetDpiForSystem)(void);

            PFN_GetDpiForWindow pGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(hUser32, "GetDpiForWindow");
            PFN_GetDpiForSystem pGetDpiForSystem = (PFN_GetDpiForSystem)GetProcAddress(hUser32, "GetDpiForSystem");

            if (pGetDpiForWindow)
                dpi = pGetDpiForWindow(hDlg);
            else if (pGetDpiForSystem)
                dpi = pGetDpiForSystem();
        }

        // Scale font sizes based on DPI (base size at 96 DPI)
        int fontHeight = MulDiv(-14, dpi, 96);
        int symbolFontHeight = MulDiv(-16, dpi, 96);

        // Create fonts
        pData->hFont = CreateFontW(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        pData->hFontBold = CreateFontW(fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        // Segoe MDL2 Assets contains the eye icon (U+E7B3 = eye, U+ED1A = eye with line)
        pData->hFontSymbol = CreateFontW(symbolFontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
        pData->bShowPassword = false;

        // Apply fonts
        SendDlgItemMessageW(hDlg, IDC_PW_PROMPT, WM_SETFONT, (WPARAM)pData->hFontBold, TRUE);
        SendDlgItemMessageW(hDlg, IDC_PW_LABEL1, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
        SendDlgItemMessageW(hDlg, IDC_PW_EDIT1, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
        SendDlgItemMessageW(hDlg, IDC_PW_LABEL2, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
        SendDlgItemMessageW(hDlg, IDC_PW_EDIT2, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
        SendDlgItemMessageW(hDlg, IDC_PW_ERROR, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
        SendDlgItemMessageW(hDlg, IDC_PW_SHOW, WM_SETFONT, (WPARAM)pData->hFontSymbol, TRUE);
        SendDlgItemMessageW(hDlg, IDOK, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
        SendDlgItemMessageW(hDlg, IDCANCEL, WM_SETFONT, (WPARAM)pData->hFont, TRUE);

        // Hide confirm field if not in confirm mode
        if (!(pData->pSection->in.uType & PDLG_CONFIRM_PW))
        {
            ShowWindow(GetDlgItem(hDlg, IDC_PW_LABEL2), SW_HIDE);
            ShowWindow(GetDlgItem(hDlg, IDC_PW_EDIT2), SW_HIDE);
        }

        // Hide error label initially
        ShowWindow(GetDlgItem(hDlg, IDC_PW_ERROR), SW_HIDE);

        // Center dialog on screen
        RECT rc;
        GetWindowRect(hDlg, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
        SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

        // Focus password field
        SetFocus(GetDlgItem(hDlg, IDC_PW_EDIT1));
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            wchar_t szPw1[256] = {};
            wchar_t szPw2[256] = {};

            GetDlgItemTextW(hDlg, IDC_PW_EDIT1, szPw1, ARRAYSIZE(szPw1));

            if (wcslen(szPw1) > PW_MAX_LENGTH)
            {
                // Password too long - show error
                SetDlgItemTextW(hDlg, IDC_PW_ERROR, pData->szToLong);
                ShowWindow(GetDlgItem(hDlg, IDC_PW_ERROR), SW_SHOW);

                // Clear both fields
                SetDlgItemTextW(hDlg, IDC_PW_EDIT1, L"");
                if (pData->pSection->in.uType & PDLG_CONFIRM_PW)
                    SetDlgItemTextW(hDlg, IDC_PW_EDIT2, L"");

                SetFocus(GetDlgItem(hDlg, IDC_PW_EDIT1));
                SecureZeroMemory(szPw1, sizeof(szPw1));
                return TRUE;
			}

            // In confirm mode, validate passwords match
            if (pData->pSection->in.uType & PDLG_CONFIRM_PW)
            {
                GetDlgItemTextW(hDlg, IDC_PW_EDIT2, szPw2, ARRAYSIZE(szPw2));

                if (wcscmp(szPw1, szPw2) != 0)
                {
                    // Passwords don't match - show error
                    SetDlgItemTextW(hDlg, IDC_PW_ERROR, pData->szMismatch);
                    ShowWindow(GetDlgItem(hDlg, IDC_PW_ERROR), SW_SHOW);

                    // Clear both fields
                    SetDlgItemTextW(hDlg, IDC_PW_EDIT1, L"");
                    SetDlgItemTextW(hDlg, IDC_PW_EDIT2, L"");
                    SetFocus(GetDlgItem(hDlg, IDC_PW_EDIT1));

                    SecureZeroMemory(szPw1, sizeof(szPw1));
                    SecureZeroMemory(szPw2, sizeof(szPw2));
                    return TRUE;
                }
                SecureZeroMemory(szPw2, sizeof(szPw2));
            }

            // Copy password to section output
            wcsncpy_s(pData->pSection->out.szPassword, ARRAYSIZE(pData->pSection->out.szPassword), szPw1, _TRUNCATE);
            pData->pSection->out.status = STATUS_SUCCESS;

            SecureZeroMemory(szPw1, sizeof(szPw1));
            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        case IDCANCEL:
            pData->pSection->out.status = STATUS_CANCELLED;
            EndDialog(hDlg, IDCANCEL);
            return TRUE;

        case IDC_PW_SHOW:
        {
            // Toggle password visibility
            pData->bShowPassword = !pData->bShowPassword;

            // Update the button text (eye icon)
            // U+E7B3 = RedEye (show), U+ED1A = Hide (eye with slash)
            SetDlgItemTextW(hDlg, IDC_PW_SHOW, pData->bShowPassword ? L"\xED1A" : L"\xE7B3");

            // Toggle password char on edit controls
            SendDlgItemMessageW(hDlg, IDC_PW_EDIT1, EM_SETPASSWORDCHAR, pData->bShowPassword ? 0 : L'\x25CF', 0);
            InvalidateRect(GetDlgItem(hDlg, IDC_PW_EDIT1), NULL, TRUE);

            if (pData->pSection->in.uType & PDLG_CONFIRM_PW)
            {
                SendDlgItemMessageW(hDlg, IDC_PW_EDIT2, EM_SETPASSWORDCHAR, pData->bShowPassword ? 0 : L'\x25CF', 0);
                InvalidateRect(GetDlgItem(hDlg, IDC_PW_EDIT2), NULL, TRUE);
            }
            return TRUE;
        }
        }
        break;

    case WM_TIMER:
        if (wParam == IDT_CHECK_DESKTOP)
        {
            // Check if the current desktop is still the input desktop
            // If user pressed Ctrl+Alt+Del, the input desktop changes
            HDESK hInputDesktop = OpenInputDesktop(0, FALSE, DESKTOP_SWITCHDESKTOP);
            if (hInputDesktop)
            {
                HDESK hCurrentDesktop = GetThreadDesktop(GetCurrentThreadId());

                // Try to switch to our desktop - if it fails, we're no longer the input desktop
                if (!SwitchDesktop(hCurrentDesktop))
                {
                    CloseDesktop(hInputDesktop);
                    // Desktop switched away - cancel the dialog (only if not already completed)
                    if (pData && pData->pSection->out.status != STATUS_SUCCESS)
                    {
                        pData->pSection->out.status = STATUS_CANCELLED;
                        KillTimer(hDlg, IDT_CHECK_DESKTOP);
                        EndDialog(hDlg, IDCANCEL);
                        return TRUE;
                    }
                }
                CloseDesktop(hInputDesktop);
            }
            return TRUE;
        }
        break;

    case WM_ACTIVATE:
        // If we're being deactivated and it's not due to a child window, cancel
        // But only if we haven't already completed successfully (e.g., user clicked OK)
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            // Check if we lost activation to something outside our dialog
            HWND hWndOther = (HWND)lParam;
            if (hWndOther == NULL || GetParent(hWndOther) != hDlg)
            {
                // Only cancel if not already successfully completed
                if (pData && pData->pSection->out.status != STATUS_SUCCESS)
                {
                    pData->pSection->out.status = STATUS_CANCELLED;
                    KillTimer(hDlg, IDT_CHECK_DESKTOP);
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
                }
            }
        }
        break;

    case WM_DESTROY:
        KillTimer(hDlg, IDT_CHECK_DESKTOP);
        if (pData)
        {
            if (pData->hFont) DeleteObject(pData->hFont);
            if (pData->hFontBold) DeleteObject(pData->hFontBold);
            if (pData->hFontSymbol) DeleteObject(pData->hFontSymbol);
        }
        break;
    }
    return FALSE;
}

// Helper to align DWORD for dialog templates
static LPWORD AlignToDWORD(LPWORD ptr)
{
    return (LPWORD)(((ULONG_PTR)ptr + 3) & ~3);
}

// Helper to write a string to dialog template
static LPWORD WriteString(LPWORD ptr, LPCWSTR str)
{
    while (*str) *ptr++ = *str++;
    *ptr++ = 0;
    return ptr;
}

static INT_PTR CreatePasswordDialogTemplate(HWND hParent, PasswordDlgData* pData)
{
    bool bConfirmMode = (pData->pSection->in.uType & PDLG_CONFIRM_PW) != 0;

    // Dialog dimensions (in dialog units)
    const int DLG_WIDTH = 280;
    const int DLG_HEIGHT = bConfirmMode ? 140 : 110;
    const int MARGIN = 10;
    const int LABEL_WIDTH = 70;
    const int EYE_BTN_WIDTH = 18;
    const int EDIT_WIDTH = DLG_WIDTH - MARGIN * 2 - LABEL_WIDTH - 5 - EYE_BTN_WIDTH - 2;
    const int CTRL_HEIGHT = 14;
    const int BTN_WIDTH = 60;
    const int BTN_HEIGHT = 18;

    // Build dialog template in memory
    BYTE buffer[4096] = {};
    LPDLGTEMPLATEW pDlg = (LPDLGTEMPLATEW)buffer;

    pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    pDlg->dwExtendedStyle = 0;
    pDlg->cdit = bConfirmMode ? 9 : 7; // Number of controls (+1 for eye button)
    pDlg->x = 0; pDlg->y = 0;
    pDlg->cx = DLG_WIDTH; pDlg->cy = DLG_HEIGHT;

    LPWORD ptr = (LPWORD)(pDlg + 1);
    *ptr++ = 0; // menu
    *ptr++ = 0; // class
    *ptr++ = 0; // title (set via WM_INITDIALOG)

    int yPos = MARGIN;

    // Prompt label (static text)
    ptr = AlignToDWORD(ptr);
    LPDLGITEMTEMPLATEW pItem = (LPDLGITEMTEMPLATEW)ptr;
    pItem->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    pItem->x = MARGIN; pItem->y = yPos;
    pItem->cx = DLG_WIDTH - MARGIN * 2; pItem->cy = CTRL_HEIGHT * 2;
    pItem->id = IDC_PW_PROMPT;
    ptr = (LPWORD)(pItem + 1);
    *ptr++ = 0xFFFF; *ptr++ = 0x0082; // Static
    *ptr++ = 0; // text
    *ptr++ = 0; // creation data

    yPos += CTRL_HEIGHT * 2 + 8;

    // Password label
    ptr = AlignToDWORD(ptr);
    pItem = (LPDLGITEMTEMPLATEW)ptr;
    pItem->style = WS_CHILD | WS_VISIBLE | SS_RIGHT;
    pItem->x = MARGIN; pItem->y = yPos + 2;
    pItem->cx = LABEL_WIDTH; pItem->cy = CTRL_HEIGHT;
    pItem->id = IDC_PW_LABEL1;
    ptr = (LPWORD)(pItem + 1);
    *ptr++ = 0xFFFF; *ptr++ = 0x0082; // Static
    *ptr++ = 0; // text (set dynamically)
    *ptr++ = 0; // creation data

    // Password edit
    ptr = AlignToDWORD(ptr);
    pItem = (LPDLGITEMTEMPLATEW)ptr;
    pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL;
    pItem->x = MARGIN + LABEL_WIDTH + 5; pItem->y = yPos;
    pItem->cx = EDIT_WIDTH; pItem->cy = CTRL_HEIGHT;
    pItem->id = IDC_PW_EDIT1;
    ptr = (LPWORD)(pItem + 1);
    *ptr++ = 0xFFFF; *ptr++ = 0x0081; // Edit
    *ptr++ = 0;
    *ptr++ = 0;

    // Show password button (eye icon)
    ptr = AlignToDWORD(ptr);
    pItem = (LPDLGITEMTEMPLATEW)ptr;
    pItem->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_CENTER;
    pItem->x = MARGIN + LABEL_WIDTH + 5 + EDIT_WIDTH + 2; pItem->y = yPos;
    pItem->cx = EYE_BTN_WIDTH; pItem->cy = CTRL_HEIGHT;
    pItem->id = IDC_PW_SHOW;
    ptr = (LPWORD)(pItem + 1);
    *ptr++ = 0xFFFF; *ptr++ = 0x0080; // Button
    // U+E7B3 = RedEye icon in Segoe MDL2 Assets
    *ptr++ = 0xE7B3; *ptr++ = 0;
    *ptr++ = 0;

    yPos += CTRL_HEIGHT + 6;

    if (bConfirmMode)
    {
        // Confirm label
        ptr = AlignToDWORD(ptr);
        pItem = (LPDLGITEMTEMPLATEW)ptr;
        pItem->style = WS_CHILD | WS_VISIBLE | SS_RIGHT;
        pItem->x = MARGIN; pItem->y = yPos + 2;
        pItem->cx = LABEL_WIDTH; pItem->cy = CTRL_HEIGHT;
        pItem->id = IDC_PW_LABEL2;
        ptr = (LPWORD)(pItem + 1);
        *ptr++ = 0xFFFF; *ptr++ = 0x0082; // Static
        *ptr++ = 0; // text (set dynamically)
        *ptr++ = 0;

        // Confirm edit
        ptr = AlignToDWORD(ptr);
        pItem = (LPDLGITEMTEMPLATEW)ptr;
        pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL;
        pItem->x = MARGIN + LABEL_WIDTH + 5; pItem->y = yPos;
        pItem->cx = EDIT_WIDTH; pItem->cy = CTRL_HEIGHT;
        pItem->id = IDC_PW_EDIT2;
        ptr = (LPWORD)(pItem + 1);
        *ptr++ = 0xFFFF; *ptr++ = 0x0081; // Edit
        *ptr++ = 0;
        *ptr++ = 0;

        yPos += CTRL_HEIGHT + 6;
    }

    // Error label (hidden initially)
    ptr = AlignToDWORD(ptr);
    pItem = (LPDLGITEMTEMPLATEW)ptr;
    pItem->style = WS_CHILD | SS_CENTER; // Not visible initially
    pItem->x = MARGIN; pItem->y = yPos;
    pItem->cx = DLG_WIDTH - MARGIN * 2; pItem->cy = CTRL_HEIGHT;
    pItem->id = IDC_PW_ERROR;
    ptr = (LPWORD)(pItem + 1);
    *ptr++ = 0xFFFF; *ptr++ = 0x0082; // Static
    *ptr++ = 0;
    *ptr++ = 0;

    yPos += CTRL_HEIGHT + 8;

    // OK button
    ptr = AlignToDWORD(ptr);
    pItem = (LPDLGITEMTEMPLATEW)ptr;
    pItem->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
    pItem->x = DLG_WIDTH / 2 - BTN_WIDTH - 5;
    pItem->y = yPos;
    pItem->cx = BTN_WIDTH; pItem->cy = BTN_HEIGHT;
    pItem->id = IDOK;
    ptr = (LPWORD)(pItem + 1);
    *ptr++ = 0xFFFF; *ptr++ = 0x0080; // Button
    *ptr++ = 0; // text (set dynamically)
    *ptr++ = 0;

    // Cancel button
    ptr = AlignToDWORD(ptr);
    pItem = (LPDLGITEMTEMPLATEW)ptr;
    pItem->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
    pItem->x = DLG_WIDTH / 2 + 5;
    pItem->y = yPos;
    pItem->cx = BTN_WIDTH; pItem->cy = BTN_HEIGHT;
    pItem->id = IDCANCEL;
    ptr = (LPWORD)(pItem + 1);
    *ptr++ = 0xFFFF; *ptr++ = 0x0080; // Button
    *ptr++ = 0; // text (set dynamically)
    *ptr++ = 0;

    return DialogBoxIndirectParamW(GetModuleHandleW(NULL), pDlg, hParent, PasswordDlgProc, (LPARAM)pData);
}

struct PasswordDialogParams
{
    SPasswordPromptSection* pSection;
    std::wstring backgroundImagePath;
    int result;
};

static DWORD WINAPI PasswordDialogThreadProc(LPVOID lpParam)
{
    PasswordDialogParams* params = (PasswordDialogParams*)lpParam;

    CSecureDesktop SecureDesktop(params->backgroundImagePath);
    if (SecureDesktop.SwitchToSecureDesktop())
    {
        PasswordDlgData data = {};
        data.pSection = params->pSection;
        params->result = (int)CreatePasswordDialogTemplate(SecureDesktop.GetBackgroundWnd(), &data);
    }

    return 0;
}

int ShowSecurePasswordDialog(HWND hParent, SPasswordPromptSection* pSection, const std::wstring& backgroundImagePath)
{
    // Initialize output
    pSection->out.status = STATUS_UNSUCCESSFUL;
    SecureZeroMemory(pSection->out.szPassword, sizeof(pSection->out.szPassword));

    // Ensure title has a default value
    if (pSection->in.szTitle[0] == L'\0')
        wcscpy_s(pSection->in.szTitle, L"MajorPrivacy");

    PasswordDialogParams params = {};
    params.pSection = pSection;
    params.backgroundImagePath = backgroundImagePath;
    params.result = -1;

    HANDLE hThread = CreateThread(NULL, 0, PasswordDialogThreadProc, &params, 0, NULL);
    if (!hThread)
        return -1;

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    return params.result;
}