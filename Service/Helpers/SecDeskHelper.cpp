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
    HANDLE hEvent;
    std::wstring backgroundImagePath;
};

// Globals
HBITMAP g_hBitmap = NULL;
UINT g_BitmapWidth = 0;
UINT g_BitmapHeight = 0;
HWND g_BackgroundWnd = NULL;

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
LRESULT CALLBACK BackgroundWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Fill the background with a solid color
        HBRUSH brush = CreateSolidBrush(RGB(30, 30, 30)); // Example: dark gray-blue
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);

        if (g_hBitmap)
        {
            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(hMemDC, g_hBitmap);

            int bmpX = (rc.right - (int)g_BitmapWidth) / 2;
            int bmpY = (rc.bottom - (int)g_BitmapHeight) / 2;

            BitBlt(
                hdc,
                bmpX, bmpY,
                g_BitmapWidth, g_BitmapHeight,
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

// Thread to show background + MessageBox
DWORD WINAPI MessageBoxThreadProc(LPVOID lpParam)
{
    MessageBoxParams* params = (MessageBoxParams*)lpParam;

    // Save current desktop
    HDESK hOriginalDesktop = GetThreadDesktop(GetCurrentThreadId());

    // Open Winlogon desktop
    HDESK hWinlogonDesktop = OpenDesktopW(L"Winlogon", 0, FALSE, GENERIC_ALL);
    //if (!hWinlogonDesktop)
    //{
    //    params->result = -1;
    //    SetEvent(params->hEvent);
    //    return 1;
    //}

    if (hWinlogonDesktop) 
    {
        if (!SwitchDesktop(hWinlogonDesktop) || !SetThreadDesktop(hWinlogonDesktop))
        {
            CloseDesktop(hWinlogonDesktop);
            params->result = -1;
            SetEvent(params->hEvent);
            return 1;
        }
    }

    // Load the PNG as HBITMAP
    if (!params->backgroundImagePath.empty())
    {
        g_hBitmap = LoadBitmapFromImage(params->backgroundImagePath.c_str(), &g_BitmapWidth, &g_BitmapHeight);
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

    g_BackgroundWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        wc.lpszClassName,
        L"",
        WS_POPUP,
        0, 0, screenW, screenH,
        NULL, NULL, wc.hInstance, NULL
    );

    if (g_BackgroundWnd)
    {
        ShowWindow(g_BackgroundWnd, SW_SHOW);
        UpdateWindow(g_BackgroundWnd);
    }

    // Show MessageBox on top
    params->result = MessageBoxW(
        g_BackgroundWnd,
        params->text.c_str(),
        params->title.c_str(),
        params->uType
    );

    // Cleanup
    if (g_BackgroundWnd)
        DestroyWindow(g_BackgroundWnd);

    if (g_hBitmap)
    {
        DeleteObject(g_hBitmap);
        g_hBitmap = NULL;
    }

    if (hWinlogonDesktop)
    {
        SwitchDesktop(hOriginalDesktop);
        CloseDesktop(hWinlogonDesktop);
    }

    SetEvent(params->hEvent);
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
    params.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

    if (!params.hEvent)
        return -1;

    HANDLE hThread = CreateThread(
        NULL,
        0,
        MessageBoxThreadProc,
        &params,
        0,
        NULL
    );

    if (!hThread)
    {
        CloseHandle(params.hEvent);
        return -1;
    }

    // Wait for the MessageBox to finish
    WaitForSingleObject(params.hEvent, INFINITE);

    CloseHandle(hThread);
    CloseHandle(params.hEvent);

    return params.result;
}
