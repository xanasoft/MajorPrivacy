#pragma once
#include "ProgramGroup.h"
#include <regex>

class CProgramPattern: public CProgramList
{
public:
	CProgramPattern(const std::wstring& Pattern);

	std::wstring GetPattern() const				{ std::shared_lock lock(m_Mutex); return m_Pattern; }
	bool MatchFileName(const std::wstring& FileName);

	virtual std::wstring GetPath() const		{ std::shared_lock lock(m_Mutex); return GetPattern(); }

	virtual int									GetSpecificity() const { return m_Specificity; }

protected:

	virtual void WriteVariant(CVariant& Data) const;

	std::wstring m_Pattern;
	std::wregex m_RegExp;
	int m_Specificity = 0;
};


typedef std::shared_ptr<CProgramPattern> CProgramPatternPtr;
typedef std::weak_ptr<CProgramPattern> CProgramPatternRef;