#include "pch.h"
#include "SID.h"

#include <phnt_windows.h>
#include <phnt.h>

#include <sddl.h> // For ConvertSidToStringSid

SSid::SSid()
{
}

SSid::SSid(const std::string& Str)
{
    PSID pSid = nullptr;
    if (ConvertStringSidToSidA(Str.c_str(), &pSid)) {
        Set((struct _SID*)pSid);
        LocalFree(pSid);
    }
}

SSid::SSid(struct _SID* pSid)
{
    Set(pSid);
}

void SSid::Set(struct _SID* pSid)
{
	Value.resize(RtlLengthSid(pSid));
	memcpy(Value.data(), pSid, Value.size());
}

std::string SSid::ToString() const
{
    LPSTR sidString = nullptr;
    if (ConvertSidToStringSidA((PSID)Value.data(), &sidString)) {
        std::string result(sidString);
        LocalFree(sidString);
        return result;
    }
    return "";
}

std::wstring SSid::ToWString() const
{
    LPWSTR sidString = nullptr;
    if (ConvertSidToStringSidW((PSID)Value.data(), &sidString)) {
        std::wstring result(sidString);
        LocalFree(sidString);
        return result;
    }
    return L"";
}