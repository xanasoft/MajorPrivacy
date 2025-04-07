#include "pch.h"
#include <winhttp.h>
#include <locale>
#include <algorithm>
#include <string>
#include <map>
#include <Windows.h>

#include "WebUtils.h"

// Conversion helper functions using Windows API
std::string WideToUTF8(const std::wstring &wstr)
{
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size_needed, NULL, NULL);
    if (!result.empty() && result.back() == '\0')
        result.pop_back(); // remove trailing null
    return result;
}

std::wstring UTF8ToWide(const std::string &str)
{
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size_needed);
    if (!result.empty() && result.back() == L'\0')
        result.pop_back(); // remove trailing null
    return result;
}

OSVERSIONINFOW g_osvi = {0};

#pragma warning(disable : 4996)

static void InitOsVersionInfo() 
{
    g_osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
    NTSTATUS(WINAPI * RtlGetVersion)(LPOSVERSIONINFOW);
    *(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (RtlGetVersion == NULL || !NT_SUCCESS(RtlGetVersion(&g_osvi)))
        GetVersionExW(&g_osvi);
}

// Helper function to parse URL using WinHttpCrackUrl
bool ParseUrl(const WCHAR* sUrl, std::wstring* pScheme, std::wstring* pHost, WORD* pPort, std::wstring* pPath, std::wstring* pQuery)
{
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t schemeBuffer[16] = {0};
    wchar_t hostBuffer[256] = {0};
    wchar_t urlPathBuffer[2048] = {0};
    wchar_t extraInfoBuffer[256] = {0};

    urlComp.lpszScheme = schemeBuffer;
    urlComp.dwSchemeLength = _countof(schemeBuffer);
    urlComp.lpszHostName = hostBuffer;
    urlComp.dwHostNameLength = _countof(hostBuffer);
    urlComp.lpszUrlPath = urlPathBuffer;
    urlComp.dwUrlPathLength = _countof(urlPathBuffer);
    urlComp.lpszExtraInfo = extraInfoBuffer;
    urlComp.dwExtraInfoLength = _countof(extraInfoBuffer);

    if (!WinHttpCrackUrl(sUrl, 0, 0, &urlComp))
        return false;

    if(pScheme) pScheme->assign(urlComp.lpszScheme, urlComp.dwSchemeLength);
    if(pHost) pHost->assign(urlComp.lpszHostName, urlComp.dwHostNameLength);
    if(pPort) *pPort = urlComp.nPort;
    if (pPath) pPath->assign(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) {
        if(pQuery)
			pQuery->assign(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
        else if (pPath)
            pPath->append(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }
    return true;
}

void GetWebPayload(PVOID RequestHandle, PSTR* pData, ULONG* pDataLength)
{
    PVOID result = NULL;
    ULONG allocatedLength;
    ULONG dataLength;
    ULONG returnLength;
    BYTE buffer[0x1000];

    if (pData == NULL)
        return;

    allocatedLength = sizeof(buffer);
    *pData = (PSTR)malloc(allocatedLength);
    dataLength = 0;

    while (WinHttpReadData(RequestHandle, buffer, sizeof(buffer), &returnLength))
    {
        if (returnLength == 0)
            break;

        if (allocatedLength < dataLength + returnLength)
        {
            allocatedLength *= 2;
            *pData = (PSTR)realloc(*pData, allocatedLength);
        }

        memcpy(*pData + dataLength, buffer, returnLength);
        dataLength += returnLength;
    }

    if (allocatedLength < dataLength + 1)
    {
        allocatedLength++;
        *pData = (PSTR)realloc(*pData, allocatedLength);
    }
    (*pData)[dataLength] = 0;
    if (pDataLength != NULL)
        *pDataLength = dataLength;
}

BOOLEAN WebDownload(const WCHAR* URL, PSTR* pData, ULONG* pDataLength)
{
    BOOLEAN success = FALSE;
    HINTERNET SessionHandle = NULL;
    HINTERNET ConnectionHandle = NULL;
    HINTERNET RequestHandle = NULL;
    std::wstring scheme, host, path;
    INTERNET_PORT port = 0;
    DWORD httpFlags = 0;

    if (!ParseUrl(URL, &scheme, &host, &port, &path))
        return FALSE;

    if (_wcsicmp(scheme.c_str(), L"https") == 0)
        httpFlags = WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH;
    else
        httpFlags = WINHTTP_FLAG_REFRESH;

    if (g_osvi.dwOSVersionInfoSize == 0)
        InitOsVersionInfo();

    SessionHandle = WinHttpOpen(NULL,
        g_osvi.dwMajorVersion >= 8 ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!SessionHandle)
        goto CleanupExit;

    if (g_osvi.dwMajorVersion >= 8) {
        ULONG Options = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
        WinHttpSetOption(SessionHandle, WINHTTP_OPTION_DECOMPRESSION, &Options, sizeof(Options));
    }

    ConnectionHandle = WinHttpConnect(SessionHandle, host.c_str(), port, 0);
    if (!ConnectionHandle)
        goto CleanupExit;

    RequestHandle = WinHttpOpenRequest(ConnectionHandle,
        L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, httpFlags);
    if (!RequestHandle)
        goto CleanupExit;

    {
        ULONG Options = WINHTTP_DISABLE_KEEP_ALIVE;
        WinHttpSetOption(RequestHandle, WINHTTP_OPTION_DISABLE_FEATURE, &Options, sizeof(Options));
    }

    if (!WinHttpSendRequest(RequestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0))
        goto CleanupExit;

    if (!WinHttpReceiveResponse(RequestHandle, NULL))
        goto CleanupExit;

    GetWebPayload(RequestHandle, pData, pDataLength);
    success = TRUE;

CleanupExit:
    if (RequestHandle)
        WinHttpCloseHandle(RequestHandle);
    if (ConnectionHandle)
        WinHttpCloseHandle(ConnectionHandle);
    if (SessionHandle)
        WinHttpCloseHandle(SessionHandle);
    return success;
}

BOOLEAN WebUpload(const WCHAR* URL, const WCHAR * FileName, PSTR* pFileData, ULONG FileLength, 
    PSTR* pData, ULONG* pDataLength, std::map<std::string, std::string> params)
{
    BOOLEAN success = FALSE;
    HINTERNET SessionHandle = NULL;
    HINTERNET ConnectionHandle = NULL;
    HINTERNET RequestHandle = NULL;
    DWORD httpFlags = 0;
    std::wstring scheme, host, path;
    INTERNET_PORT port = 0;

    if (!ParseUrl(URL, &scheme, &host, &port, &path))
        return FALSE;

    if (_wcsicmp(scheme.c_str(), L"https") == 0)
        httpFlags = WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH;
    else
        httpFlags = WINHTTP_FLAG_REFRESH;

    if (g_osvi.dwOSVersionInfoSize == 0)
        InitOsVersionInfo();

    SessionHandle = WinHttpOpen(NULL,
        g_osvi.dwMajorVersion >= 8 ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!SessionHandle)
        goto CleanupExit;

    if (g_osvi.dwMajorVersion >= 8) {
        ULONG Options = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
        WinHttpSetOption(SessionHandle, WINHTTP_OPTION_DECOMPRESSION, &Options, sizeof(Options));
    }

    ConnectionHandle = WinHttpConnect(SessionHandle, host.c_str(), port, 0);
    if (!ConnectionHandle)
        goto CleanupExit;

    RequestHandle = WinHttpOpenRequest(ConnectionHandle,
        L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, httpFlags);
    if (!RequestHandle)
        goto CleanupExit;

    {
        ULONG Options = WINHTTP_DISABLE_KEEP_ALIVE;
        WinHttpSetOption(RequestHandle, WINHTTP_OPTION_DISABLE_FEATURE, &Options, sizeof(Options));
    }

    {
        std::string Boundary = "-------------------------" + std::to_string(GetTickCount64()) + "-" + std::to_string(rand());
        // Convert the boundary string from UTF-8 to wide string.
        std::wstring wBoundary = UTF8ToWide(Boundary);
        std::wstring wContentHeader = L"Content-Type: multipart/form-data; boundary=" + wBoundary;

        WinHttpAddRequestHeaders(RequestHandle, wContentHeader.c_str(), (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
        WinHttpAddRequestHeaders(RequestHandle, L"Connection: close", (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

        std::string Payload;

        for (auto I = params.begin(); I != params.end(); ++I)
        {
            Payload.append("--" + Boundary + "\r\nContent-Disposition: form-data; name=\"" + I->first + "\"\r\n\r\n");
            Payload.append(I->second);
            Payload.append("\r\n");
        }

        Payload.append("--" + Boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + WideToUTF8(FileName) + "\"\r\n\r\n");
        Payload.append((char*)pFileData, FileLength);
        Payload.append("\r\n--" + Boundary + "--");

        std::wstring wContentLength = L"Content-Length: " + std::to_wstring((DWORD)Payload.size());

        if(!WinHttpSendRequest(RequestHandle, wContentLength.c_str(), -1, (LPVOID)Payload.c_str(), (DWORD)Payload.size(), (DWORD)Payload.size(), 0))
            goto CleanupExit;
    }

    if (!WinHttpReceiveResponse(RequestHandle, NULL))
        goto CleanupExit;

    GetWebPayload(RequestHandle, pData, pDataLength);
    success = TRUE;

CleanupExit:
    if (RequestHandle)
        WinHttpCloseHandle(RequestHandle);
    if (ConnectionHandle)
        WinHttpCloseHandle(ConnectionHandle);
    if (SessionHandle)
        WinHttpCloseHandle(SessionHandle);
    return success;
}

#include <time.h>  // for _mkgmtime

BOOLEAN WebGetLastModifiedDate(const WCHAR* URL, time_t &unixTimestamp)
{
    BOOLEAN success = FALSE;
    HINTERNET SessionHandle = NULL;
    HINTERNET ConnectionHandle = NULL;
    HINTERNET RequestHandle = NULL;
    DWORD httpFlags = 0;
    wchar_t headerBuffer[256] = {0};
    DWORD headerBufferSize = sizeof(headerBuffer);
    DWORD origBufferSize = headerBufferSize;
    BOOL headerFound;

    std::wstring scheme, host, path;
    INTERNET_PORT port = 0;
    if (!ParseUrl(URL, &scheme, &host, &port, &path))
        return FALSE;

    if (_wcsicmp(scheme.c_str(), L"https") == 0)
        httpFlags = WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH;
    else
        httpFlags = WINHTTP_FLAG_REFRESH;

    if (g_osvi.dwOSVersionInfoSize == 0)
        InitOsVersionInfo();

    SessionHandle = WinHttpOpen(NULL,
        g_osvi.dwMajorVersion >= 8 ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!SessionHandle)
        goto CleanupExit;

    if (g_osvi.dwMajorVersion >= 8) {
        ULONG Options = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
        WinHttpSetOption(SessionHandle, WINHTTP_OPTION_DECOMPRESSION, &Options, sizeof(Options));
    }

    ConnectionHandle = WinHttpConnect(SessionHandle, host.c_str(), port, 0);
    if (!ConnectionHandle)
        goto CleanupExit;

    RequestHandle = WinHttpOpenRequest(ConnectionHandle, L"HEAD", path.c_str(), NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, httpFlags);
    if (!RequestHandle)
        goto CleanupExit;

    {
        ULONG Options = WINHTTP_DISABLE_KEEP_ALIVE;
        WinHttpSetOption(RequestHandle, WINHTTP_OPTION_DISABLE_FEATURE, &Options, sizeof(Options));
    }

    if (!WinHttpSendRequest(RequestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0))
        goto CleanupExit;

    if (!WinHttpReceiveResponse(RequestHandle, NULL))
        goto CleanupExit;

    // Try to query for the Last-Modified header first.
    headerFound = WinHttpQueryHeaders(RequestHandle, WINHTTP_QUERY_LAST_MODIFIED, 
        WINHTTP_HEADER_NAME_BY_INDEX, headerBuffer, 
        &headerBufferSize, WINHTTP_NO_HEADER_INDEX);
    // If Last-Modified not found, try the Date header.
    if (!headerFound)
    {
        headerBufferSize = origBufferSize;
        headerFound = WinHttpQueryHeaders(RequestHandle, WINHTTP_QUERY_DATE,
            WINHTTP_HEADER_NAME_BY_INDEX, headerBuffer,
            &headerBufferSize, WINHTTP_NO_HEADER_INDEX);
    }

    if (headerFound)
    {
        // Expected header format: "Tue, 11 Mar 2025 09:35:28 GMT"
        int day = 0, year = 0, hour = 0, minute = 0, second = 0;
        wchar_t monthStr[4] = {0};

        if (swscanf(headerBuffer, L"%*3s, %d %3s %d %d:%d:%d GMT",
            &day, monthStr, &year, &hour, &minute, &second) == 6)
        {
            int month = 0;
            if (_wcsicmp(monthStr, L"Jan") == 0) month = 1;
            else if (_wcsicmp(monthStr, L"Feb") == 0) month = 2;
            else if (_wcsicmp(monthStr, L"Mar") == 0) month = 3;
            else if (_wcsicmp(monthStr, L"Apr") == 0) month = 4;
            else if (_wcsicmp(monthStr, L"May") == 0) month = 5;
            else if (_wcsicmp(monthStr, L"Jun") == 0) month = 6;
            else if (_wcsicmp(monthStr, L"Jul") == 0) month = 7;
            else if (_wcsicmp(monthStr, L"Aug") == 0) month = 8;
            else if (_wcsicmp(monthStr, L"Sep") == 0) month = 9;
            else if (_wcsicmp(monthStr, L"Oct") == 0) month = 10;
            else if (_wcsicmp(monthStr, L"Nov") == 0) month = 11;
            else if (_wcsicmp(monthStr, L"Dec") == 0) month = 12;

            if (month != 0)
            {
                struct tm t = {0};
                t.tm_year = year - 1900;
                t.tm_mon  = month - 1;
                t.tm_mday = day;
                t.tm_hour = hour;
                t.tm_min  = minute;
                t.tm_sec  = second;
                t.tm_isdst = 0; // GMT does not use DST
                unixTimestamp = _mkgmtime(&t);
                success = TRUE;
            }
        }
    }
    else
    {
        unixTimestamp = 0;
    }

CleanupExit:
    if (RequestHandle)
        WinHttpCloseHandle(RequestHandle);
    if (ConnectionHandle)
        WinHttpCloseHandle(ConnectionHandle);
    if (SessionHandle)
        WinHttpCloseHandle(SessionHandle);
    return success;
}