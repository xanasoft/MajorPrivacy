#include "pch.h"
#include "StVariant.h"


std::vector<byte> StVariant::AsBytes() const
{
	if (!IsRaw())
		return std::vector<byte>();

	std::vector<byte> vec(GetSize()); 
	MemCopy(vec.data(), GetData(), vec.size()); 
	return vec; 
}

std::string StVariant::ToString() const
{
	std::string str;
	if(m_Type == VAR_TYPE_EMPTY)
		return str;

	if(m_Type == VAR_TYPE_ASCII || m_Type == VAR_TYPE_UTF8 || m_Type == VAR_TYPE_BYTES)
		str = std::string((char*)GetData(), GetSize());
	else if(m_Type == VAR_TYPE_UNICODE)
	{
		std::wstring wstr = ToWString();
		str = w2s(wstr);
	}
#ifndef VAR_NO_EXCEPTIONS
	else
		throw CException(ErrorString(eErrTypeMismatch));
#endif
	return str;
}

std::wstring StVariant::ToWString() const
{
	std::wstring wstr;
	if(m_Type == VAR_TYPE_EMPTY)
		return wstr;

	if(m_Type == VAR_TYPE_UNICODE)
		wstr = std::wstring((wchar_t*)GetData(), GetSize() / sizeof(wchar_t));
	else if(m_Type == VAR_TYPE_ASCII || m_Type == VAR_TYPE_BYTES)
	{
		std::string str = std::string((char*)GetData(), GetSize());
		wstr = s2w(str);
	}
	else if(m_Type == VAR_TYPE_UTF8)
	{
		FW::StringW StrW = FromUtf8(m_pMem, (char*)GetData(), GetSize());
		wstr = std::wstring(StrW.ConstData(), StrW.Length());
	}
#ifndef VAR_NO_EXCEPTIONS
	else
		throw CException(ErrorString(eErrTypeMismatch));
#endif
	return wstr;
}

std::wstring StVariant::AsStr(bool* pOk) const
{
	if (pOk) *pOk = true;
#ifndef VAR_NO_EXCEPTIONS
	try {
#endif
		EType Type = GetType();
		if (Type == VAR_TYPE_ASCII || Type == VAR_TYPE_BYTES || Type == VAR_TYPE_UTF8 || Type == VAR_TYPE_UNICODE)
			return ToWString();
		else if (Type == VAR_TYPE_SINT) {
			sint64 sint = 0;
			GetInt(sizeof(sint64), &sint);
			return std::to_wstring(sint);
		}
		else if (Type == VAR_TYPE_UINT) {
			uint64 uint = 0;
			GetInt(sizeof(sint64), &uint);
			return std::to_wstring(uint);
		}
#ifndef VAR_NO_FPU
		else if (Type == VAR_TYPE_DOUBLE) {
			double val = 0;
			GetDouble(sizeof(double), &val);
			return std::to_wstring(val);
		}
#endif
#ifndef VAR_NO_EXCEPTIONS
	} catch (...) {}
	if (pOk) *pOk = false;
#endif
	return L"";
}