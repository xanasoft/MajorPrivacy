#pragma once
#include "ProgramGroup.h"
#include <regex>

typedef std::wstring		TPatternId;	// Pattern as string

class CProgramPattern: public CProgramList
{
public:
	CProgramPattern(const std::wstring& Pattern);

	virtual EProgramType GetType() const		{ return EProgramType::eFilePattern; }

	std::wstring GetPattern() const				{ std::unique_lock lock(m_Mutex); return m_Pattern; }
	bool MatchFileName(const std::wstring& FileName);

	//virtual std::wstring GetPath() const		{ std::unique_lock lock(m_Mutex); return GetPattern(); }

	virtual int									GetSpecificity() const { return m_Specificity; }

protected:
	friend class CAppInstallation;
	CProgramPattern() {}

	void SetPathPattern(const std::wstring& Pattern);

	void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const CVariant& Data) override;
	void ReadMValue(const SVarName& Name, const CVariant& Data) override;

	std::wstring m_Pattern;
	std::wregex m_RegExp;
	int m_Specificity = 0;
};


typedef std::shared_ptr<CProgramPattern> CProgramPatternPtr;
typedef std::weak_ptr<CProgramPattern> CProgramPatternRef;