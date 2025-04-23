#pragma once
#include "../Library/Common/StVariant.h"
#include "Programs/ProgramID.h"
#include "../GenericRule.h"

#include "../../Library/API/PrivacyDefs.h"

class CAccessRule: public CGenericRule
{
	TRACK_OBJECT(CAccessRule)
public:
	CAccessRule(const CProgramID& ID);
	~CAccessRule();

	std::shared_ptr<CAccessRule> Clone(bool CloneGuid = false) const;


	void SetType(EAccessRuleType Type)				{std::unique_lock Lock(m_Mutex); m_Type = Type;}
	EAccessRuleType GetType() const					{std::shared_lock Lock(m_Mutex); return m_Type;}
	const std::wstring& GetProgramPath() const		{std::shared_lock Lock(m_Mutex); return m_ProgramPath;}
	void SetProgramPath(const std::wstring& Path)	{std::shared_lock Lock(m_Mutex); m_ProgramPath = Path;}
	const std::wstring& GetAccessPath() const		{std::shared_lock Lock(m_Mutex); return m_AccessPath;}
	void SetAccessPath(const std::wstring& Path)	{std::shared_lock Lock(m_Mutex); m_AccessPath = Path;}

	bool IsVolumeRule() const						{ return m_bVolumeRule; }
	void SetVolumeRule(bool bVolumeRule)			{ m_bVolumeRule = bVolumeRule; }

	void Update(const std::shared_ptr<CAccessRule>& Rule);

protected:
	void WriteIVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const StVariant& Data) override;
	void ReadMValue(const SVarName& Name, const StVariant& Data) override;

	EAccessRuleType m_Type = EAccessRuleType::eNone;
	std::wstring m_AccessPath; // can be pattern
	std::wstring m_ProgramPath; // can be pattern
	bool m_bVolumeRule = false;
};

typedef std::shared_ptr<CAccessRule> CAccessRulePtr;
