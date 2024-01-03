/*
 * Side-by-side assembly support definitions.
 *
 * This file is part of System Informer.
 */

#ifndef _NTSXS_H
#define _NTSXS_H
#include "pshpack4.h"

#define ACTIVATION_CONTEXT_DATA_MAGIC ('xtcA')
#define ACTIVATION_CONTEXT_DATA_FORMAT_WHISTLER 1

#define ACTIVATION_CONTEXT_FLAG_NO_INHERIT 0x00000001

#if (PHNT_MODE == PHNT_MODE_KERNEL)
typedef enum
{
    ACTCTX_RUN_LEVEL_UNSPECIFIED = 0,
    ACTCTX_RUN_LEVEL_AS_INVOKER,
    ACTCTX_RUN_LEVEL_HIGHEST_AVAILABLE,
    ACTCTX_RUN_LEVEL_REQUIRE_ADMIN,
    ACTCTX_RUN_LEVEL_NUMBERS
} ACTCTX_REQUESTED_RUN_LEVEL;

typedef enum
{
    ACTCTX_COMPATIBILITY_ELEMENT_TYPE_UNKNOWN = 0,
    ACTCTX_COMPATIBILITY_ELEMENT_TYPE_OS,
    ACTCTX_COMPATIBILITY_ELEMENT_TYPE_MITIGATION,
    ACTCTX_COMPATIBILITY_ELEMENT_TYPE_MAXVERSIONTESTED
} ACTCTX_COMPATIBILITY_ELEMENT_TYPE;
#endif

typedef struct _ACTIVATION_CONTEXT_DATA
{
    ULONG Magic;
    ULONG HeaderSize;
    ULONG FormatVersion;
    ULONG TotalSize;
    ULONG DefaultTocOffset; // to ACTIVATION_CONTEXT_DATA_TOC_HEADER
    ULONG ExtendedTocOffset; // to ACTIVATION_CONTEXT_DATA_EXTENDED_TOC_HEADER
    ULONG AssemblyRosterOffset; // to ACTIVATION_CONTEXT_DATA_ASSEMBLY_ROSTER_HEADER
    ULONG Flags; // ACTIVATION_CONTEXT_FLAG_*
} ACTIVATION_CONTEXT_DATA, *PACTIVATION_CONTEXT_DATA;

#define ACTIVATION_CONTEXT_DATA_TOC_HEADER_DENSE 0x00000001
#define ACTIVATION_CONTEXT_DATA_TOC_HEADER_INORDER 0x00000002

typedef struct _ACTIVATION_CONTEXT_DATA_TOC_HEADER
{
    ULONG HeaderSize;
    ULONG EntryCount;
    ULONG FirstEntryOffset; // to ACTIVATION_CONTEXT_DATA_TOC_ENTRY[], from ACTIVATION_CONTEXT_DATA base
    ULONG Flags; // ACTIVATION_CONTEXT_DATA_TOC_HEADER_*
} ACTIVATION_CONTEXT_DATA_TOC_HEADER, *PACTIVATION_CONTEXT_DATA_TOC_HEADER;

typedef struct _ACTIVATION_CONTEXT_DATA_TOC_ENTRY
{
    ULONG Id; // ACTIVATION_CONTEXT_SECTION_*
    ULONG Offset; // to ACTIVATION_CONTEXT_*_SECTION_HEADER, from ACTIVATION_CONTEXT_DATA base
    ULONG Length;
    ULONG Format; // ACTIVATION_CONTEXT_SECTION_FORMAT_*
} ACTIVATION_CONTEXT_DATA_TOC_ENTRY, *PACTIVATION_CONTEXT_DATA_TOC_ENTRY;

typedef struct _ACTIVATION_CONTEXT_DATA_EXTENDED_TOC_HEADER
{
    ULONG HeaderSize;
    ULONG EntryCount;
    ULONG FirstEntryOffset; // to ACTIVATION_CONTEXT_DATA_EXTENDED_TOC_ENTRY[], from ACTIVATION_CONTEXT_DATA base
    ULONG Flags;
} ACTIVATION_CONTEXT_DATA_EXTENDED_TOC_HEADER, *PACTIVATION_CONTEXT_DATA_EXTENDED_TOC_HEADER;

