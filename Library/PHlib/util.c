/*
* Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
*
* This file is part of System Informer.
*
* Authors:
*
*     wj32    2009-2016
*     dmex    2017-2023
*
*/

#include <ph.h>

// from mapldr.c
/**
* Finds and returns the address of a resource.
*
* \param DllBase The base address of the image containing the resource.
* \param Name The name of the resource or the integer identifier.
* \param Type The type of resource or the integer identifier.
* \param ResourceLength A variable which will receive the length of the resource block.
* \param ResourceBuffer A variable which receives the address of the resource data.
*
* \return TRUE if the resource was found, FALSE if an error occurred.
*
* \remarks Use this function instead of LoadResource() because no memory allocation is required.
* This function returns the address of the resource from the read-only section of the image
* and does not need to be allocated or deallocated. This function cannot be used when the
* image will be unloaded since the validity of the address is tied to the lifetime of the image.
*/
_Success_(return)
BOOLEAN PhLoadResource(
    _In_ PVOID DllBase,
    _In_ PCWSTR Name,
    _In_ PCWSTR Type,
    _Out_opt_ ULONG *ResourceLength,
    _Out_opt_ PVOID *ResourceBuffer
)
{
    LDR_RESOURCE_INFO resourceInfo;
    PIMAGE_RESOURCE_DATA_ENTRY resourceData;
    ULONG resourceLength;
    PVOID resourceBuffer;

    resourceInfo.Type = (ULONG_PTR)Type;
    resourceInfo.Name = (ULONG_PTR)Name;
    resourceInfo.Language = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

    if (!NT_SUCCESS(LdrFindResource_U(DllBase, &resourceInfo, RESOURCE_DATA_LEVEL, &resourceData)))
        return FALSE;

    if (!NT_SUCCESS(LdrAccessResource(DllBase, resourceData, &resourceBuffer, &resourceLength)))
        return FALSE;

    if (ResourceLength)
        *ResourceLength = resourceLength;
    if (ResourceBuffer)
        *ResourceBuffer = resourceBuffer;

    return TRUE;
}

// from mapldr.c
/**
* Finds and returns a copy of the resource.
*
* \param DllBase The base address of the image containing the resource.
* \param Name The name of the resource or the integer identifier.
* \param Type The type of resource or the integer identifier.
* \param ResourceLength A variable which will receive the length of the resource block.
* \param ResourceBuffer A variable which receives the address of the resource data.
*
* \return TRUE if the resource was found, FALSE if an error occurred.
*
* \remarks This function returns a copy of a resource from heap memory
* and must be deallocated. Use this function when the image will be unloaded.
*/
_Success_(return)
BOOLEAN PhLoadResourceCopy(
    _In_ PVOID DllBase,
    _In_ PCWSTR Name,
    _In_ PCWSTR Type,
    _Out_opt_ ULONG *ResourceLength,
    _Out_opt_ PVOID *ResourceBuffer
)
{
    ULONG resourceLength;
    PVOID resourceBuffer;

    if (!PhLoadResource(DllBase, Name, Type, &resourceLength, &resourceBuffer))
        return FALSE;

    if (ResourceLength)
        *ResourceLength = resourceLength;
    if (ResourceBuffer)
        *ResourceBuffer = PhAllocateCopy(resourceBuffer, resourceLength);

    return TRUE;
}






/**
* Modifies a string to ensure it is within the specified length.
*
* \param String The input string.
* \param DesiredCount The desired number of characters in the new string. If necessary, parts of
* the string are replaced with an ellipsis to indicate characters have been omitted.
*
* \return The new string.
*/
PPH_STRING PhEllipsisString(
    _In_ PPH_STRING String,
    _In_ SIZE_T DesiredCount
)
{
    if (
        (ULONG)String->Length / sizeof(WCHAR) <= DesiredCount ||
        DesiredCount < 3
        )
    {
        return PhReferenceObject(String);
    }
    else
    {
        PPH_STRING string;

        string = PhCreateStringEx(NULL, DesiredCount * sizeof(WCHAR));
        memcpy(string->Buffer, String->Buffer, (DesiredCount - 3) * sizeof(WCHAR));
        memcpy(&string->Buffer[DesiredCount - 3], L"...", 6);

        return string;
    }
}

