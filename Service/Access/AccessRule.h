#pragma once
#include "../Library/Common/Variant.h"
#include "Programs/ProgramID.h"
#include "../GenericRule.h"

#include "../../Library/API/PrivacyDefs.h"

class CAccessRule: public CGenericRule
{
public:
	CAccessRule(const CProgramID& ID);
	~CAccessRule();

	std::shared_ptr<CAccessRule> Clone() const;


	EAccessRuleType GetType() const					{std::shared_lock Lock(m_Mutex); return m_Type;}
	const std::wstring& GetProgramPath() const		{std::shared_lock Lock(m_Mutex); return m_ProgramPath;}
	void SetProgramPath(const std::wstring& Path)	{std::shared_lock Lock(m_Mutex); m_ProgramPath = Path;}
	const std::wstring& GetAccessPath() const		{std::shared_lock Lock(m_Mutex); return m_AccessPath;}
	void SetAccessPath(const std::wstring& Path)	{std::shared_lock Lock(m_Mutex); m_AccessPath = Path;}

	void Update(const std::shared_ptr<CAccessRule>& Rule);

protected:
	void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const CVariant& Data) override;
	void ReadMValue(const SVarName& Name, const CVariant& Data) override;

	EAccessRuleType m_Type = EAccessRuleType::eNone;
	std::wstring m_AccessPath; // can be pattern
	std::wstring m_ProgramPath; // can be pattern
};

typedef std::shared_ptr<CAccessRule> CAccessRulePtr;
