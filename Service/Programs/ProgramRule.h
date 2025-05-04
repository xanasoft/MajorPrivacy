#pragma once
#include "../Library/Common/StVariant.h"
#include "Programs/ProgramID.h"
#include "../GenericRule.h"
#include "../Library/API/PrivacyDefs.h"

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
	KPH_VERIFY_AUTHORITY GetSignatureLevel() const	{std::shared_lock Lock(m_Mutex); return m_SignatureLevel;}
	bool GetImageLoadProtection() const				{std::shared_lock Lock(m_Mutex); return m_ImageLoadProtection;}

	void Update(const std::shared_ptr<CProgramRule>& Rule);

protected:
	void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const StVariant& Data) override;
	void ReadMValue(const SVarName& Name, const StVariant& Data) override;

	EExecRuleType m_Type = EExecRuleType::eUnknown;
	std::wstring m_ProgramPath; // can be pattern
	KPH_VERIFY_AUTHORITY m_SignatureLevel = KPH_VERIFY_AUTHORITY::KphUntestedAuthority;
	bool m_ImageLoadProtection = true;
};

typedef std::shared_ptr<CProgramRule> CProgramRulePtr;
typedef std::weak_ptr<CProgramRule> CProgramRuleRef;
