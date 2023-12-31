#pragma once
#include "../lib_global.h"
#include "../Types.h"

struct LIBRARY_EXPORT SSid
{
	SSid();
	SSid(const std::string& Str);
	SSid(struct _SID* pSid);

	void Set(struct _SID* pSid);
	const void* Get() const { return Value.data(); }

	std::string ToString() const;
	std::wstring ToWString() const;

	std::vector<uint8> Value;
};