/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

#include "pch.h"
#include "EfiVars.h"

#include <windows.h>

bool	CEfiVars::m_bInitialized = false;
bool	CEfiVars::m_bUefi = false;
STATUS	CEfiVars::m_InitStatus;

//
// The largest a firmware variable is allowed to get. UEFI itself has no fixed
// limit, but the MOK lists are the biggest thing this code reads and they are
// measured in kilobytes; 1 MiB is generous and stops a malformed size from
// turning into a huge allocation.
//
#define EFI_VAR_MAX_SIZE  (1024 * 1024)

//
// Turn the privilege on, and say what went wrong if it did not. The result is
// worth reporting but not worth deciding on - see Init below.
//
static DWORD EnablePrivilege(LPCWSTR Name)
{
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return GetLastError();

	TOKEN_PRIVILEGES tp = { 0 };
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	DWORD uError = ERROR_SUCCESS;
	if (!LookupPrivilegeValueW(NULL, Name, &tp.Privileges[0].Luid))
		uError = GetLastError();
	else {
		//
		// AdjustTokenPrivileges reports success even when it granted nothing,
		// and signals that through the last error - which therefore has to be
		// cleared first, or what LookupPrivilegeValue left behind is read back
		// as a failure.
		//
		SetLastError(ERROR_SUCCESS);
		AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
		uError = GetLastError();
	}

	CloseHandle(hToken);
	return uError;
}

STATUS CEfiVars::Init()
{
	if (m_bInitialized)
		return m_InitStatus;
	m_bInitialized = true;

	//
	// Enabling the privilege is an attempt, not a verdict. It can report a
	// failure while the firmware calls work perfectly well - the privilege may
	// already be enabled, so that there was nothing to grant. The only thing
	// that settles it is asking the firmware, so that is what decides here.
	//
	DWORD uPrivError = EnablePrivilege(SE_SYSTEM_ENVIRONMENT_NAME);

	//
	// Asking for a variable that cannot exist is how Windows is asked whether
	// there is a firmware variable store at all, and what this process may do
	// with it. On a BIOS machine the call fails with ERROR_INVALID_FUNCTION;
	// without the privilege it fails with ERROR_PRIVILEGE_NOT_HELD; on UEFI it
	// gets as far as looking and comes back ERROR_ENVVAR_NOT_FOUND.
	//
	SetLastError(ERROR_SUCCESS);
	unsigned char Dummy[4];
	GetFirmwareEnvironmentVariableW(L" ", L"{00000000-0000-0000-0000-000000000000}", Dummy, sizeof(Dummy));
	DWORD uError = GetLastError();

	switch (uError)
	{
	case ERROR_SUCCESS:
	case ERROR_ENVVAR_NOT_FOUND:
	case ERROR_INSUFFICIENT_BUFFER:
	case ERROR_MORE_DATA:
		m_bUefi = true;
		break;

	case ERROR_INVALID_FUNCTION:
	case ERROR_NOT_SUPPORTED:
		m_InitStatus = ERR(STATUS_NOT_SUPPORTED,
			L"This machine did not boot through UEFI, so it has no firmware variables and no EFI boot loader.");
		break;

	case ERROR_PRIVILEGE_NOT_HELD:
	case ERROR_ACCESS_DENIED:
		m_InitStatus = ERR(STATUS_PRIVILEGE_NOT_HELD,
			L"Reading UEFI firmware variables requires administrator rights ("
			L"SeSystemEnvironmentPrivilege, error " + std::to_wstring(uError)
			+ (uPrivError != ERROR_SUCCESS ? L", enabling it failed with " + std::to_wstring(uPrivError) : L"")
			+ L").");
		break;

	default:
		m_InitStatus = ERR(STATUS_UNSUCCESSFUL,
			L"The UEFI firmware variables could not be read (error "
			+ std::to_wstring(uError) + L").");
		break;
	}

	return m_InitStatus;
}

bool CEfiVars::IsUefiBoot()
{
	Init();
	return m_bUefi;
}

bool CEfiVars::IsSecureBoot()
{
	std::vector<unsigned char> Value = Get(L"SecureBoot");
	return !Value.empty() && Value[0] != 0;
}

int CEfiVars::GetSecureBootStatus()
{
	DWORD uValue = 0;
	DWORD uSize = sizeof(uValue);

	if (RegGetValueW(HKEY_LOCAL_MACHINE,
			L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State",
			L"UEFISecureBootEnabled", RRF_RT_REG_DWORD, NULL, &uValue, &uSize) == ERROR_SUCCESS)
		return uValue ? 1 : 0;

	//
	// No registry value - on a BIOS machine there is none to publish. Ask the
	// firmware, which only answers for a process holding the privilege, and
	// admit to not knowing otherwise.
	//
	if (IsUefiBoot())
		return IsSecureBoot() ? 1 : 0;

	return -1;
}

