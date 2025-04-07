#pragma once
#include "NtObj.h"
#include "../Framework/Common/Buffer.h"


LIBRARY_EXPORT bool NtIo_WaitForFolder(POBJECT_ATTRIBUTES objattrs, int seconds = 10, bool (*cb)(const WCHAR* info, void* param) = NULL, void* param = NULL);

LIBRARY_EXPORT BOOLEAN NtIo_FileExists(POBJECT_ATTRIBUTES objattrs);

LIBRARY_EXPORT NTSTATUS NtIo_RemoveProblematicAttributes(POBJECT_ATTRIBUTES objattrs);

LIBRARY_EXPORT NTSTATUS NtIo_RemoveJunction(POBJECT_ATTRIBUTES objattrs);

LIBRARY_EXPORT NTSTATUS NtIo_DeleteFile(SNtObject& ntObject, bool (*cb)(const WCHAR* info, void* param) = NULL, void* param = NULL);
LIBRARY_EXPORT NTSTATUS NtIo_DeleteFolderRecursively(POBJECT_ATTRIBUTES objattrs, bool (*cb)(const WCHAR* info, void* param) = NULL, void* param = NULL);

LIBRARY_EXPORT NTSTATUS NtIo_RenameFile(POBJECT_ATTRIBUTES src_objattrs, POBJECT_ATTRIBUTES dest_objattrs, const WCHAR* DestName);
LIBRARY_EXPORT NTSTATUS NtIo_RenameFolder(POBJECT_ATTRIBUTES src_objattrs, POBJECT_ATTRIBUTES dest_objattrs, const WCHAR* DestName);
LIBRARY_EXPORT NTSTATUS NtIo_RenameJunction(POBJECT_ATTRIBUTES src_objattrs, POBJECT_ATTRIBUTES dest_objattrs, const WCHAR* DestName);

LIBRARY_EXPORT NTSTATUS NtIo_MergeFolder(POBJECT_ATTRIBUTES src_objattrs, POBJECT_ATTRIBUTES dest_objattrs, bool (*cb)(const WCHAR* info, void* param) = NULL, void* param = NULL);

LIBRARY_EXPORT NTSTATUS NtIo_ReadFile(const std::wstring &NtPath, CBuffer *Data);
LIBRARY_EXPORT NTSTATUS NtIo_WriteFile(const std::wstring &NtPath, const CBuffer *Data);