#pragma once
#include "../Library/Common/Variant.h"
#include "Programs/ProgramID.h"

class CFirewallRule
{
public:
	CFirewallRule();
	CFirewallRule(const std::shared_ptr<struct SWindowsFwRule>& Rule);
	~CFirewallRule();

	void Update(const std::shared_ptr<struct SWindowsFwRule>& Rule);

	std::wstring GetGuid() const;
	const CProgramID& GetProgramID() { return m_ProgramID; }

	std::shared_ptr<struct SWindowsFwRule> GetData() { std::shared_lock Lock(m_Mutex); return m_Data; }

	void IncrHitCount() { std::unique_lock Lock(m_Mutex); m_HitCount++; }

	CVariant ToVariant() const;
	bool FromVariant(const CVariant& FwRule);

protected:
	mutable std::shared_mutex  m_Mutex;

	CProgramID m_ProgramID;
	
	std::shared_ptr<struct SWindowsFwRule> m_Data;

	int m_HitCount = 0;
};

typedef std::shared_ptr<CFirewallRule> CFirewallRulePtr;