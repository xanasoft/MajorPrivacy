#pragma once
#include "../lib_global.h"

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