typedef struct _ACTIVATION_CONTEXT_DATA_EXTENDED_TOC_ENTRY
{
    GUID ExtensionGuid;
    ULONG TocOffset; // to ACTIVATION_CONTEXT_DATA_TOC_HEADER, from ACTIVATION_CONTEXT_DATA base
    ULONG Length;
} ACTIVATION_CONTEXT_DATA_EXTENDED_TOC_ENTRY, *PACTIVATION_CONTEXT_DATA_EXTENDED_TOC_ENTRY;

#define ACTIVATION_CONTEXT_DATA_ASSEMBLY_ROSTER_ENTRY_INVALID 0x00000001
#define ACTIVATION_CONTEXT_DATA_ASSEMBLY_ROSTER_ENTRY_ROOT 0x00000002

typedef struct _ACTIVATION_CONTEXT_DATA_ASSEMBLY_ROSTER_HEADER
{
    ULONG HeaderSize;
    ULONG HashAlgorithm; // HASH_STRING_ALGORITHM_*
    ULONG EntryCount;
    ULONG FirstEntryOffset; // to ACTIVATION_CONTEXT_DATA_ASSEMBLY_ROSTER_ENTRY[], from ACTIVATION_CONTEXT_DATA base
    ULONG AssemblyInformationSectionOffset; // to resolve section-relative offsets
} ACTIVATION_CONTEXT_DATA_ASSEMBLY_ROSTER_HEADER, *PACTIVATION_CONTEXT_DATA_ASSEMBLY_ROSTER_HEADER;

typedef struct _ACTIVATION_CONTEXT_DATA_ASSEMBLY_ROSTER_ENTRY
{
    ULONG Flags;
    ULONG PseudoKey;
    ULONG AssemblyNameOffset; // to WCHAR[], from ACTIVATION_CONTEXT_DATA base
    ULONG AssemblyNameLength;
    ULONG AssemblyInformationOffset; // to ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION, from ACTIVATION_CONTEXT_DATA base
    ULONG AssemblyInformationLength;
} ACTIVATION_CONTEXT_DATA_ASSEMBLY_ROSTER_ENTRY, *PACTIVATION_CONTEXT_DATA_ASSEMBLY_ROSTER_ENTRY;

#define ACTIVATION_CONTEXT_SECTION_FORMAT_UNKNOWN 0
#define ACTIVATION_CONTEXT_SECTION_FORMAT_STRING_TABLE 1 // ACTIVATION_CONTEXT_STRING_SECTION_HEADER
#define ACTIVATION_CONTEXT_SECTION_FORMAT_GUID_TABLE 2 // ACTIVATION_CONTEXT_GUID_SECTION_HEADER

#define ACTIVATION_CONTEXT_STRING_SECTION_MAGIC ('dHsS')
#define ACTIVATION_CONTEXT_STRING_SECTION_FORMAT_WHISTLER 1

#define ACTIVATION_CONTEXT_STRING_SECTION_CASE_INSENSITIVE 0x00000001
#define ACTIVATION_CONTEXT_STRING_SECTION_ENTRIES_IN_PSEUDOKEY_ORDER 0x00000002

typedef struct _ACTIVATION_CONTEXT_STRING_SECTION_HEADER
{
    ULONG Magic;
    ULONG HeaderSize;
    ULONG FormatVersion;
    ULONG DataFormatVersion;
    ULONG Flags; // ACTIVATION_CONTEXT_STRING_SECTION_*
    ULONG ElementCount;
    ULONG ElementListOffset; // to ACTIVATION_CONTEXT_STRING_SECTION_ENTRY[], from this struct base
    ULONG HashAlgorithm; // HASH_STRING_ALGORITHM_*
    ULONG SearchStructureOffset; // to ACTIVATION_CONTEXT_STRING_SECTION_HASH_TABLE, from this struct base
    ULONG UserDataOffset; // to data depending on section Id, from this struct base
    ULONG UserDataSize;
} ACTIVATION_CONTEXT_STRING_SECTION_HEADER, *PACTIVATION_CONTEXT_STRING_SECTION_HEADER;

