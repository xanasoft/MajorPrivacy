#pragma once
#include "../lib_global.h"



struct LIBRARY_EXPORT SNtString
{
	SNtString(const std::wstring& Value)
	{
		value = Value;
		uni.MaximumLength = (uni.Length = (USHORT)value.length() * sizeof(WCHAR)) + sizeof(UNICODE_NULL);
		uni.Buffer = (PWCH)value.c_str();
	}

	SNtString(size_t len)
	{
		value.resize(len);
		uni.MaximumLength = ((USHORT)value.length() * sizeof(WCHAR)) + sizeof(UNICODE_NULL);
		uni.Length = 0;
		uni.Buffer = (PWCH)value.c_str();
	}

	PUNICODE_STRING Get() { return &uni; }
	std::wstring Value() { return value.substr(0, uni.Length / sizeof(WCHAR)); }

	std::wstring value;
	UNICODE_STRING uni;

private:
	SNtString(const SNtString&) {}
	SNtString& operator=(const SNtString&) { return *this; }
};


struct LIBRARY_EXPORT SNtObject
{
	SNtObject(const std::wstring& Name, HANDLE parent = NULL, PSECURITY_DESCRIPTOR sd = NULL)
	{
		name = Name;
		RtlInitUnicodeString(&uni, name.c_str());
		InitializeObjectAttributes(&attr, &uni, OBJ_CASE_INSENSITIVE, parent, sd);
	}

	POBJECT_ATTRIBUTES Get() { return &attr; }

	std::wstring name;
	UNICODE_STRING uni;
	OBJECT_ATTRIBUTES attr;

private:
	SNtObject(const SNtObject&) {}
	SNtObject& operator=(const SNtObject&) { return *this; }
};
