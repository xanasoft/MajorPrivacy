#pragma once
#include "../Library/Status.h"
#include "Network/Firewall/FirewallRule.h"
#include "Programs/ProgramFile.h"
#include "Programs/AppPackage.h"
#include "Programs/WindowsService.h"
#include "../Library/Helpers/RegUtil.h"
#include "../Library/Common/FlexGuid.h"

class CFirewall
{
	TRACK_OBJECT(CFirewall)
public:
	CFirewall();
	~CFirewall();

	STATUS Init();

	void Update();

	STATUS LoadRules();
	void LoadDefaults();

	void PurgeExpired(bool All);

	std::map<std::wstring, CFirewallRulePtr> FindRules(const CProgramID& ID);

	std::map<std::wstring, CFirewallRulePtr> GetAllRules();

	STATUS SetRule(const CFirewallRulePtr& pRule);
	CFirewallRulePtr GetRule(const std::wstring& RuleId);
	STATUS DelRule(const std::wstring& RuleId);

	STATUS SetFilteringMode(FwFilteringModes Mode);
	FwFilteringModes GetFilteringMode();

	STATUS SetAuditPolicy(FwAuditPolicy Mode);
	FwAuditPolicy GetAuditPolicy(bool bCurrent = true);

	static bool IsEmptyOrStar(const std::vector<std::wstring>& value);
	static bool MatchPort(uint16 Port, const std::vector<std::wstring>& Ports);
    static bool MatchAddress(const CAddress& Address, const std::vector<std::wstring>& Addresses, std::shared_ptr<struct SAdapterInfo> pNicInfo = std::shared_ptr<struct SAdapterInfo>());
	static bool MatchEndpoint(const std::vector<std::wstring>& Addresses, const std::vector<std::wstring>& Ports, const CAddress& Address, uint16 Port, std::shared_ptr<struct SAdapterInfo> pNicInfo = std::shared_ptr<struct SAdapterInfo>());
	static std::vector<std::wstring> GetSpecialNet(std::wstring SubNet, std::shared_ptr<struct SAdapterInfo> pNicInfo = std::shared_ptr<struct SAdapterInfo>());

protected:
	bool MatchRuleID(const std::shared_ptr<struct SWindowsFwRule>& pRule, const CFirewallRulePtr& pFwRule);

	CFirewallRulePtr UpdateFWRule(const std::shared_ptr<struct SWindowsFwRule>& pRule, const std::wstring& RuleId);

	void AddRuleUnsafe(const CFirewallRulePtr& pFwRule);
	void RemoveRuleUnsafe(const CFirewallRulePtr& pFwRule);

	uint32 OnFwGuardEvent(const struct SWinFwGuardEvent* pEvent);
	uint32 OnFwLogEvent(const struct SWinFwLogEvent* pEvent);

	void ProcessFwEvent(const struct SWinFwLogEvent* pEvent, class CSocket* pSocket);

	struct SRuleMatch
	{
		EFwActions Result = EFwActions::Undefined;
        int BlockCount = 0;
        int AllowCount = 0;
		//std::list<CFirewallRulePtr> AllowRules;
		//std::list<CFirewallRulePtr> BlockRules;
	};

	static SRuleMatch MatchRulesWithEvent(const std::set<CFirewallRulePtr>& Rules, const struct SWinFwLogEvent* pEvent, std::vector<CFlexGuid>& AllowRules, std::vector<CFlexGuid>& BlockRules, std::shared_ptr<struct SAdapterInfo> pNicInfo = std::shared_ptr<struct SAdapterInfo>());

	EFwActions GetDefaultAction(EFwDirections Direction, EFwProfiles Profiles);

	void LoadFwLog();

	class CWindowsFwLog*	m_pLog = NULL;
	class CWindowsFwGuard*	m_pGuard = NULL;

	std::recursive_mutex	m_RulesMutex;

	CRegWatcher				m_RegWatcher;

	bool					m_UpdateAllRules = true;
	std::map<std::wstring, CFirewallRulePtr> m_FwRules;

	std::unordered_multimap<std::wstring, CFirewallRulePtr>	m_FileRules;
	std::unordered_multimap<std::wstring, CFirewallRulePtr>	m_SvcRules;
	std::unordered_multimap<std::wstring, CFirewallRulePtr>	m_AppRules;
	//std::map<std::wstring, CFirewallRulePtr> m_AllProgramsRules;

	bool					m_UpdateDefaultProfiles = true;
	bool					m_BlockAllInbound[3] = { false, false, false };
	EFwActions				m_DefaultInboundAction[3] = { EFwActions::Undefined, EFwActions::Undefined, EFwActions::Undefined };
	EFwActions				m_DefaultoutboundAction[3] = { EFwActions::Undefined, EFwActions::Undefined, EFwActions::Undefined };

	uint64					m_LastRulePurge = 0;
};