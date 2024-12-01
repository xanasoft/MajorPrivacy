#pragma once
#include "../lib_global.h"
#include "../Status.h"
#include "../Common/Variant.h"

enum SVC_OPTIONS {
    OPT_NONE    = 0,
    OPT_SYSTEM_START    = 0x01,
    OPT_AUTO_START      = 0x02,
    OPT_DEMAND_START    = 0x04,
    OPT_OWN_TYPE        = 0x08,
    OPT_KERNEL_TYPE     = 0x10,
    OPT_START_NOW       = 0x20
};

LIBRARY_EXPORT STATUS InstallService(PCWSTR Name, PCWSTR FilePath, PCWSTR Display, PCWSTR Group, uint32 Options, const CVariant& Params = CVariant());

enum SVC_STATE {
    SVC_NOT_FOUND = 0x00,
    SVC_INSTALLED = 0x01,
    SVC_RUNNING   = 0x03
};

LIBRARY_EXPORT SVC_STATE GetServiceState(PCWSTR Name);

LIBRARY_EXPORT STATUS RunService(PCWSTR Name);
LIBRARY_EXPORT STATUS KillService(PCWSTR Name);

LIBRARY_EXPORT DWORD GetServiceStart(PCWSTR Name);
LIBRARY_EXPORT STATUS SetServiceStart(PCWSTR Name, DWORD StartValue);

LIBRARY_EXPORT std::wstring GetServiceNameFromTag(HANDLE ProcessId, ULONG ServiceTag);

LIBRARY_EXPORT std::wstring GetServiceBinaryPath(PCWSTR Name);

LIBRARY_EXPORT STATUS RemoveService(PCWSTR Name);   