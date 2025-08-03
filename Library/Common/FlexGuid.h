#pragma once
#include "../lib_global.h"
#include "../Common/StVariant.h"

struct SGuid
{  
	DWORD Data1;
	WORD Data2;
	WORD Data3;
	WORD Data4;
	BYTE Data5[6];
};

class LIBRARY_EXPORT CFlexGuid
{
public:
	CFlexGuid();
	CFlexGuid(const std::wstring& Str) { FromWString(Str); }
	CFlexGuid(const std::string& Str) { FromString(Str); }
	CFlexGuid(const CFlexGuid& Other) { *this = Other; }
	CFlexGuid(CFlexGuid&& Other); // move constructor
	CFlexGuid(const FW::CVariant& Variant) { FromVariant(Variant); }
	~CFlexGuid();

	CFlexGuid& operator = (const CFlexGuid& Other);

	bool			IsNull() const { return m_Type == Null; }
	bool			IsRegular() const { return m_Type == Regular || m_Type == Null; }
	bool			IsString() const { return m_Type == Unicode || m_Type == Ascii; }

	void			Clear();

	void			SetRegularGuid(const SGuid* pGuid);
	SGuid*			GetRegularGuid() const;

	void			FromWString(const std::wstring& Str) { FromWString(Str.c_str(), Str.size()); }
	std::wstring	ToWString() const;
	void			FromString(const std::string& Str) { FromAString(Str.c_str(), Str.size()); }
	std::string		ToString() const;

	int Compare(const CFlexGuid& other) const;
	bool operator<(const CFlexGuid& other) const { return Compare(other) < 0; }
	bool operator==(const CFlexGuid& other) const { return Compare(other) == 0; }
	bool operator != (const CFlexGuid& other) const { return Compare(other) != 0; }

	bool			FromVariant(const FW::CVariant& Variant);
	FW::CVariant		ToVariant(bool bTextOnly/* = false*/, FW::AbstractMemPool* pMemPool = nullptr) const;

	static FW::CVariant WriteList(const std::vector<CFlexGuid>& List, bool bTextOnly/* = false*/, FW::AbstractMemPool* pMemPool = nullptr)
	{
		FW::CVariantWriter Writer(pMemPool);
		Writer.BeginList();
		for (const CFlexGuid& Guid : List)
			Writer.WriteVariant(Guid.ToVariant(bTextOnly, pMemPool));
		return Writer.Finish();
	}

protected:
	void			FromWString(const wchar_t* pStr, size_t uLen = -1, bool bIgnoreCase = false);
	void			FromAString(const char* pStr, size_t uLen = -1, bool bIgnoreCase = false);
	bool		    ToWString(wchar_t* pStr) const;
	bool		    ToAString(char* pStr) const;

	//
	// Note: on a 64 bit system the fields will be pointer aligned,
	// hence the size of the object if > 16 will be at leats 24,
	// so lets use put all the 24 bytes to good use
	//
	union {
		byte		m_Data[16];
		struct {
			size_t	m_Length;
			void*	m_String;
		};
	};
	// 16
	uint32			m_LowerCaseMask = 0; // WARNING: Do now change Data and LowerCaseMask order, this is a HACK, see ToVariant/FromVariant
	// 20
	enum EType : uint8
	{
		Null = 0,
		Regular = 1,
		Unicode = 2,
		Ascii = 3
	}				m_Type = Null;
	// 21
	uint8			m_Reserved1 = 0;
	// 22
	uint16			m_Reserved2 = 0;
	// 24
	// +++
	// 
	// only 3 bytes unused
	// 
};
