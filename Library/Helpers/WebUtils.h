#pragma once
#include "../lib_global.h"

bool LIBRARY_EXPORT ParseUrl(const WCHAR* sUrl, std::wstring* pScheme, std::wstring* pHost, WORD* pPort, std::wstring* pPath, std::wstring* pQuery = NULL);

BOOLEAN LIBRARY_EXPORT WebDownload(const WCHAR* URL, PSTR* pData, ULONG* pDataLength);

BOOLEAN LIBRARY_EXPORT WebUpload(const WCHAR* URL, const WCHAR * FileName, PSTR* pFileData, ULONG FileLength, 
    PSTR* pData, ULONG* pDataLength, std::map<std::string, std::string> params);

BOOLEAN LIBRARY_EXPORT WebGetLastModifiedDate(const WCHAR* URL, time_t &unixTimestamp);