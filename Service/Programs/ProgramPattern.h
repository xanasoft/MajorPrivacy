#pragma once
#include "ProgramGroup.h"
#include <regex>

class CProgramPattern: public CProgramListEx
{
	TRACK_OBJECT(CProgramPattern)
public:
	CProgramPattern(const std::wstring& Pattern);

	virtual EProgramType GetType() const		{ return EProgramType::eFilePattern; }

	std::wstring GetPattern() const				{ std::unique_lock lock(m_Mutex); return m_Pattern; }

	virtual std::wstring GetPath() const		{ return GetPattern(); }

	virtual bool MatchFileName(const std::wstring& FileName) const;

protected:
	friend class CAppInstallation;
	CProgramPattern() {}

	void SetDosPattern(const std::wstring& Pattern);

	void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const CVariant& Data) override;
	void ReadMValue(const SVarName& Name, const CVariant& Data) override;

	std::wstring m_Pattern;
	std::wregex m_RegExp;
};


typedef std::shared_ptr<CProgramPattern> CProgramPatternPtr;
typedef std::weak_ptr<CProgramPattern> CProgramPatternRef;