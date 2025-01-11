#include "pch.h"
#include "ProgramPattern.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Library/Common/Strings.h"
#include "../Programs/ProgramManager.h"
#include "../ServiceCore.h"

CProgramPattern::CProgramPattern(const std::wstring& Pattern)
{
    m_ID = CProgramID(MkLower(Pattern));

    SetDosPattern(Pattern);
}

void CProgramPattern::SetDosPattern(const std::wstring& Pattern)
{
	m_Pattern = Pattern; 

    std::wstring regex = Pattern;
    static const std::wregex specialChars(L"[.^$|()\\[\\]{}*+?\\\\]");
    regex = std::regex_replace(regex, specialChars, L"\\$&");
    static const std::wregex asterisk(L"\\\\\\*");
    regex = std::regex_replace(regex, asterisk, L".*");
    static const std::wregex questionMark(L"\\\\\\?");
    regex = std::regex_replace(regex, questionMark, L".");

    m_RegExp = std::wregex(L"^" + regex + L"$", std::regex_constants::icase);

    size_t pos = Pattern.find(L'*');
    if (pos == std::wstring::npos)
        pos = Pattern.length();
    //m_Specificity = (int)pos;
}

bool CProgramPattern::MatchFileName(const std::wstring& FileName) const
{
	std::unique_lock lock(m_Mutex);

    //
    // Note: we may have multiple identical patterns we must not add them to each other
    //

    if (m_Pattern.empty() || m_Pattern == FileName)
        return false;

    std::wcmatch matches;
    if (std::regex_search(FileName.c_str(), matches, m_RegExp))
        return true;
    return false;
}