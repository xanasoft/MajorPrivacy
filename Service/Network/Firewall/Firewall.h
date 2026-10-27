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

	STATUS ReLoad();
	STATUS Store();

	STATUS LoadRules(const StVariant& Entries);
	StVariant SaveRules(const SVarWriteOpt& Opts);

	void Update();

	STATUS UpdateRules();
	void UpdateDefaults();

	void UpdateAutoGuard() {m_bUpdateAutoGuard = true;}

	void PurgeExpired(bool All);

	void RevertChanges();

	void CleanupOldTemplateRules();

	std::map<CFlexGuid, CFirewallRulePtr> FindRules(const CProgramID& ID);

	std::map<CFlexGuid, CFirewallRulePtr> GetAllRules();

	std::vector<CFirewallRulePtr> GetRulesFromTemplate(const CFlexGuid& TemplateGuid);

	//
	// What the existing rule templates have to say about a rule the user has not seen yet. Path
	// alone is not enough, the rule also has to lie inside what the template reaches in terms of
	// direction, protocol, profile, interface and endpoints before its action is taken as an answer.
	//
	enum class ETemplateVerdict
	{
		eUnknown = 0,	// no template has anything to say about this rule
		eApprove,		// a template of the same action covers it, that decision is already made
		eReject,		// a block template covers it while the rule would allow, against what was set
	};
	ETemplateVerdict AskTemplatesUnsafe(const std::wstring& DosPath, const std::shared_ptr<struct SWindowsFwRule>& pRule, FW::SharedPtr<CFwRuleTemplate>* ppTemplate = nullptr);
	static bool TemplateAppliesToRule(const std::shared_ptr<struct SWindowsFwRule>& pTemplate, const std::shared_ptr<struct SWindowsFwRule>& pRule);
	static bool ListCoversList(const std::vector<std::wstring>& Template, const std::vector<std::wstring>& Rule, bool bPorts);

	STATUS SetRule(const CFirewallRulePtr& pRule, const std::vector<std::wstring>& TakeOver = std::vector<std::wstring>()); // TakeOver: for a template, existing rules it is to stand for from now on
	CFirewallRulePtr GetRule(const CFlexGuid& Guid);
	STATUS DelRule(const CFlexGuid& Guid, bool bWithDerived = false); // bWithDerived: for a template, the rules made from it go as well

	STATUS SetFilteringMode(FwFilteringModes Mode);
	FwFilteringModes GetFilteringMode();

	STATUS SetAuditPolicy(FwAuditPolicy Mode);
	FwAuditPolicy GetAuditPolicy(bool bCurrent = true);

	static bool IsEmptyOrStar(const std::vector<std::wstring>& value);
	static bool MatchPort(uint16 Port, const std::vector<std::wstring>& Ports);
	static bool MatchAddress(const CAddress& Address, const std::vector<std::wstring>& Addresses, std::shared_ptr<struct SAdapterInfo> pNicInfo = std::shared_ptr<struct SAdapterInfo>());
	static bool MatchEndpoint(const std::vector<std::wstring>& Addresses, const std::vector<std::wstring>& Ports, const CAddress& Address, uint16 Port, std::shared_ptr<struct SAdapterInfo> pNicInfo = std::shared_ptr<struct SAdapterInfo>());
	//static bool MatchUserAuthorization(const std::wstring& Sddl, const std::wstring& UserSid, const std::map<std::wstring, SGroupInfo>& UserGroups);
	static bool MatchUserAuthorization(const CFirewallRulePtr& pRule, const std::wstring& UserSid, const std::map<std::wstring, SGroupInfo>& UserGroups);
	static std::vector<std::wstring> GetSpecialNet(std::wstring SubNet, std::shared_ptr<struct SAdapterInfo> pNicInfo = std::shared_ptr<struct SAdapterInfo>());

	STATUS RestoreDefaultFwRules();