/**
* Modifies a string to ensure it is within the specified length, parsing the string as a path.
*
* \param String The input string.
* \param DesiredCount The desired number of characters in the new string. If necessary, parts of
* the string are replaced with an ellipsis to indicate characters have been omitted.
*
* \return The new string.
*/
PPH_STRING PhEllipsisStringPath(
    _In_ PPH_STRING String,
    _In_ SIZE_T DesiredCount
)
{
    ULONG_PTR secondPartIndex;

    secondPartIndex = PhFindLastCharInString(String, 0, OBJ_NAME_PATH_SEPARATOR);

    if (secondPartIndex == SIZE_MAX)
        secondPartIndex = PhFindLastCharInString(String, 0, OBJ_NAME_ALTPATH_SEPARATOR);
    if (secondPartIndex == SIZE_MAX)
        return PhEllipsisString(String, DesiredCount);

    if (
        String->Length / sizeof(WCHAR) <= DesiredCount ||
        DesiredCount < 3
        )
    {
        return PhReferenceObject(String);
    }
    else
    {
        PPH_STRING string;
        ULONG_PTR firstPartCopyLength;
        ULONG_PTR secondPartCopyLength;

        string = PhCreateStringEx(NULL, DesiredCount * sizeof(WCHAR));
        secondPartCopyLength = String->Length / sizeof(WCHAR) - secondPartIndex;

        // Check if we have enough space for the entire second part of the string.
        if (secondPartCopyLength + 3 <= DesiredCount)
        {
            // Yes, copy part of the first part and the entire second part.
            firstPartCopyLength = DesiredCount - secondPartCopyLength - 3;
        }
        else
        {
            // No, copy part of both, from the beginning of the first part and the end of the second
            // part.
            firstPartCopyLength = (DesiredCount - 3) / sizeof(WCHAR);
            secondPartCopyLength = DesiredCount - 3 - firstPartCopyLength;
            secondPartIndex = String->Length / sizeof(WCHAR) - secondPartCopyLength;
        }

        memcpy(
            string->Buffer,
            String->Buffer,
            firstPartCopyLength * sizeof(WCHAR)
        );
        memcpy(
            &string->Buffer[firstPartCopyLength],
            L"...",
            6
        );
        memcpy(
            &string->Buffer[firstPartCopyLength + 3],
            &String->Buffer[secondPartIndex],
            secondPartCopyLength * sizeof(WCHAR)
        );

        return string;
    }
}




/**
* Retrieves image version information for a file.
*
* \param FileName The file name.
*
* \return A version information block. You must free this using PhFree() when you no longer need
* it.
*/
PVOID PhGetFileVersionInfo(
    _In_ PWSTR FileName
)
{
    PVOID libraryModule;
    PVOID versionInfo;

    libraryModule = LoadLibraryEx(
        FileName,
        NULL,
        LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE
    );

    if (!libraryModule)
        return NULL;

    if (PhLoadResourceCopy(
        libraryModule,
        MAKEINTRESOURCE(VS_VERSION_INFO),
        VS_FILE_INFO,
        NULL,
        &versionInfo
    ))
    {
        if (PhIsFileVersionInfo32(versionInfo))
        {
            FreeLibrary(libraryModule);
            return versionInfo;
        }

        PhFree(versionInfo);
    }

    FreeLibrary(libraryModule);
    return NULL;
}

//PVOID PhGetFileVersionInfoEx(
//    _In_ PPH_STRINGREF FileName
//)
//{
//    PVOID imageBaseAddress;
//    PVOID versionInfo;
//
//    if (!NT_SUCCESS(PhLoadLibraryAsImageResource(FileName, TRUE, &imageBaseAddress)))
//        return NULL;
//
//    if (PhLoadResourceCopy(
//        imageBaseAddress,
//        MAKEINTRESOURCE(VS_VERSION_INFO),
//        VS_FILE_INFO,
//        NULL,
//        &versionInfo
//    ))
//    {
//        if (PhIsFileVersionInfo32(versionInfo))
//        {
//            PhFreeLibraryAsImageResource(imageBaseAddress);
//            return versionInfo;
//        }
//
//        PhFree(versionInfo);
//    }
//
//    PhFreeLibraryAsImageResource(imageBaseAddress);
//    return NULL;
//}

