#pragma once
#include "../Library/Common/StVariant.h"
#include "Programs/ProgramID.h"
#include "../GenericRule.h"
#include "JSEngine/JSEngine.h"

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

	NTSTATUS FromVariant(const StVariant& Rule) override;

	bool HasScript() const;
	CJSEnginePtr GetScriptEngine() const;

	bool IsInteractive() const
	{
		std::shared_lock Lock(m_Mutex);
		return m_bInteractive;
	}

protected:
	void WriteIVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const StVariant& Data) override;
	void ReadMValue(const SVarName& Name, const StVariant& Data) override;

	void UpdateScript_NoLock();

	EAccessRuleType m_Type = EAccessRuleType::eNone;
	bool m_bUseScript = false;
	std::string m_Script;
	bool m_bInteractive = false;
	std::wstring m_AccessPath; // can be pattern
	std::wstring m_ProgramPath; // can be pattern
	bool m_bVolumeRule = false;
	CJSEnginePtr m_pScript;
};

typedef std::shared_ptr<CAccessRule> CAccessRulePtr;
