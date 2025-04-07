//#include "pch.h"
#include "SmartPattern.h"
#ifndef KERNEL_MODE
#include <xmemory>
#endif


/////////////////////////////////////////////////////////////////////////////
// CSmartPattern
//

CSmartPattern::CSmartPattern(FW::AbstractMemPool* pMemPool, const wchar_t* pPattern)
	: FW::Object(pMemPool), m_Pattern(pMemPool), m_Fragments(pMemPool)
{
	Set(pPattern);
}

CSmartPattern::CSmartPattern(FW::AbstractMemPool* pMemPool, const FW::StringW& Pattern)
	: FW::Object(pMemPool), m_Pattern(pMemPool), m_Fragments(pMemPool)
{
	Set(Pattern);
}

int CSmartPattern::Set(const FW::StringW& Pattern)
{
	m_Pattern = Pattern;
	m_Info.Clear();
	m_Fragments.Clear();

	if (m_Pattern.IsEmpty())
		return -3; // empty path

	size_t uLen = m_Pattern.Length();
	wchar_t* pPattern = m_Pattern.Data(); // modify raw data in string object!
	for (size_t i = 0, j = 0; ; i++)
	{
		if (i == uLen || pPattern[i] == L'*')
		{
			if (i > j)
			{
				SFragment Fragment;
				Fragment.Text = m_Pattern.SubStr(j, i - j);
				//const wchar_t* pText = Fragment.Text.ConstData();
				m_Fragments.Append(Fragment);
			}

			if(i == uLen)
				break;

			if (pPattern[i+1] == L'*') // double asterisk
			{
				// todo: two double asterisks within the same segment are not supported as well
				if (pPattern[i+2] == L'*')
					return -2; // tripple (or more) asterisks are not supported

				if (m_Info.FirstDblAsterisk == (ULONG)-1)
					m_Info.FirstDblAsterisk = (ULONG)i;
				m_Info.LastDblAsterisk = (ULONG)i;

				i++; // skip second asterisk

				m_Fragments.Append(SFragment{ SFragment::eDblAsterisk });
			}
			else
			{
				if (m_Info.FirstAsterisk == (ULONG)-1)
					m_Info.FirstAsterisk = (ULONG)i;
				m_Info.LastAsterisk = (ULONG)i;

				m_Fragments.Append(SFragment{ SFragment::eAsterisk });
			}

			j = i + 1;
			continue;
		}

		//if (cur == L'<')
		//{
		//	// todo: <expression>
		//}

		wchar_t cur = pPattern[i];
		if ((cur >= L'A') && (cur <= L'Z'))
			pPattern[i] += 32; // to lower case

		if (cur == L'\"' || cur == L'<' || cur == L'>' || cur == L'|')
			return -4; // Reject not allowed characters

		// Check for ASCII control characters (1–31 and 127 (delete))
		if (cur < 32 || cur == 127)
			return -4; // Reject invalid control characters

		if (cur == L'?' && m_Info.FirstQuestionMark == (ULONG)-1)
			m_Info.FirstQuestionMark = (ULONG)i;
	}

	m_Info.PatternLength = (ULONG)uLen;

	return 0; // no error
}

static bool CSmartPattern__MatchString(const wchar_t* pLStr, const wchar_t* pRStr, size_t uLen)
{
	for (size_t uCur = 0; ; uCur++)
	{
		if (uCur == uLen)
			return true;
		if (pLStr[uCur] != pRStr[uCur] && pLStr[uCur] != L'?')
			return false;
	}
}

static size_t CSmartPattern__MatchSuffix(const wchar_t* pSuffix, size_t uSuffixLen, const wchar_t* pStr, size_t uStrLen, size_t uStrPos, wchar_t Seperator)
{
	const wchar_t* pStrEnd = pStr + uStrLen - uSuffixLen + 1;
	for (const wchar_t* pPtr = pStr + uStrPos; pPtr < pStrEnd; pPtr++)
	{
		if (Seperator && pPtr[uSuffixLen] == Seperator)
			break;
		if (CSmartPattern__MatchString(pSuffix, pPtr, uSuffixLen))
			return (pPtr - pStr) - uStrPos; // Suffix matched
	}
	return FW::StringW::NPos; // no match
}

bool CSmartPattern::Match(FW::StringW String) const
{
	String.MakeLower();

	size_t uLen = String.Length();
	const wchar_t* pString = String.ConstData();

	SFragment::EType LastFragmentType = SFragment::eText;
	size_t uPos = RawMatch(pString, uLen, LastFragmentType);
	if (uPos == FW::StringW::NPos)
		return false;
	if (uPos == uLen)
		return true;

	if (LastFragmentType == SFragment::eAsterisk || LastFragmentType == SFragment::eDblAsterisk) 
	{
		if (m_Seperator && LastFragmentType == SFragment::eAsterisk)
		{
			for (; uPos < uLen; uPos++) {
				if (pString[uPos] == m_Seperator)
					return false;
			}
		}
		return true;
	}

	return false;
}

size_t CSmartPattern::RawMatch(const wchar_t* pString, size_t uLen, SFragment::EType &LastFragmentType) const
{
	size_t uPos = 0;

	for (size_t i = 0; i < m_Fragments.Count(); i++)
	{
		const SFragment& Fragment = m_Fragments[i];
		switch (Fragment.Type)
		{
		case SFragment::eText:
		{
			size_t uTextLen = Fragment.Text.Length();
			const wchar_t* pText = Fragment.Text.ConstData();
			if (uTextLen > uLen - uPos)
				return FW::StringW::NPos; // no match
			if (LastFragmentType == SFragment::eAsterisk || LastFragmentType == SFragment::eDblAsterisk ) 
			{
				size_t uOffset = CSmartPattern__MatchSuffix(pText, uTextLen, pString, uLen, uPos, LastFragmentType == SFragment::eAsterisk ? m_Seperator : 0);
				if (uOffset == FW::StringW::NPos)
					return FW::StringW::NPos; // no match
				uPos += uOffset + uTextLen;
			}
			else
			{
				if (!CSmartPattern__MatchString(pText, pString + uPos, uTextLen))
					return FW::StringW::NPos; // no match
				uPos += uTextLen;
			}
		}
		break;
		case SFragment::eAsterisk:
		case SFragment::eDblAsterisk:
		break;
		case SFragment::eExpression:
		{
			return FW::StringW::NPos; // todo: support <expression>
		}
		break;
		}
		LastFragmentType = Fragment.Type;
	}

	return uPos;
}

FW::StringW CSmartPattern::Print() const
{
	FW::StringW Result(m_pMem);
	for (size_t i = 0; i < m_Fragments.Count(); i++)
	{
		if (!Result.IsEmpty())
			Result += L"|";
		const SFragment& Fragment = m_Fragments[i];
		switch (Fragment.Type)
		{
		case SFragment::eText:
			Result += Fragment.Text;
			break;
		case SFragment::eAsterisk:
			Result += L"*";
			break;
		case SFragment::eDblAsterisk:
			Result += L"**";
			break;
		case SFragment::eExpression:
			Result += L"<";
			Result += Fragment.Text;
			Result += L">";
			break;
		}
	}
	return Result;
}