PVOID PhGetFileVersionInfoValue(
    _In_ PVS_VERSION_INFO_STRUCT32 VersionInfo
)
{
    PWSTR keyOffset = VersionInfo->Key + PhCountStringZ(VersionInfo->Key) + 1;

    return PTR_ADD_OFFSET(VersionInfo, ALIGN_UP(PTR_SUB_OFFSET(keyOffset, VersionInfo), ULONG));
}

_Success_(return)
BOOLEAN PhGetFileVersionInfoKey(
    _In_ PVS_VERSION_INFO_STRUCT32 VersionInfo,
    _In_ SIZE_T KeyLength,
    _In_ PWSTR Key,
    _Out_opt_ PVOID* Buffer
)
{
    PVOID value;
    ULONG valueOffset;
    PVS_VERSION_INFO_STRUCT32 child;

    if (!(value = PhGetFileVersionInfoValue(VersionInfo)))
        return FALSE;

    valueOffset = VersionInfo->ValueLength * (VersionInfo->Type ? sizeof(WCHAR) : sizeof(BYTE));
    child = PTR_ADD_OFFSET(value, ALIGN_UP(valueOffset, ULONG));

    while ((ULONG_PTR)child < (ULONG_PTR)PTR_ADD_OFFSET(VersionInfo, VersionInfo->Length))
    {
        if (_wcsnicmp(child->Key, Key, KeyLength) == 0 && child->Key[KeyLength] == UNICODE_NULL)
        {
            if (Buffer)
                *Buffer = child;

            return TRUE;
        }

        if (child->Length == 0)
            break;

        child = PTR_ADD_OFFSET(child, ALIGN_UP(child->Length, ULONG));
    }

    return FALSE;
}

_Success_(return)
BOOLEAN PhGetFileVersionVarFileInfoValue(
    _In_ PVOID VersionInfo,
    _In_ PPH_STRINGREF KeyName,
    _Out_opt_ PVOID* Buffer,
    _Out_opt_ PULONG BufferLength
)
{
    static PH_STRINGREF varfileBlockName = PH_STRINGREF_INIT(L"VarFileInfo");
    PVS_VERSION_INFO_STRUCT32 varfileBlockInfo;
    PVS_VERSION_INFO_STRUCT32 varfileBlockValue;

    if (PhGetFileVersionInfoKey(
        VersionInfo,
        varfileBlockName.Length / sizeof(WCHAR),
        varfileBlockName.Buffer,
        &varfileBlockInfo
    ))
    {
        if (PhGetFileVersionInfoKey(
            varfileBlockInfo,
            KeyName->Length / sizeof(WCHAR),
            KeyName->Buffer,
            &varfileBlockValue
        ))
        {
            if (BufferLength)
                *BufferLength = varfileBlockValue->ValueLength;
            if (Buffer)
                *Buffer = PhGetFileVersionInfoValue(varfileBlockValue);
            return TRUE;
        }
    }

    return FALSE;
}

VS_FIXEDFILEINFO* PhGetFileVersionFixedInfo(
    _In_ PVOID VersionInfo
)
{
    VS_FIXEDFILEINFO* fileInfo;

    fileInfo = PhGetFileVersionInfoValue(VersionInfo);

    if (fileInfo && fileInfo->dwSignature == VS_FFI_SIGNATURE)
    {
        return fileInfo;
    }
    else
    {
        return NULL;
    }
}

/**
* Retrieves the language ID and code page used by a version information block.
*
* \param VersionInfo The version information block.
*/
ULONG PhGetFileVersionInfoLangCodePage(
    _In_ PVOID VersionInfo
)
{
    static PH_STRINGREF translationName = PH_STRINGREF_INIT(L"Translation");
    PLANGANDCODEPAGE codePage;
    ULONG codePageLength;

    if (PhGetFileVersionVarFileInfoValue(VersionInfo, &translationName, &codePage, &codePageLength))
    {
        //for (ULONG i = 0; i < (codePageLength / sizeof(LANGANDCODEPAGE)); i++)
        return ((ULONG)codePage[0].Language << 16) + codePage[0].CodePage; // Combine the language ID and code page.
    }

    return (MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US) << 16) + 1252;
}