typedef struct _ACTIVATION_CONTEXT_STRING_SECTION_ENTRY
{
    ULONG PseudoKey;
    ULONG KeyOffset; // to WCHAR[], from section header
    ULONG KeyLength;
    ULONG Offset; // to data depending on section Id, from section header
    ULONG Length;
    ULONG AssemblyRosterIndex;
} ACTIVATION_CONTEXT_STRING_SECTION_ENTRY, *PACTIVATION_CONTEXT_STRING_SECTION_ENTRY;

typedef struct _ACTIVATION_CONTEXT_STRING_SECTION_HASH_TABLE
{
    ULONG BucketTableEntryCount;
    ULONG BucketTableOffset; // to ACTIVATION_CONTEXT_STRING_SECTION_HASH_BUCKET[], from section header
} ACTIVATION_CONTEXT_STRING_SECTION_HASH_TABLE, *PACTIVATION_CONTEXT_STRING_SECTION_HASH_TABLE;

typedef struct _ACTIVATION_CONTEXT_STRING_SECTION_HASH_BUCKET
{
    ULONG ChainCount;
    ULONG ChainOffset; // to LONG[], from section header
} ACTIVATION_CONTEXT_STRING_SECTION_HASH_BUCKET, *PACTIVATION_CONTEXT_STRING_SECTION_HASH_BUCKET;

#define ACTIVATION_CONTEXT_GUID_SECTION_MAGIC ('dHsG')
#define ACTIVATION_CONTEXT_GUID_SECTION_FORMAT_WHISTLER 1

#define ACTIVATION_CONTEXT_GUID_SECTION_ENTRIES_IN_ORDER 0x00000001

typedef struct _ACTIVATION_CONTEXT_GUID_SECTION_HEADER
{
    ULONG Magic;
    ULONG HeaderSize;
    ULONG FormatVersion;
    ULONG DataFormatVersion;
    ULONG Flags; // ACTIVATION_CONTEXT_GUID_SECTION_*
    ULONG ElementCount;
    ULONG ElementListOffset; // to ACTIVATION_CONTEXT_GUID_SECTION_ENTRY[], from this struct base
    ULONG SearchStructureOffset; // to ACTIVATION_CONTEXT_GUID_SECTION_HASH_TABLE, from this struct base
    ULONG UserDataOffset; // to data depending on section Id, from this struct base
    ULONG UserDataSize;
} ACTIVATION_CONTEXT_GUID_SECTION_HEADER, *PACTIVATION_CONTEXT_GUID_SECTION_HEADER;

typedef struct _ACTIVATION_CONTEXT_GUID_SECTION_ENTRY
{
    GUID Guid;
    ULONG Offset; // to data depending on section Id, from section header
    ULONG Length;
    ULONG AssemblyRosterIndex;
} ACTIVATION_CONTEXT_GUID_SECTION_ENTRY, *PACTIVATION_CONTEXT_GUID_SECTION_ENTRY;

typedef struct _ACTIVATION_CONTEXT_GUID_SECTION_HASH_TABLE
{
    ULONG BucketTableEntryCount;
    ULONG BucketTableOffset; // to ACTIVATION_CONTEXT_GUID_SECTION_HASH_BUCKET, from section header
} ACTIVATION_CONTEXT_GUID_SECTION_HASH_TABLE, *PACTIVATION_CONTEXT_GUID_SECTION_HASH_TABLE;

typedef struct _ACTIVATION_CONTEXT_GUID_SECTION_HASH_BUCKET
{
    ULONG ChainCount;
    ULONG ChainOffset; // to LONG[], from section header
} ACTIVATION_CONTEXT_GUID_SECTION_HASH_BUCKET, *PACTIVATION_CONTEXT_GUID_SECTION_HASH_BUCKET;

