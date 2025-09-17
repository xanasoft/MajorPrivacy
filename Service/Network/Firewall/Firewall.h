#pragma once
#include "../Library/Status.h"
#include "Network/Firewall/FirewallRule.h"
#include "Programs/ProgramFile.h"
#include "Programs/AppPackage.h"
#include "Programs/WindowsService.h"
#include "../Library/API/PrivacyDefs.h"
#include "../Library/Helpers/RegUtil.h"
#include "../Library/Common/FlexGuid.h"
#include "../Framework/Common/PathTree.h"

class CFwRuleTemplate : public CPathEntry
{
public:
	CFwRuleTemplate(FW::AbstractMemPool* pMem, const CFirewallRulePtr& pRule) : CPathEntry(pMem, FW::StringW(pMem, pRule->GetBinaryPath().c_str())) {
		m_pRule = pRule;
	}

	CFirewallRulePtr GetRule() const { return m_pRule; }

protected:
	CFirewallRulePtr m_pRule;
};

class CFwAutoGuardEntry : public CPathEntry
{
public:
	enum EMode {
		eNone = 0,
		eApprove,
		eReject,
	};

	CFwAutoGuardEntry(FW::AbstractMemPool* pMem, const std::wstring& Path, EMode Mode) : CPathEntry(pMem, FW::StringW(pMem, Path.c_str())) {
		m_Mode = Mode;
	}

	EMode GetMode() const { return m_Mode; }

protected:
	EMode m_Mode;
};



class CFirewall
{
	TRACK_OBJECT(CFirewall)
public:
	CFirewall();
	~CFirewall();

	STATUS Init();

	STATUS Load();
	STATUS Store();

	STATUS LoadRules(const StVariant& Entries);
	StVariant SaveRules(const SVarWriteOpt& Opts);

	void Update();

	STATUS UpdateRules();
	void UpdateDefaults();

	void UpdateAutoGuard() {m_bUpdateAutoGuard = true;}

	void PurgeExpired(bool All);

	void RevertChanges();

	std::map<CFlexGuid, CFirewallRulePtr> FindRules(const CProgramID& ID);

	std::map<CFlexGuid, CFirewallRulePtr> GetAllRules();

	STATUS SetRule(const CFirewallRulePtr& pRule);
	CFirewallRulePtr GetRule(const CFlexGuid& Guid);
	STATUS DelRule(const CFlexGuid& Guid);

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
	bool MatchProgramID(const std::shared_ptr<struct SWindowsFwRule>& pRule, const CFirewallRulePtr& pFwRule);

	CFirewallRulePtr UpdateFWRuleUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule, const CFlexGuid& Guid);

	void InitAutoGuard();
	bool FWRuleChangedUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule, const CFirewallRulePtr& pFwRule, bool* pExpected = nullptr);

	void AddRuleUnsafe(const CFirewallRulePtr& pFwRule);
	void AddRuleUnsafeImpl(const CFirewallRulePtr& pFwRule);
	STATUS UpdateRuleAndTest(const std::shared_ptr<struct SWindowsFwRule>& pData);
	void RemoveRuleUnsafe(const CFirewallRulePtr& pFwRule);
	void RemoveRuleUnsafeImpl(const CFirewallRulePtr& pFwRule);

	STATUS RestoreRule(const CFirewallRulePtr& pFwRuleBackup);

	void SetTempalte(const CFirewallRulePtr& pFwRule);
	bool TryRemoveTemplate(const CFlexGuid& Guid);

	uint32 OnFwGuardEvent(const struct SWinFwGuardEvent* pEvent);
	uint32 OnFwLogEvent(const struct SWinFwLogEvent* pEvent);

	void EmitChangeEvent(const CFlexGuid& Guid, const std::wstring& Name, enum class EConfigEvent Event, bool bExpected);

	void ProcessFwEvent(const struct SWinFwLogEvent* pEvent, class CSocket* pSocket);

	bool IsDefaultWindowsRule(const CFirewallRulePtr& pFwRule) const;
	bool IsWindowsStoreRule(const CFirewallRulePtr& pFwRule) const;

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
	std::map<CFlexGuid, CFirewallRulePtr> m_FwRules;
	FW::Map<CFlexGuid, FW::SharedPtr<CFwRuleTemplate>> m_FwRuleTemplates;
	CPathTree				m_FwTemplatesTree;

	std::unordered_multimap<std::wstring, CFirewallRulePtr>	m_FileRules;
	std::unordered_multimap<std::wstring, CFirewallRulePtr>	m_SvcRules;
	std::unordered_multimap<std::wstring, CFirewallRulePtr>	m_AppRules;
	//std::map<std::wstring, CFirewallRulePtr> m_AllProgramsRules;

	int						m_UpdateDefaultProfiles = 2;
	bool					m_BlockAllInbound[3] = { false, false, false };
	EFwActions				m_DefaultInboundAction[3] = { EFwActions::Undefined, EFwActions::Undefined, EFwActions::Undefined };
	EFwActions				m_DefaultoutboundAction[3] = { EFwActions::Undefined, EFwActions::Undefined, EFwActions::Undefined };

	uint64					m_LastRulePurge = 0;

	std::set<CFirewallRulePtr> m_RulesToRemove;
	std::set<CFirewallRulePtr> m_RulesToRevert;

	bool 					m_bUpdateAutoGuard = false;	
	CPathTree				m_AutoGuardTree;
};