/**
* Retrieves a string in a version information block.
*
* \param VersionInfo The version information block.
* \param SubBlock The path to the sub-block.
*/
PPH_STRING PhGetFileVersionInfoString(
    _In_ PVOID VersionInfo,
    _In_ PWSTR SubBlock
)
{
    PH_STRINGREF name;
    PWSTR buffer;
    ULONG length;

    PhInitializeStringRefLongHint(&name, SubBlock);

    if (PhGetFileVersionVarFileInfoValue(
        VersionInfo,
        &name,
        &buffer,
        &length
    ))
    {
        PPH_STRING string;

        // Check if the string has a valid length.
        if (length <= sizeof(UNICODE_NULL))
            return NULL;

        string = PhCreateStringEx(buffer, length * sizeof(WCHAR));
        // length may include the null terminator.
        PhTrimToNullTerminatorString(string);

        return string;
    }
    else
    {
        return NULL;
    }
}

/**
* Retrieves a string in a version information block.
*
* \param VersionInfo The version information block.
* \param LangCodePage The language ID and code page of the string.
* \param KeyName The name of the string.
*/
PPH_STRING PhGetFileVersionInfoString2(
    _In_ PVOID VersionInfo,
    _In_ ULONG LangCodePage,
    _In_ PPH_STRINGREF KeyName
)
{
    static PH_STRINGREF blockInfoName = PH_STRINGREF_INIT(L"StringFileInfo");
    PVS_VERSION_INFO_STRUCT32 blockStringInfo;
    PVS_VERSION_INFO_STRUCT32 blockLangInfo;
    PVS_VERSION_INFO_STRUCT32 stringNameBlockInfo;
    PWSTR stringNameBlockValue;
    PPH_STRING string;
    SIZE_T returnLength;
    PH_FORMAT format[3];
    WCHAR langNameString[65];

    if (!PhGetFileVersionInfoKey(
        VersionInfo,
        blockInfoName.Length / sizeof(WCHAR),
        blockInfoName.Buffer,
        &blockStringInfo
    ))
    {
        return NULL;
    }

    PhInitFormatX(&format[0], LangCodePage);
    format[0].Type |= FormatPadZeros | FormatUpperCase;
    format[0].Width = 8;

    if (!PhFormatToBuffer(format, 1, langNameString, sizeof(langNameString), &returnLength))
        return NULL;

    if (!PhGetFileVersionInfoKey(
        blockStringInfo,
        returnLength / sizeof(WCHAR) - sizeof(ANSI_NULL),
        langNameString,
        &blockLangInfo
    ))
    {
        return NULL;
    }

    if (!PhGetFileVersionInfoKey(
        blockLangInfo,
        KeyName->Length / sizeof(WCHAR),
        KeyName->Buffer,
        &stringNameBlockInfo
    ))
    {
        return NULL;
    }

    if (stringNameBlockInfo->ValueLength <= sizeof(UNICODE_NULL)) // Check if the string has a valid length.
        return NULL;
    if (!(stringNameBlockValue = PhGetFileVersionInfoValue(stringNameBlockInfo)))
        return NULL;

    string = PhCreateStringEx(
        stringNameBlockValue,
        UInt32x32To64((stringNameBlockInfo->ValueLength - 1), sizeof(WCHAR))
    );
    //PhTrimToNullTerminatorString(string); // length may include the null terminator.

    return string;
}

PPH_STRING PhGetFileVersionInfoStringEx(
    _In_ PVOID VersionInfo,
    _In_ ULONG LangCodePage,
    _In_ PPH_STRINGREF KeyName
)
{
    PPH_STRING string;

    // Use the default language code page.
    if (string = PhGetFileVersionInfoString2(VersionInfo, LangCodePage, KeyName))
        return string;

    // Use the windows-1252 code page.
    if (string = PhGetFileVersionInfoString2(VersionInfo, (LangCodePage & 0xffff0000) + 1252, KeyName))
        return string;

    // Use the default language (US English).
    if (string = PhGetFileVersionInfoString2(VersionInfo, (MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US) << 16) + 1252, KeyName))
        return string;

    // Use the default language (US English).
    if (string = PhGetFileVersionInfoString2(VersionInfo, (MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US) << 16) + 0, KeyName))
        return string;

    return NULL;
}