// winnt.h - known section IDs
// #define ACTIVATION_CONTEXT_SECTION_ASSEMBLY_INFORMATION         (1) // ACTIVATION_CONTEXT_SECTION_ASSEMBLY_INFORMATION + ACTIVATION_CONTEXT_DATA_ASSEMBLY_GLOBAL_INFORMATION
// #define ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION              (2) // ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION
// #define ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION     (3) // ACTIVATION_CONTEXT_DATA_WINDOW_CLASS_REDIRECTION
// #define ACTIVATION_CONTEXT_SECTION_COM_SERVER_REDIRECTION       (4) // ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION
// #define ACTIVATION_CONTEXT_SECTION_COM_INTERFACE_REDIRECTION    (5) // ACTIVATION_CONTEXT_DATA_COM_INTERFACE_REDIRECTION
// #define ACTIVATION_CONTEXT_SECTION_COM_TYPE_LIBRARY_REDIRECTION (6) // ACTIVATION_CONTEXT_DATA_COM_TYPE_LIBRARY_REDIRECTION
// #define ACTIVATION_CONTEXT_SECTION_COM_PROGID_REDIRECTION       (7) // ACTIVATION_CONTEXT_DATA_COM_PROGID_REDIRECTION
// #define ACTIVATION_CONTEXT_SECTION_GLOBAL_OBJECT_RENAME_TABLE   (8)
// #define ACTIVATION_CONTEXT_SECTION_CLR_SURROGATES               (9) // ACTIVATION_CONTEXT_DATA_CLR_SURROGATE
// #define ACTIVATION_CONTEXT_SECTION_APPLICATION_SETTINGS         (10) // ACTIVATION_CONTEXT_DATA_APPLICATION_SETTINGS
// #define ACTIVATION_CONTEXT_SECTION_COMPATIBILITY_INFO           (11) // ACTIVATION_CONTEXT_COMPATIBILITY_INFORMATION[_LEGACY]
// #define ACTIVATION_CONTEXT_SECTION_WINRT_ACTIVATABLE_CLASSES    (12) // since 19H1

#define ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION_FORMAT_WHISTLER 1

#define ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION_ROOT_ASSEMBLY 0x00000001
#define ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION_POLICY_APPLIED 0x00000002
#define ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION_ASSEMBLY_POLICY_APPLIED 0x00000004
#define ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION_ROOT_POLICY_APPLIED 0x00000008
#define ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION_PRIVATE_ASSEMBLY 0x00000010

typedef struct _ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION
{
    ULONG Size;
    ULONG Flags; // ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION_*
    ULONG EncodedAssemblyIdentityLength;
    ULONG EncodedAssemblyIdentityOffset; // to WCHAR[], from section header
    ULONG ManifestPathType; // ACTIVATION_CONTEXT_PATH_TYPE_*
    ULONG ManifestPathLength;
    ULONG ManifestPathOffset; // to WCHAR[], from section header
    LARGE_INTEGER ManifestLastWriteTime;
    ULONG PolicyPathType; // ACTIVATION_CONTEXT_PATH_TYPE_*
    ULONG PolicyPathLength;
    ULONG PolicyPathOffset; // to WCHAR[], from section header
    LARGE_INTEGER PolicyLastWriteTime;
    ULONG MetadataSatelliteRosterIndex;
    ULONG Unused2;
    ULONG ManifestVersionMajor;
    ULONG ManifestVersionMinor;
    ULONG PolicyVersionMajor;
    ULONG PolicyVersionMinor;
    ULONG AssemblyDirectoryNameLength;
    ULONG AssemblyDirectoryNameOffset; // to WCHAR[], from section header
    ULONG NumOfFilesInAssembly;
    ULONG LanguageLength;
    ULONG LanguageOffset; // to WCHAR[], from section header
    ACTCTX_REQUESTED_RUN_LEVEL RunLevel;
    ULONG UiAccess;
} ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION, *PACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION;

