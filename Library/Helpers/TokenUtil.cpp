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