VOID PhpGetImageVersionInfoFields(
    _Out_ PPH_IMAGE_VERSION_INFO ImageVersionInfo,
    _In_ PVOID VersionInfo,
    _In_ ULONG LangCodePage
)
{
    static PH_STRINGREF companyName = PH_STRINGREF_INIT(L"CompanyName");
    static PH_STRINGREF fileDescription = PH_STRINGREF_INIT(L"FileDescription");
    static PH_STRINGREF productName = PH_STRINGREF_INIT(L"ProductName");

    ImageVersionInfo->CompanyName = PhGetFileVersionInfoStringEx(VersionInfo, LangCodePage, &companyName);
    ImageVersionInfo->FileDescription = PhGetFileVersionInfoStringEx(VersionInfo, LangCodePage, &fileDescription);
    ImageVersionInfo->ProductName = PhGetFileVersionInfoStringEx(VersionInfo, LangCodePage, &productName);
}

VOID PhpGetImageVersionVersionString(
    _Out_ PPH_IMAGE_VERSION_INFO ImageVersionInfo,
    _In_ PVOID VersionInfo
)
{
    VS_FIXEDFILEINFO* rootBlock;

    // The version information is language-independent and must be read from the root block.
    if (rootBlock = PhGetFileVersionFixedInfo(VersionInfo))
    {
        PH_FORMAT fileVersionFormat[7];

        PhInitFormatU(&fileVersionFormat[0], HIWORD(rootBlock->dwFileVersionMS));
        PhInitFormatC(&fileVersionFormat[1], L'.');
        PhInitFormatU(&fileVersionFormat[2], LOWORD(rootBlock->dwFileVersionMS));
        PhInitFormatC(&fileVersionFormat[3], L'.');
        PhInitFormatU(&fileVersionFormat[4], HIWORD(rootBlock->dwFileVersionLS));
        PhInitFormatC(&fileVersionFormat[5], L'.');
        PhInitFormatU(&fileVersionFormat[6], LOWORD(rootBlock->dwFileVersionLS));

        ImageVersionInfo->FileVersion = PhFormat(fileVersionFormat, RTL_NUMBER_OF(fileVersionFormat), 64);
    }
    else
    {
        ImageVersionInfo->FileVersion = NULL;
    }
}

//VOID PhpGetImageVersionVersionStringEx(
//    _Out_ PPH_IMAGE_VERSION_INFO ImageVersionInfo,
//    _In_ PVOID VersionInfo,
//    _In_ ULONG LangCodePage
//)
//{
//    static PH_STRINGREF fileVersion = PH_STRINGREF_INIT(L"FileVersion");
//    VS_FIXEDFILEINFO* rootBlock;
//    PPH_STRING versionString;
//
//    if (versionString = PhGetFileVersionInfoStringEx(VersionInfo, LangCodePage, &fileVersion))
//    {
//        ImageVersionInfo->FileVersion = versionString;
//        return;
//    }
//
//    if (rootBlock = PhGetFileVersionFixedInfo(VersionInfo))
//    {
//        PH_FORMAT fileVersionFormat[7];
//
//        PhInitFormatU(&fileVersionFormat[0], HIWORD(rootBlock->dwFileVersionMS));
//        PhInitFormatC(&fileVersionFormat[1], L'.');
//        PhInitFormatU(&fileVersionFormat[2], LOWORD(rootBlock->dwFileVersionMS));
//        PhInitFormatC(&fileVersionFormat[3], L'.');
//        PhInitFormatU(&fileVersionFormat[4], HIWORD(rootBlock->dwFileVersionLS));
//        PhInitFormatC(&fileVersionFormat[5], L'.');
//        PhInitFormatU(&fileVersionFormat[6], LOWORD(rootBlock->dwFileVersionLS));
//
//        ImageVersionInfo->FileVersion = PhFormat(fileVersionFormat, RTL_NUMBER_OF(fileVersionFormat), 64);
//    }
//    else
//    {
//        ImageVersionInfo->FileVersion = NULL;
//    }
//}