bool CEfiVars::IsSetupMode()
{
	std::vector<unsigned char> Value = Get(L"SetupMode");
	return !Value.empty() && Value[0] != 0;
}

std::vector<unsigned char> CEfiVars::Get(const std::wstring& Name, const std::wstring& Guid, bool* pExists)
{
	if (pExists) *pExists = false;

	if (!IsUefiBoot())
		return std::vector<unsigned char>();

	//
	// There is no way to ask how long a variable is, so the buffer is grown
	// until it stops complaining. Starting small keeps the common case - a
	// single byte of SecureBoot state - from allocating a megabyte.
	//
	std::vector<unsigned char> Data(256);
	for (;;)
	{
		SetLastError(ERROR_SUCCESS);
		DWORD uRead = GetFirmwareEnvironmentVariableW(Name.c_str(), Guid.c_str(), Data.data(), (DWORD)Data.size());
		if (uRead > 0) {
			Data.resize(uRead);
			if (pExists) *pExists = true;
			return Data;
		}

		DWORD uError = GetLastError();
		if (uError == ERROR_SUCCESS) {
			// a variable that exists and is empty
			if (pExists) *pExists = true;
			return std::vector<unsigned char>();
		}
		if (uError != ERROR_INSUFFICIENT_BUFFER && uError != ERROR_MORE_DATA)
			return std::vector<unsigned char>();

		if (Data.size() >= EFI_VAR_MAX_SIZE)
			return std::vector<unsigned char>();
		Data.resize(Data.size() * 4 < EFI_VAR_MAX_SIZE ? Data.size() * 4 : EFI_VAR_MAX_SIZE);
	}
}

STATUS CEfiVars::Set(const std::wstring& Name, const std::wstring& Guid, const void* pData, size_t uSize, unsigned long uAttributes)
{
	STATUS Status = Init();
	if (Status.IsError())
		return Status;

	BOOL bOk;
	if (uAttributes)
		bOk = SetFirmwareEnvironmentVariableExW(Name.c_str(), Guid.c_str(), (PVOID)pData, (DWORD)uSize, uAttributes);
	else
		bOk = SetFirmwareEnvironmentVariableW(Name.c_str(), Guid.c_str(), (PVOID)pData, (DWORD)uSize);

	if (!bOk)
		return ERR(STATUS_UNSUCCESSFUL, L"Failed to write the UEFI variable " + Name + L".");
	return OK;
}

STATUS CEfiVars::Set(const std::wstring& Name, const std::wstring& Guid, const std::vector<unsigned char>& Data, unsigned long uAttributes)
{
	return Set(Name, Guid, Data.empty() ? NULL : Data.data(), Data.size(), uAttributes);
}

STATUS CEfiVars::Delete(const std::wstring& Name, const std::wstring& Guid)
{
	// writing zero bytes is how a firmware variable is removed
	return Set(Name, Guid, NULL, 0);
}

bool CEfiVars::Exists(const std::wstring& Name, const std::wstring& Guid)
{
	bool bExists = false;
	Get(Name, Guid, &bExists);
	return bExists;
}

std::wstring CEfiVars::BootEntryName(unsigned short uIndex, const std::wstring& Prefix)
{
	wchar_t Name[32];
	swprintf_s(Name, L"%s%04X", Prefix.c_str(), uIndex);
	return Name;
}

std::vector<unsigned short> CEfiVars::GetBootOrder()
{
	std::vector<unsigned char> Data = Get(L"BootOrder");

	std::vector<unsigned short> Order;
	Order.reserve(Data.size() / sizeof(unsigned short));
	for (size_t i = 0; i + sizeof(unsigned short) <= Data.size(); i += sizeof(unsigned short))
		Order.push_back((unsigned short)(Data[i] | (Data[i + 1] << 8)));
	return Order;
}

STATUS CEfiVars::SetBootOrder(const std::vector<unsigned short>& Order)
{
	std::vector<unsigned char> Data;
	Data.reserve(Order.size() * sizeof(unsigned short));
	for (size_t i = 0; i < Order.size(); i++) {
		Data.push_back((unsigned char)(Order[i] & 0xFF));
		Data.push_back((unsigned char)(Order[i] >> 8));
	}
	return Set(L"BootOrder", EFI_GLOBAL_VAR_GUID, Data);
}
