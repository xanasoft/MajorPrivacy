#pragma once
#include "../Library/Common/StVariant.h"
#include "Programs/ProgramID.h"

class CFirewallRule
{
	TRACK_OBJECT(CFirewallRule)
public:
	CFirewallRule();
	CFirewallRule(const std::shared_ptr<struct SWindowsFwRule>& Rule);
	~CFirewallRule();

	bool Match(const std::shared_ptr<struct SWindowsFwRule>& Rule) const;
	static bool Match(const std::shared_ptr<struct SWindowsFwRule>& RuleL, const std::shared_ptr<struct SWindowsFwRule>& RuleR);

	void Update(const std::shared_ptr<struct SWindowsFwRule>& Rule);
	void Update(const std::shared_ptr<CFirewallRule>& pRule);

	std::wstring GetGuidStr() const;

	std::wstring GetName() const;
	
	void SetApproved(bool bApproved)		{ std::unique_lock Lock(m_Mutex); m_State = bApproved ? EFwRuleState::eApproved : EFwRuleState::eUnapproved; }
	bool IsApproved() const					{ std::shared_lock Lock(m_Mutex); return m_State == EFwRuleState::eApproved; }
	void SetAsBackup(bool bRemoved, bool bProgramChanged);
	bool IsBackup() const					{ std::shared_lock Lock(m_Mutex); return m_State == EFwRuleState::eBackup; }
	void SetDiverged(bool bDiverged)		{ std::unique_lock Lock(m_Mutex); m_State = bDiverged ? EFwRuleState::eDiverged : EFwRuleState::eApproved; }
	bool IsDiverged() const					{ std::shared_lock Lock(m_Mutex); return m_State == EFwRuleState::eDiverged; }

	std::wstring GetOriginalGuid() const	{ std::shared_lock Lock(m_Mutex); return m_OriginalGuid; }

	const CProgramID& GetProgramID()		{ std::shared_lock Lock(m_Mutex); return m_ProgramID; }
	std::wstring GetBinaryPath() const;

	void SetSource(EFwRuleSource Source)	{ std::unique_lock Lock(m_Mutex); m_Source = Source; }
	EFwRuleSource GetSource() const			{ std::shared_lock Lock(m_Mutex); return m_Source; }

	bool IsTemplate() const					{ std::shared_lock Lock(m_Mutex); return m_ProgramID.GetType() == EProgramType::eFilePattern; }

	void SetEnabled(bool bEnabled);
	bool IsEnabled() const;

	bool IsTemporary() const				{ std::shared_lock Lock(m_Mutex); return m_bTemporary; }
	void SetTemporary(bool bTemporary)		{ std::unique_lock Lock(m_Mutex); m_bTemporary = bTemporary; }

	bool HasTimeOut() const					{ std::shared_lock Lock(m_Mutex); return m_uTimeOut != -1; }
	bool IsExpired() const;

	std::shared_ptr<struct SWindowsFwRule> GetFwRule() { std::shared_lock Lock(m_Mutex); return m_FwRule; }

	void IncrHitCount() { std::unique_lock Lock(m_Mutex); m_HitCount++; }

	void IncrSetErrorCount() { std::unique_lock Lock(m_Mutex); m_SetErrorCount++; }
	void ClearSetErrorCount() { std::unique_lock Lock(m_Mutex); m_SetErrorCount = 0; }
	int GetSetErrorCount() const { std::shared_lock Lock(m_Mutex);  return m_SetErrorCount; }

	StVariant ToVariant(const SVarWriteOpt& Opts) const;
	bool FromVariant(const StVariant& Rule);

protected:
	friend class CFirewall;
	mutable std::shared_mutex  m_Mutex;

	virtual void WriteIVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const StVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	CProgramID m_ProgramID;
	
	std::wstring m_BinaryPath;
	std::shared_ptr<struct SWindowsFwRule> m_FwRule;

	EFwRuleState m_State = EFwRuleState::eUnapproved;
	std::wstring m_OriginalGuid;

	EFwRuleSource m_Source = EFwRuleSource::eUnknown;

	bool m_bTemporary = false;
	uint64 m_uTimeOut = -1;

	int m_HitCount = 0;
	int m_SetErrorCount = 0;
};

typedef std::shared_ptr<CFirewallRule> CFirewallRulePtr;