/**
* Initializes a structure with version information.
*
* \param ImageVersionInfo The version information structure.
* \param FileName The file name of an image.
*/
_Success_(return)
BOOLEAN PhInitializeImageVersionInfo(
    _Out_ PPH_IMAGE_VERSION_INFO ImageVersionInfo,
    _In_ PWSTR FileName
)
{
    PVOID versionInfo;
    ULONG langCodePage;

    versionInfo = PhGetFileVersionInfo(FileName);

    if (!versionInfo)
        return FALSE;

    langCodePage = PhGetFileVersionInfoLangCodePage(versionInfo);
    PhpGetImageVersionVersionString(ImageVersionInfo, versionInfo);
    PhpGetImageVersionInfoFields(ImageVersionInfo, versionInfo, langCodePage);

    PhFree(versionInfo);
    return TRUE;
}

//_Success_(return)
//BOOLEAN PhInitializeImageVersionInfoEx(
//    _Out_ PPH_IMAGE_VERSION_INFO ImageVersionInfo,
//    _In_ PPH_STRINGREF FileName,
//    _In_ BOOLEAN ExtendedVersionInfo
//)
//{
//    PVOID versionInfo;
//    ULONG langCodePage;
//
//    versionInfo = PhGetFileVersionInfoEx(FileName);
//
//    if (!versionInfo)
//        return FALSE;
//
//    langCodePage = PhGetFileVersionInfoLangCodePage(versionInfo);
//    if (ExtendedVersionInfo)
//        PhpGetImageVersionVersionStringEx(ImageVersionInfo, versionInfo, langCodePage);
//    else
//        PhpGetImageVersionVersionString(ImageVersionInfo, versionInfo);
//    PhpGetImageVersionInfoFields(ImageVersionInfo, versionInfo, langCodePage);
//
//    PhFree(versionInfo);
//    return TRUE;
//}

/**
* Frees a version information structure initialized by PhInitializeImageVersionInfo().
*
* \param ImageVersionInfo The version information structure.
*/
VOID PhDeleteImageVersionInfo(
    _Inout_ PPH_IMAGE_VERSION_INFO ImageVersionInfo
)
{
    if (ImageVersionInfo->CompanyName) PhDereferenceObject(ImageVersionInfo->CompanyName);
    if (ImageVersionInfo->FileDescription) PhDereferenceObject(ImageVersionInfo->FileDescription);
    if (ImageVersionInfo->FileVersion) PhDereferenceObject(ImageVersionInfo->FileVersion);
    if (ImageVersionInfo->ProductName) PhDereferenceObject(ImageVersionInfo->ProductName);
}