// via UserData
typedef struct _ACTIVATION_CONTEXT_DATA_ASSEMBLY_GLOBAL_INFORMATION
{
    ULONG Size;
    ULONG Flags;
    GUID PolicyCoherencyGuid;
    GUID PolicyOverrideGuid;
    ULONG ApplicationDirectoryPathType; // ACTIVATION_CONTEXT_PATH_TYPE_*
    ULONG ApplicationDirectoryLength;
    ULONG ApplicationDirectoryOffset; // to WCHAR[], from this struct base
    ULONG ResourceName;
} ACTIVATION_CONTEXT_DATA_ASSEMBLY_GLOBAL_INFORMATION, *PACTIVATION_CONTEXT_DATA_ASSEMBLY_GLOBAL_INFORMATION;

#define ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION_FORMAT_WHISTLER 1

#define ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION_PATH_INCLUDES_BASE_NAME 0x00000001
#define ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION_PATH_OMITS_ASSEMBLY_ROOT 0x00000002
#define ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION_PATH_EXPAND 0x00000004
#define ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION_PATH_SYSTEM_DEFAULT_REDIRECTED_SYSTEM32_DLL 0x00000008

typedef struct _ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION
{
    ULONG Size;
    ULONG Flags; // ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION_*
    ULONG TotalPathLength;
    ULONG PathSegmentCount;
    ULONG PathSegmentOffset; // to ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION_PATH_SEGMENT[], from section header
} ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION, *PACTIVATION_CONTEXT_DATA_DLL_REDIRECTION;

typedef struct _ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION_PATH_SEGMENT
{
    ULONG Length;
    ULONG Offset; // to WCHAR[], from section header
} ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION_PATH_SEGMENT, *PACTIVATION_CONTEXT_DATA_DLL_REDIRECTION_PATH_SEGMENT;

#define ACTIVATION_CONTEXT_DATA_WINDOW_CLASS_REDIRECTION_FORMAT_WHISTLER 1

typedef struct _ACTIVATION_CONTEXT_DATA_WINDOW_CLASS_REDIRECTION
{
    ULONG Size;
    ULONG Flags;
    ULONG VersionSpecificClassNameLength;
    ULONG VersionSpecificClassNameOffset; // to WHCAR[], from this struct base
    ULONG DllNameLength;
    ULONG DllNameOffset; // to WCHAR[], from section header
} ACTIVATION_CONTEXT_DATA_WINDOW_CLASS_REDIRECTION, *PACTIVATION_CONTEXT_DATA_WINDOW_CLASS_REDIRECTION;

#define ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_FORMAT_WHISTLER 1

#define ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_THREADING_MODEL_INVALID 0
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_THREADING_MODEL_APARTMENT 1
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_THREADING_MODEL_FREE 2
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_THREADING_MODEL_SINGLE 3
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_THREADING_MODEL_BOTH 4
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_THREADING_MODEL_NEUTRAL 5

#define ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_FLAG_OFFSET 8
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_HAS_DEFAULT (0x01 << ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_FLAG_OFFSET)
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_HAS_ICON (0x02 << ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_FLAG_OFFSET)
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_HAS_CONTENT (0x04 << ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_FLAG_OFFSET)
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_HAS_THUMBNAIL (0x08 << ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_FLAG_OFFSET)
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_HAS_DOCPRINT (0x10 << ACTIVATION_CONTEXT_DATA_COM_SERVER_MISCSTATUS_FLAG_OFFSET)

