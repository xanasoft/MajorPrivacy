#pragma once
#include "../Library/Common/StVariant.h"
#include "Programs/ProgramID.h"
#include "../GenericRule.h"
#include "../Library/API/PrivacyDefs.h"
#include "JSEngine/JSEngine.h"

class CProgramRule: public CGenericRule
{
public:
	CProgramRule(const CProgramID& ID);
	~CProgramRule();

	std::shared_ptr<CProgramRule> Clone(bool CloneGuid = false) const;

	std::wstring GetProgramPath() const				{std::shared_lock Lock(m_Mutex); return m_ProgramPath;}

	void SetType(EExecRuleType Type)				{std::unique_lock Lock(m_Mutex); m_Type = Type;}
	EExecRuleType GetType() const					{std::shared_lock Lock(m_Mutex); return m_Type;}
	bool IsBlock() const							{std::shared_lock Lock(m_Mutex); return m_Type == EExecRuleType::eBlock;}
	bool IsProtect() const							{std::shared_lock Lock(m_Mutex); return m_Type == EExecRuleType::eProtect;}
	USignatures GetAllowedSignatures() const {std::shared_lock Lock(m_Mutex); return m_AllowedSignatures;}
	std::list<std::wstring> GetAllowedCollections() const {std::shared_lock Lock(m_Mutex); return m_AllowedCollections;}
	bool GetImageLoadProtection() const				{std::shared_lock Lock(m_Mutex); return m_ImageLoadProtection;}
	bool GetImageCoherencyChecking() const			{std::shared_lock Lock(m_Mutex); return m_ImageCoherencyChecking;}

	void Update(const std::shared_ptr<CProgramRule>& Rule);

	NTSTATUS FromVariant(const StVariant& Rule) override;

	bool HasScript() const;
	CJSEnginePtr GetScriptEngine() const;

protected:
	void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const StVariant& Data) override;
	void ReadMValue(const SVarName& Name, const StVariant& Data) override;

	void UpdateScript_NoLock();

	EExecRuleType m_Type = EExecRuleType::eUnknown;
	bool m_bUseScript = false;
	std::string m_Script;
	std::wstring m_ProgramPath; // can be pattern
	USignatures m_AllowedSignatures = { 0 };
	std::list<std::wstring> m_AllowedCollections;
	bool m_ImageLoadProtection = true;
	bool m_ImageCoherencyChecking = true;

	CJSEnginePtr m_pScript;
};

typedef std::shared_ptr<CProgramRule> CProgramRulePtr;
typedef std::weak_ptr<CProgramRule> CProgramRuleRef;