PPH_STRING PhFormatImageVersionInfo(
    _In_opt_ PPH_STRING FileName,
    _In_ PPH_IMAGE_VERSION_INFO ImageVersionInfo,
    _In_opt_ PPH_STRINGREF Indent,
    _In_opt_ ULONG LineLimit
)
{
    PH_STRING_BUILDER stringBuilder;

    if (LineLimit == 0)
        LineLimit = ULONG_MAX;

    PhInitializeStringBuilder(&stringBuilder, 40);

    // File name

    if (!PhIsNullOrEmptyString(FileName))
    {
        PPH_STRING temp;

        if (Indent) PhAppendStringBuilder(&stringBuilder, Indent);

        temp = PhEllipsisStringPath(FileName, LineLimit);
        PhAppendStringBuilder(&stringBuilder, &temp->sr);
        PhDereferenceObject(temp);
        PhAppendCharStringBuilder(&stringBuilder, L'\n');
    }

    // File description & version

    if (!(
        PhIsNullOrEmptyString(ImageVersionInfo->FileDescription) &&
        PhIsNullOrEmptyString(ImageVersionInfo->FileVersion)
        ))
    {
        PPH_STRING tempDescription = NULL;
        PPH_STRING tempVersion = NULL;
        ULONG limitForDescription;
        ULONG limitForVersion;

        if (LineLimit != ULONG_MAX)
        {
            limitForVersion = (LineLimit - 1) / 4; // 1/4 space for version (and space character)
            limitForDescription = LineLimit - limitForVersion;
        }
        else
        {
            limitForDescription = ULONG_MAX;
            limitForVersion = ULONG_MAX;
        }

        if (!PhIsNullOrEmptyString(ImageVersionInfo->FileDescription))
        {
            tempDescription = PhEllipsisString(
                ImageVersionInfo->FileDescription,
                limitForDescription
            );
        }

        if (!PhIsNullOrEmptyString(ImageVersionInfo->FileVersion))
        {
            tempVersion = PhEllipsisString(
                ImageVersionInfo->FileVersion,
                limitForVersion
            );
        }

        if (Indent) PhAppendStringBuilder(&stringBuilder, Indent);

        if (tempDescription)
        {
            PhAppendStringBuilder(&stringBuilder, &tempDescription->sr);

            if (tempVersion)
                PhAppendCharStringBuilder(&stringBuilder, L' ');
        }

        if (tempVersion)
            PhAppendStringBuilder(&stringBuilder, &tempVersion->sr);

        if (tempDescription)
            PhDereferenceObject(tempDescription);
        if (tempVersion)
            PhDereferenceObject(tempVersion);

        PhAppendCharStringBuilder(&stringBuilder, L'\n');
    }

    // File company

    if (!PhIsNullOrEmptyString(ImageVersionInfo->CompanyName))
    {
        PPH_STRING temp;

        if (Indent) PhAppendStringBuilder(&stringBuilder, Indent);

        temp = PhEllipsisString(ImageVersionInfo->CompanyName, LineLimit);
        PhAppendStringBuilder(&stringBuilder, &temp->sr);
        PhDereferenceObject(temp);
        PhAppendCharStringBuilder(&stringBuilder, L'\n');
    }

    // Remove the extra newline.
    if (stringBuilder.String->Length != 0)
        PhRemoveEndStringBuilder(&stringBuilder, 1);

    return PhFinalStringBuilderString(&stringBuilder);
}

typedef struct _PH_FILE_VERSIONINFO_CACHE_ENTRY
{
    PPH_STRING FileName;
    PPH_STRING CompanyName;
    PPH_STRING FileDescription;
    PPH_STRING FileVersion;
    PPH_STRING ProductName;
} PH_FILE_VERSIONINFO_CACHE_ENTRY, *PPH_FILE_VERSIONINFO_CACHE_ENTRY;

static PPH_HASHTABLE PhpImageVersionInfoCacheHashtable = NULL;
static PH_QUEUED_LOCK PhpImageVersionInfoCacheLock = PH_QUEUED_LOCK_INIT;

static BOOLEAN PhpImageVersionInfoCacheHashtableEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
)
{
    PPH_FILE_VERSIONINFO_CACHE_ENTRY entry1 = Entry1;
    PPH_FILE_VERSIONINFO_CACHE_ENTRY entry2 = Entry2;

    return PhEqualString(entry1->FileName, entry2->FileName, FALSE);
}

static ULONG PhpImageVersionInfoCacheHashtableHashFunction(
    _In_ PVOID Entry
)
{
    PPH_FILE_VERSIONINFO_CACHE_ENTRY entry = Entry;

    return PhHashStringRefEx(&entry->FileName->sr, FALSE, PH_STRING_HASH_X65599);
}