typedef struct _ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION
{
    ULONG Size;
    ULONG Flags;
    ULONG ThreadingModel; // ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_THREADING_MODEL_*
    GUID ReferenceClsid;
    GUID ConfiguredClsid;
    GUID ImplementedClsid;
    GUID TypeLibraryId;
    ULONG ModuleLength;
    ULONG ModuleOffset; // to WCHAR[], from section header
    ULONG ProgIdLength;
    ULONG ProgIdOffset; // to WCHAR[], from this struct base
    ULONG ShimDataLength;
    ULONG ShimDataOffset; // to ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_SHIM, from this struct base
    ULONG MiscStatusDefault;
    ULONG MiscStatusContent;
    ULONG MiscStatusThumbnail;
    ULONG MiscStatusIcon;
    ULONG MiscStatusDocPrint;
} ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION, *PACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION;

#define ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_SHIM_TYPE_OTHER 1
#define ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_SHIM_TYPE_CLR_CLASS 2

typedef struct _ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_SHIM
{
    ULONG Size;
    ULONG Flags;
    ULONG Type; // ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_SHIM_TYPE_*
    ULONG ModuleLength;
    ULONG ModuleOffset; // to WCHAR[], from section header
    ULONG TypeLength;
    ULONG TypeOffset; // to WCHAR[], from this struct base
    ULONG ShimVersionLength;
    ULONG ShimVersionOffset; // to WCHAR[], from this struct base
    ULONG DataLength;
    ULONG DataOffset; // from this struct base
} ACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_SHIM, *PACTIVATION_CONTEXT_DATA_COM_SERVER_REDIRECTION_SHIM;

#define ACTIVATION_CONTEXT_DATA_COM_INTERFACE_REDIRECTION_FORMAT_WHISTLER 1

#define ACTIVATION_CONTEXT_DATA_COM_INTERFACE_REDIRECTION_FLAG_NUM_METHODS_VALID 0x00000001
#define ACTIVATION_CONTEXT_DATA_COM_INTERFACE_REDIRECTION_FLAG_BASE_INTERFACE_VALID 0x00000002

typedef struct _ACTIVATION_CONTEXT_DATA_COM_INTERFACE_REDIRECTION
{
    ULONG Size;
    ULONG Flags; // ACTIVATION_CONTEXT_DATA_COM_INTERFACE_REDIRECTION_FLAG_*
    GUID ProxyStubClsid32;
    ULONG NumMethods;
    GUID TypeLibraryId;
    GUID BaseInterface;
    ULONG NameLength;
    ULONG NameOffset; // to WCHAR[], from this struct base
} ACTIVATION_CONTEXT_DATA_COM_INTERFACE_REDIRECTION, *PACTIVATION_CONTEXT_DATA_COM_INTERFACE_REDIRECTION;

#define ACTIVATION_CONTEXT_DATA_COM_TYPE_LIBRARY_REDIRECTION_FORMAT_WHISTLER 1

typedef struct _ACTIVATION_CONTEXT_DATA_TYPE_LIBRARY_VERSION
{
    USHORT Major;
    USHORT Minor;
} ACTIVATION_CONTEXT_DATA_TYPE_LIBRARY_VERSION, *PACTIVATION_CONTEXT_DATA_TYPE_LIBRARY_VERSION;

typedef struct _ACTIVATION_CONTEXT_DATA_COM_TYPE_LIBRARY_REDIRECTION
{
    ULONG   Size;
    ULONG   Flags;
    ULONG   NameLength;
    ULONG   NameOffset; // to WCHAR[], from section header
    USHORT  ResourceId;
    USHORT  LibraryFlags; // LIBFLAG_* oaidl.h
    ULONG   HelpDirLength;
    ULONG   HelpDirOffset; // to WCHAR[], from this struct base
    ACTIVATION_CONTEXT_DATA_TYPE_LIBRARY_VERSION Version;
} ACTIVATION_CONTEXT_DATA_COM_TYPE_LIBRARY_REDIRECTION, *PACTIVATION_CONTEXT_DATA_COM_TYPE_LIBRARY_REDIRECTION;

#define ACTIVATION_CONTEXT_DATA_COM_PROGID_REDIRECTION_FORMAT_WHISTLER 1

