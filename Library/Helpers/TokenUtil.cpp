#include "pch.h"
#include "TokenUtil.h"

#include <phnt_windows.h>
#include <phnt.h>

STATUS QueryTokenVariable(HANDLE TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass, CBuffer& Buffer)
{
    NTSTATUS status;
    ULONG returnLength = 0;

    Buffer.AllocBuffer(0x80);
    status = NtQueryInformationToken(TokenHandle, TokenInformationClass, Buffer.GetBuffer(), (ULONG)Buffer.GetCapacity(), &returnLength);

    if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)
    {
        Buffer.AllocBuffer(returnLength);
        status = NtQueryInformationToken(TokenHandle, TokenInformationClass, Buffer.GetBuffer(), (ULONG)Buffer.GetCapacity(), &returnLength);
    }

    if (NT_SUCCESS(status))
        Buffer.SetSize(returnLength);

    return ERR(status);
}

#pragma warning(disable : 4996)

bool TokenIsAdmin(HANDLE hToken, bool OnlyFull)
{
	//
	// check if token is member of the Administrators group
	//

	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup;
	BOOL b = AllocateAndInitializeSid(
		&NtAuthority,
		2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&AdministratorsGroup);
	if (b) {
		if (! CheckTokenMembership(NULL, AdministratorsGroup, &b))
			b = FALSE;
		FreeSid(AdministratorsGroup);

		//
		// on Windows Vista, check for UAC split token
		//

		if (! b || OnlyFull) {
			OSVERSIONINFO osvi;
			osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			if (GetVersionEx(&osvi) && osvi.dwMajorVersion >= 6) {
				ULONG elevationType, len;
				b = GetTokenInformation(
					hToken, (TOKEN_INFORMATION_CLASS)TokenElevationType,
					&elevationType, sizeof(elevationType), &len);
				if (b && (elevationType != TokenElevationTypeFull &&
					(OnlyFull || elevationType != TokenElevationTypeLimited)))
					b = FALSE;
			}
		}
	}

	return b ? true : false;
}