_Success_(return)
BOOLEAN PhInitializeImageVersionInfoCached(
    _Out_ PPH_IMAGE_VERSION_INFO ImageVersionInfo,
    _In_ PPH_STRING FileName,
    _In_ BOOLEAN IsSubsystemProcess,
    _In_ BOOLEAN ExtendedVersion
)
{
    PH_IMAGE_VERSION_INFO versionInfo = { 0 };
    PH_FILE_VERSIONINFO_CACHE_ENTRY newEntry;

    if (PhpImageVersionInfoCacheHashtable)
    {
        PPH_FILE_VERSIONINFO_CACHE_ENTRY entry;
        PH_FILE_VERSIONINFO_CACHE_ENTRY lookupEntry;

        lookupEntry.FileName = FileName;

        PhAcquireQueuedLockShared(&PhpImageVersionInfoCacheLock);
        entry = PhFindEntryHashtable(PhpImageVersionInfoCacheHashtable, &lookupEntry);
        PhReleaseQueuedLockShared(&PhpImageVersionInfoCacheLock);

        if (entry)
        {
            PhSetReference(&ImageVersionInfo->CompanyName, entry->CompanyName);
            PhSetReference(&ImageVersionInfo->FileDescription, entry->FileDescription);
            PhSetReference(&ImageVersionInfo->FileVersion, entry->FileVersion);
            PhSetReference(&ImageVersionInfo->ProductName, entry->ProductName);

            return TRUE;
        }
    }

    PhInitializeImageVersionInfo(&versionInfo, FileName->sr.Buffer); // KISS - Keep it super simple

    //if (IsSubsystemProcess)
    //{
    //    if (!PhInitializeLxssImageVersionInfo(&versionInfo, &FileName->sr))
    //    {
    //        // Note: If the function fails the version info is empty. For extra performance
    //        // we still update the cache with a reference to the filename (without version info)
    //        // and just return the empty result from the cache instead of loading the same file again.
    //        // We flush every few hours so the cache doesn't waste memory or become state. (dmex)
    //    }
    //}
    //else
    //{
    //    if (!PhInitializeImageVersionInfoEx(&versionInfo, &FileName->sr, ExtendedVersion))
    //    {
    //        // Note: If the function fails the version info is empty. For extra performance
    //        // we still update the cache with a reference to the filename (without version info)
    //        // and just return the empty result from the cache instead of loading the same file again.
    //        // We flush every few hours so the cache doesn't waste memory or become state. (dmex)
    //    }
    //}

    if (!PhpImageVersionInfoCacheHashtable)
    {
        PhpImageVersionInfoCacheHashtable = PhCreateHashtable(
            sizeof(PH_FILE_VERSIONINFO_CACHE_ENTRY),
            PhpImageVersionInfoCacheHashtableEqualFunction,
            PhpImageVersionInfoCacheHashtableHashFunction,
            100
        );
    }

    PhSetReference(&newEntry.FileName, FileName);
    PhSetReference(&newEntry.CompanyName, versionInfo.CompanyName);
    PhSetReference(&newEntry.FileDescription, versionInfo.FileDescription);
    PhSetReference(&newEntry.FileVersion, versionInfo.FileVersion);
    PhSetReference(&newEntry.ProductName, versionInfo.ProductName);

    PhAcquireQueuedLockExclusive(&PhpImageVersionInfoCacheLock);
    PhAddEntryHashtable(PhpImageVersionInfoCacheHashtable, &newEntry);
    PhReleaseQueuedLockExclusive(&PhpImageVersionInfoCacheLock);

    ImageVersionInfo->CompanyName = versionInfo.CompanyName;
    ImageVersionInfo->FileDescription = versionInfo.FileDescription;
    ImageVersionInfo->FileVersion = versionInfo.FileVersion;
    ImageVersionInfo->ProductName = versionInfo.ProductName;
    return TRUE;
}

VOID PhFlushImageVersionInfoCache(
    VOID
)
{
    PH_HASHTABLE_ENUM_CONTEXT enumContext;
    PPH_FILE_VERSIONINFO_CACHE_ENTRY entry;

    if (!PhpImageVersionInfoCacheHashtable)
        return;

    PhAcquireQueuedLockExclusive(&PhpImageVersionInfoCacheLock);

    PhBeginEnumHashtable(PhpImageVersionInfoCacheHashtable, &enumContext);

    while (entry = PhNextEnumHashtable(&enumContext))
    {
        if (entry->FileName) PhDereferenceObject(entry->FileName);
        if (entry->CompanyName) PhDereferenceObject(entry->CompanyName);
        if (entry->FileDescription) PhDereferenceObject(entry->FileDescription);
        if (entry->FileVersion) PhDereferenceObject(entry->FileVersion);
        if (entry->ProductName) PhDereferenceObject(entry->ProductName);
    }

    PhClearReference(&PhpImageVersionInfoCacheHashtable);

    PhpImageVersionInfoCacheHashtable = PhCreateHashtable(
        sizeof(PH_FILE_VERSIONINFO_CACHE_ENTRY),
        PhpImageVersionInfoCacheHashtableEqualFunction,
        PhpImageVersionInfoCacheHashtableHashFunction,
        100
    );

    PhReleaseQueuedLockExclusive(&PhpImageVersionInfoCacheLock);
}