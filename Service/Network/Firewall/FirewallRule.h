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

	void Update(const std::shared_ptr<struct SWindowsFwRule>& Rule);
	void Update(const std::shared_ptr<CFirewallRule>& pRule);

	std::wstring GetGuidStr() const;
	const CProgramID& GetProgramID()		{ return m_ProgramID; }
	std::wstring GetBinaryPath() const;

	bool IsTemplate() const					{ return m_ProgramID.GetType() == EProgramType::eFilePattern; }

	bool IsEnabled() const;

	bool IsTemporary() const				{ std::shared_lock Lock(m_Mutex); return m_bTemporary; }
	void SetTemporary(bool bTemporary)		{ std::unique_lock Lock(m_Mutex); m_bTemporary = bTemporary; }

	bool HasTimeOut() const					{ std::shared_lock Lock(m_Mutex); return m_uTimeOut != -1; }
	bool IsExpired() const;

	std::shared_ptr<struct SWindowsFwRule> GetData() { std::shared_lock Lock(m_Mutex); return m_Data; }

	void IncrHitCount() { std::unique_lock Lock(m_Mutex); m_HitCount++; }

	StVariant ToVariant(const SVarWriteOpt& Opts) const;
	bool FromVariant(const StVariant& Rule);

protected:
	mutable std::shared_mutex  m_Mutex;

	virtual void WriteIVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const StVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	CProgramID m_ProgramID;
	
	std::wstring m_BinaryPath;
	std::shared_ptr<struct SWindowsFwRule> m_Data;

	bool m_bTemporary = false;
	uint64 m_uTimeOut = -1;

	int m_HitCount = 0;
};

typedef std::shared_ptr<CFirewallRule> CFirewallRulePtr;