typedef struct _ACTIVATION_CONTEXT_DATA_COM_PROGID_REDIRECTION
{
    ULONG Size;
    ULONG Flags;
    ULONG ConfiguredClsidOffset; // to CLSID, from section header
} ACTIVATION_CONTEXT_DATA_COM_PROGID_REDIRECTION, *PACTIVATION_CONTEXT_DATA_COM_PROGID_REDIRECTION;

#define ACTIVATION_CONTEXT_DATA_CLR_SURROGATE_FORMAT_WHISTLER 1

typedef struct _ACTIVATION_CONTEXT_DATA_CLR_SURROGATE
{
    ULONG   Size;
    ULONG   Flags;
    GUID    SurrogateIdent;
    ULONG   VersionOffset;
    ULONG   VersionLength;
    ULONG   TypeNameOffset;
    ULONG   TypeNameLength; // to WCHAR[], from this struct base
} ACTIVATION_CONTEXT_DATA_CLR_SURROGATE, *PACTIVATION_CONTEXT_DATA_CLR_SURROGATE;

#define ACTIVATION_CONTEXT_DATA_APPLICATION_SETTINGS_FORMAT_LONGHORN 1

#define SXS_WINDOWS_SETTINGS_NAMESPACE L"http://schemas.microsoft.com/SMI/2005/WindowsSettings"
#define SXS_WINDOWS_SETTINGS_2011_NAMESPACE L"http://schemas.microsoft.com/SMI/2011/WindowsSettings"
#define SXS_WINDOWS_SETTINGS_2013_NAMESPACE L"http://schemas.microsoft.com/SMI/2013/WindowsSettings"
#define SXS_WINDOWS_SETTINGS_2014_NAMESPACE L"http://schemas.microsoft.com/SMI/2014/WindowsSettings"
#define SXS_WINDOWS_SETTINGS_2016_NAMESPACE L"http://schemas.microsoft.com/SMI/2016/WindowsSettings"
#define SXS_WINDOWS_SETTINGS_2017_NAMESPACE L"http://schemas.microsoft.com/SMI/2017/WindowsSettings"
#define SXS_WINDOWS_SETTINGS_2019_NAMESPACE L"http://schemas.microsoft.com/SMI/2019/WindowsSettings"
#define SXS_WINDOWS_SETTINGS_2020_NAMESPACE L"http://schemas.microsoft.com/SMI/2020/WindowsSettings"

typedef struct _ACTIVATION_CONTEXT_DATA_APPLICATION_SETTINGS
{
    ULONG Size;
    ULONG Flags;
    ULONG SettingNamespaceLength;
    ULONG SettingNamespaceOffset; // to WCHAR[], from this struct base
    ULONG SettingNameLength;
    ULONG SettingNameOffset; // to WCHAR[], from this struct base
    ULONG SettingValueLength;
    ULONG SettingValueOffset; // to WCHAR[], from this struct base
} ACTIVATION_CONTEXT_DATA_APPLICATION_SETTINGS, *PACTIVATION_CONTEXT_DATA_APPLICATION_SETTINGS;

// COMPATIBILITY_CONTEXT_ELEMENT from winnt.h before 19H1
typedef struct _COMPATIBILITY_CONTEXT_ELEMENT_LEGACY
{
    GUID Id;
    ACTCTX_COMPATIBILITY_ELEMENT_TYPE Type;
} COMPATIBILITY_CONTEXT_ELEMENT_LEGACY, *PCOMPATIBILITY_CONTEXT_ELEMENT_LEGACY;

// ACTIVATION_CONTEXT_COMPATIBILITY_INFORMATION from winnt.h before 19H1
typedef struct _ACTIVATION_CONTEXT_COMPATIBILITY_INFORMATION_LEGACY
{
    DWORD ElementCount;
    COMPATIBILITY_CONTEXT_ELEMENT_LEGACY Elements[ANYSIZE_ARRAY];
} ACTIVATION_CONTEXT_COMPATIBILITY_INFORMATION_LEGACY, *PACTIVATION_CONTEXT_COMPATIBILITY_INFORMATION_LEGACY;

#include "poppack.h"

// begin_private

