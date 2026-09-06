#pragma once
#include "SharedCrypto.h"
#include "../Common/StVariant.h"
#include "../API/PrivacyDefs.h"

#define MIN_SEC_PASSWORD      1	             // Minimum password length
#define MAX_SEC_PASSWORD      128            // Maximum password length

class LIBRARY_EXPORT CSecurePassword
{
public:
	CSecurePassword();
	CSecurePassword(const wchar_t* pass, size_t size = -1);
	~CSecurePassword();

	CSecurePassword(const CSecurePassword& other);
	CSecurePassword& operator=(const CSecurePassword& other);

	CSecurePassword(CSecurePassword&& other) noexcept;
	CSecurePassword& operator=(CSecurePassword&& other) noexcept;

	bool			operator==(const CSecurePassword& other) const;
	bool			operator!=(const CSecurePassword& other) const { return !(*this == other); }

	StVariant		ToVariant(const SVarWriteOpt& Opts) const;
	bool			FromVariant(const StVariant& Data);

	StVariant		ToVariant(const CBuffer& SharedSecret, const SVarWriteOpt& Opts) const;
	bool			FromVariant(const StVariant& Data, const CBuffer& SharedSecret);

	bool			SetPassword(const wchar_t* pass, size_t size);
	bool			SetPassword(const std::wstring& pass) { return SetPassword(pass.c_str(), pass.length() * sizeof(wchar_t)); }
	void 			ClearPassword();

	const wchar_t*	GetPassword() const;
	size_t			GetPasswordSize() const;
	size_t			GetPasswordLength() const { return GetPasswordSize() / sizeof(wchar_t); }
	bool 			IsEmpty() const { return GetPasswordSize() == 0; }

protected:
	void			AttachData(struct SSecurePassword* ptr);
	void			DetachData();
	bool			MakeExclusive();

	struct SSecurePassword* m = nullptr;
};