protected:
	STATUS Load();

	bool MatchProgramID(const std::shared_ptr<struct SWindowsFwRule>& pRule, const CFirewallRulePtr& pFwRule);

	CFirewallRulePtr UpdateFWRuleUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule, const CFlexGuid& Guid);

	void InitAutoGuard();
	bool FWRuleChangedUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule, const CFirewallRulePtr& pFwRule, bool* pExpected = nullptr);
	CFwAutoGuardEntry::EMode DecideRuleChangeUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule, const CFirewallRulePtr& pFwRule, bool bProgramChanged, const std::wstring& OldPath, const std::wstring& NewPath);

	void AddRuleUnsafe(const CFirewallRulePtr& pFwRule);
	void AddRuleUnsafeImpl(const CFirewallRulePtr& pFwRule);
	STATUS UpdateRuleAndTest(const std::shared_ptr<struct SWindowsFwRule>& pData);
	void RemoveRuleUnsafe(const CFirewallRulePtr& pFwRule);
	void RemoveRuleUnsafeImpl(const CFirewallRulePtr& pFwRule);

	STATUS RestoreRule(const CFirewallRulePtr& pFwRuleBackup, bool bApprove = true);
	void ClearBackups(const std::wstring& Guid);

	void SetTempalte(const CFirewallRulePtr& pFwRule, const std::vector<std::wstring>& TakeOver);
	bool TryRemoveTemplate(const CFlexGuid& Guid, bool bWithDerived);

	// what happened to a rule while the lock was held, reported once it is let go
	struct SRuleChange
	{
		std::wstring		Guid;
		std::wstring		Name;
		EConfigEvent		Event;
	};

	static std::shared_ptr<struct SWindowsFwRule> MakeDerivedRuleData(const std::shared_ptr<struct SWindowsFwRule>& pTemplate, const std::wstring& BinaryPath, EFwDirections Direction);
	std::vector<CFirewallRulePtr> GetDerivedRulesUnsafe(const std::wstring& TemplateGuid);
	bool RemoveDerivedRuleUnsafe(const CFirewallRulePtr& pRule);
	void SyncDerivedRulesUnsafe(const CFirewallRulePtr& pTemplateRule, std::vector<SRuleChange>& Changes);

	uint32 OnFwGuardEvent(const struct SWinFwGuardEvent* pEvent);
	uint32 OnFwLogEvent(const struct SWinFwLogEvent* pEvent);

	void EmitChangeEvent(const CFlexGuid& Guid, const std::wstring& Name, enum class EConfigEvent Event, bool bExpected, bool bNoConfig = false);

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

	static SRuleMatch MatchRulesWithEvent(const std::set<CFirewallRulePtr>& Rules, const struct SWinFwLogEvent* pEvent, const CProcessPtr& pProcess, std::vector<CFlexGuid>& AllowRules, std::vector<CFlexGuid>& BlockRules, std::shared_ptr<struct SAdapterInfo> pNicInfo = std::shared_ptr<struct SAdapterInfo>());

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

	//
	// A rule which has been taken away and not asked about yet. Programs are fond of dropping a
	// rule only to put the very same one back an instant later, so the question is held back for
	// a moment in case it turns out not to need asking at all.
	//
	struct SWithdrawnRule
	{
		CFirewallRulePtr	pBackup;
		uint64				uWithdrawn = 0;
		bool				bRevert = false; // the blanket guard turns it down if nothing comes back, instead of asking
	};
	std::vector<SWithdrawnRule> m_WithdrawnRules;

	// the other way round: a rule which turned up identical to one still standing, waiting for that one to go
	struct SHeldRule
	{
		CFirewallRulePtr	pRule;
		uint64				uHeld = 0;
		bool				bReject = false; // the blanket guard turns it down if the original stays, instead of asking
	};
	std::vector<SHeldRule> m_HeldRules;

	// a removal let through at once, on other grounds than a twin, kept in mind in case it is the first half of a replacement
	struct SSettledRemoval
	{
		CFirewallRulePtr	pRule;
		uint64				uRemoved = 0;
	};
	std::vector<SSettledRemoval> m_SettledRemovals;

	CFirewallRulePtr		TakeSettledTwinUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule);

	bool					TakeHeldRuleUnsafe(const CFirewallRulePtr& pRule);
	CFirewallRulePtr		FindLiveTwinUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pComing);
	void					ReleaseHeldRules();

	CFirewallRulePtr		TakeWithdrawnTwinUnsafe(const std::shared_ptr<struct SWindowsFwRule>& pRule);
	CFirewallRulePtr		FindLiveTwinUnsafe(const CFirewallRulePtr& pGoing);
	void					AskAboutWithdrawnRules();

	bool 					m_bUpdateAutoGuard = false;	
	CPathTree				m_AutoGuardTree;
};