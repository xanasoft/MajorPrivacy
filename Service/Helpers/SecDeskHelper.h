#pragma once

#include <windows.h>
#include <string>

int ShowSecureMessageBox(const std::wstring& text, const std::wstring& title, UINT uType, const std::wstring& backgroundImagePath = std::wstring());

int ShowSecureDialog(int(*func)(HWND hWnd, void* param), void* param, const std::wstring& backgroundImagePath = std::wstring());

DWORD GetPromptOnSecureDesktop();

/////////////////////////////////////////////////////////////////////////////
// Password Dialog
//

// Password dialog types (mirrored from PrivacyAPI.h)
#ifndef PDLG_ENTER_PW
#define PDLG_ENTER_PW    0x100   // Single password entry
#define PDLG_CONFIRM_PW  0x200   // Password with confirmation field
#endif

#define PW_MAX_LENGTH    128

#pragma pack(push, 1)
struct SPasswordPromptSection
{
    // Input (from service to dialog process)
    struct {
        UINT    uType;                  // PDLG_ENTER_PW or PDLG_CONFIRM_PW
        WCHAR   szPrompt[256];          // Prompt text
        WCHAR   szTitle[64];            // Dialog title

        // Localized UI strings (empty = use English defaults)
        WCHAR   szLblPassword[64];      // "Password:" label
        WCHAR   szLblConfirm[64];       // "Confirm:" label
        WCHAR   szLblShow[64];          // Tooltip for show password button
        WCHAR   szBtnOk[64];            // "OK" button
        WCHAR   szBtnCancel[64];        // "Cancel" button
        WCHAR   szErrMismatch[128];     // "Passwords do not match!" error
        WCHAR   szErrToLong[128];       // "Password is too long!" error
    } in;

    // Output (from dialog process to service)
    struct {
        NTSTATUS status;                // STATUS_SUCCESS, STATUS_CANCELLED, etc.
        WCHAR   szPassword[PW_MAX_LENGTH + 1]; // Entered password (wiped after read)
    } out;
};
static_assert(sizeof(SPasswordPromptSection) <= 0x1000, "Section too large");
#pragma pack(pop)

int ShowSecurePasswordDialog(HWND hParent, SPasswordPromptSection* pSection, const std::wstring& backgroundImagePath = std::wstring());