#pragma once
/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

//
// UEFI firmware variables.
//
// Everything the bootloader installer does that is not file copying goes
// through these: the boot manager's own entries, Secure Boot's state, and the
// MOK variables shim reads at boot. Windows exposes them through
// Get/SetFirmwareEnvironmentVariable, which needs SE_SYSTEM_ENVIRONMENT_NAME -
// so nothing here works unelevated, and CEfiVars::Init says so rather than
// failing later with a privilege error nobody can place.
//
// Ported from DiskCryptor's dcapi/efiinst.c and dcapi/mok_sup.c.
//

#include "../lib_global.h"
#include "../Status.h"

#include <string>
#include <vector>

//
// The two GUID namespaces everything here lives in. The global one is UEFI's
// own; the shim one is where MokManager keeps its lists.
//
#define EFI_GLOBAL_VAR_GUID		L"{8BE4DF61-93CA-11D2-AA0D-00E098032B8C}"
#define EFI_SHIM_VAR_GUID		L"{605DAB50-E046-4300-ABB6-3DD810DD8B23}"
#define EFI_SECURITY_VAR_GUID	L"{D719B2CB-3D3A-4596-A3BC-DAD00E67656F}"

class LIBRARY_EXPORT CEfiVars
{
public:
	//
	// Acquire SE_SYSTEM_ENVIRONMENT_NAME and confirm the machine booted UEFI.
	// Call once before anything else; the result is cached.
	//
	static STATUS			Init();

	// True when the firmware variable interface answered at all, which on
	// Windows is also the test for "this machine booted UEFI, not BIOS".
	static bool				IsUefiBoot();

	static bool				IsSecureBoot();

	//
	// Secure Boot state without needing any privilege, which the firmware
	// variable above does. Windows publishes it in the registry for exactly
	// this purpose, so an unelevated process can show it.
	//
	// 1 on, 0 off, -1 not known - no UEFI, or a Windows that does not publish
	// the value. The three are worth keeping apart: "off" and "could not tell"
	// mean very different things to someone reading a status line.
	//
	static int				GetSecureBootStatus();
	static bool				IsSetupMode();

	//
	// Raw variable access. Get returns an empty vector both for a variable that
	// does not exist and for one that is genuinely empty; pass pExists when the
	// difference matters.
	//
	static std::vector<unsigned char>	Get(const std::wstring& Name, const std::wstring& Guid = EFI_GLOBAL_VAR_GUID, bool* pExists = nullptr);
	static STATUS			Set(const std::wstring& Name, const std::wstring& Guid, const void* pData, size_t uSize, unsigned long uAttributes = 0);
	static STATUS			Set(const std::wstring& Name, const std::wstring& Guid, const std::vector<unsigned char>& Data, unsigned long uAttributes = 0);
	static STATUS			Delete(const std::wstring& Name, const std::wstring& Guid = EFI_GLOBAL_VAR_GUID);
	static bool				Exists(const std::wstring& Name, const std::wstring& Guid = EFI_GLOBAL_VAR_GUID);

	//
	// BootOrder and friends, as numbers rather than raw little-endian bytes.
	// A boot entry is BootXXXX where XXXX is upper-case hex, which is the one
	// piece of UEFI naming it is worth having a helper for.
	//
	static std::wstring		BootEntryName(unsigned short uIndex, const std::wstring& Prefix = L"Boot");
	static std::vector<unsigned short>	GetBootOrder();
	static STATUS			SetBootOrder(const std::vector<unsigned short>& Order);

protected:
	static bool				m_bInitialized;
	static bool				m_bUefi;
	static STATUS			m_InitStatus;
};
