#pragma once

#include "../Core/Header.h"
#include "../Core/String.h"
#include "../Core/List.h"
#include "../Core/Array.h"
#include "../Core/Map.h"
#include "../Core/Table.h"
#include "../Core/Object.h"

struct FRAMEWORK_EXPORT SPatternInfo
{
	SPatternInfo() {Clear();}

	void Clear()
	{
		PatternLength = 0;
		FirstQuestionMark = (ULONG)-1;
		FirstAsterisk = (ULONG)-1;
		LastAsterisk = (ULONG)-1;
		FirstDblAsterisk = (ULONG)-1;
		LastDblAsterisk = (ULONG)-1;
	}

	bool IsExact() const 
	{ 
		if (FirstAsterisk != (ULONG)-1 && LastAsterisk == PatternLength - 1)
			return false;
		if (FirstDblAsterisk != (ULONG)-1 && FirstDblAsterisk + 1 == PatternLength - 1)
			return false;
		return true; // its exact when it does not end in a wildcard
	}

	ULONG GetMatchLen() const
	{
		ULONG uMatchLength = min(FirstAsterisk, FirstDblAsterisk);
		if(uMatchLength == (ULONG)-1)
			return PatternLength;
		return uMatchLength + 1;
	}

	ULONG PatternLength;
	ULONG FirstQuestionMark;
	ULONG FirstAsterisk;
	ULONG LastAsterisk;
	ULONG FirstDblAsterisk;
	ULONG LastDblAsterisk;
};

/////////////////////////////////////////////////////////////////////////////
// CSmartPattern
//

class FRAMEWORK_EXPORT CSmartPattern: public FW::Object
{
public:
	CSmartPattern(FW::AbstractMemPool* pMemPool, const wchar_t* pPattern = NULL);
	CSmartPattern(FW::AbstractMemPool* pMemPool, const FW::StringW& Pattern);

	void SetSeparator(wchar_t Separator) { m_Seperator = Separator; }

	int Set(const FW::StringW& Pattern);
	int Set(const wchar_t* pPattern) { return Set(FW::StringW(m_pMem, pPattern)); }
	FW::StringW Get() const { return m_Pattern; }
	const SPatternInfo& GetInfo() const { return m_Info; }
	bool IsEmpty() const { return m_Fragments.IsEmpty(); }

	struct SFragment
	{
		enum EType
		{
			eText,
			eAsterisk,
			eDblAsterisk,
			eExpression // todo: support <expression>
		};
		EType Type = eText;
		FW::StringW Text;
	};

	bool Match(FW::StringW String) const;
	bool Match(const wchar_t* pString) const	{ return Match(FW::StringW(m_pMem, pString)); }
	size_t RawMatch(const wchar_t* pString, size_t uLen, SFragment::EType &LastFragmentType) const;

	FW::StringW Print() const;

protected:
	FW::StringW m_Pattern;
	SPatternInfo m_Info;

	FW::Array<SFragment> m_Fragments;
	wchar_t m_Seperator = 0;

};

typedef FW::SharedPtr<CSmartPattern> CSmartPatternPtr;
