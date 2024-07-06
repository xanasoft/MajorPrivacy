#include "pch.h"
#include "ProgramPattern.h"
#include "../../Library/API/PrivacyAPI.h"

CProgramPattern::CProgramPattern(const std::wstring& Pattern)
{
    m_ID.Set(EProgramType::eFilePattern, Pattern);

    SetPathPattern(Pattern);
}

void CProgramPattern::SetPathPattern(const std::wstring& Pattern)
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
    m_Specificity = (int)pos;
}

bool CProgramPattern::MatchFileName(const std::wstring& FileName)
{
	std::unique_lock lock(m_Mutex);

    std::wcmatch matches;
    return std::regex_search(FileName.c_str(), matches, m_RegExp);
}