typedef struct _ASSEMBLY_STORAGE_MAP_ENTRY
{
    ULONG Flags;
    UNICODE_STRING DosPath;
    HANDLE Handle;
} ASSEMBLY_STORAGE_MAP_ENTRY, *PASSEMBLY_STORAGE_MAP_ENTRY;

#define ASSEMBLY_STORAGE_MAP_ASSEMBLY_ARRAY_IS_HEAP_ALLOCATED 0x00000001

typedef struct _ASSEMBLY_STORAGE_MAP
{
    ULONG Flags;
    ULONG AssemblyCount;
    PASSEMBLY_STORAGE_MAP_ENTRY *AssemblyArray;
} ASSEMBLY_STORAGE_MAP, *PASSEMBLY_STORAGE_MAP;

typedef struct _ACTIVATION_CONTEXT *PACTIVATION_CONTEXT;

#define ACTIVATION_CONTEXT_NOTIFICATION_DESTROY 1
#define ACTIVATION_CONTEXT_NOTIFICATION_ZOMBIFY 2
#define ACTIVATION_CONTEXT_NOTIFICATION_USED 3

typedef VOID (NTAPI *PACTIVATION_CONTEXT_NOTIFY_ROUTINE)(
    _In_ ULONG NotificationType, // ACTIVATION_CONTEXT_NOTIFICATION_*
    _In_ PACTIVATION_CONTEXT ActivationContext,
    _In_ PACTIVATION_CONTEXT_DATA ActivationContextData,
    _In_opt_ PVOID NotificationContext,
    _In_opt_ PVOID NotificationData,
    _Inout_ PBOOLEAN DisableThisNotification
    );

typedef struct _ACTIVATION_CONTEXT
{
    LONG RefCount;
    ULONG Flags;
    PACTIVATION_CONTEXT_DATA ActivationContextData;
    PACTIVATION_CONTEXT_NOTIFY_ROUTINE NotificationRoutine;
    PVOID NotificationContext;
    ULONG SentNotifications[8];
    ULONG DisabledNotifications[8];
    ASSEMBLY_STORAGE_MAP StorageMap;
    PASSEMBLY_STORAGE_MAP_ENTRY InlineStorageMapEntries[32];
} ACTIVATION_CONTEXT, *PACTIVATION_CONTEXT;

#define RTL_ACTIVATION_CONTEXT_STACK_FRAME_FLAG_RELEASE_ON_DEACTIVATION 0x00000001
#define RTL_ACTIVATION_CONTEXT_STACK_FRAME_FLAG_NO_DEACTIVATE 0x00000002
#define RTL_ACTIVATION_CONTEXT_STACK_FRAME_FLAG_ON_FREE_LIST 0x00000004
#define RTL_ACTIVATION_CONTEXT_STACK_FRAME_FLAG_HEAP_ALLOCATED 0x00000008
#define RTL_ACTIVATION_CONTEXT_STACK_FRAME_FLAG_NOT_REALLY_ACTIVATED 0x00000010

typedef struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME
{
    struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME *Previous;
    PACTIVATION_CONTEXT ActivationContext;
    ULONG Flags; // RTL_ACTIVATION_CONTEXT_STACK_FRAME_FLAG_*
} RTL_ACTIVATION_CONTEXT_STACK_FRAME, *PRTL_ACTIVATION_CONTEXT_STACK_FRAME;

#define ACTIVATION_CONTEXT_STACK_FLAG_QUERIES_DISABLED 0x00000001

typedef struct _ACTIVATION_CONTEXT_STACK
{
    PRTL_ACTIVATION_CONTEXT_STACK_FRAME ActiveFrame;
    LIST_ENTRY FrameListCache;
    ULONG Flags; // ACTIVATION_CONTEXT_STACK_FLAG_*
    ULONG NextCookieSequenceNumber;
    ULONG StackId;
} ACTIVATION_CONTEXT_STACK, *PACTIVATION_CONTEXT_STACK;

// end_private

#endif