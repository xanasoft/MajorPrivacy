#pragma once
#include "../Library/Status.h"
#include "AccessRule.h"

class CAccessManager
{
	TRACK_OBJECT(CAccessManager)
public:
	CAccessManager();
	~CAccessManager();

	STATUS Init();

	STATUS Load();
	STATUS Store();
	STATUS StoreAsync();

	void Update();

	STATUS CleanUp();

	STATUS LoadRules();

	void PurgeExpired();

	class CHandleList*		HandleList() {return m_pHandleList;}

	std::map<CFlexGuid, CAccessRulePtr>	GetAllRules();
	CAccessRulePtr GetRule(const CFlexGuid& Guid);

	RESULT(std::wstring) AddRule(const CAccessRulePtr& pRule);
	STATUS RemoveRule(const CFlexGuid& Guid);

	std::map<CFlexGuid, CAccessRulePtr> GetAccessRules() { std::unique_lock lock(m_RulesMutex); return m_Rules; }

protected:

	class CHandleList*		m_pHandleList = NULL;

	//////////////////////////////////////////////////////////////////////////
	// Rules

	void UpdateRule(const CAccessRulePtr& pRule, const CFlexGuid& Guid);

	void AddRuleUnsafe(const CAccessRulePtr& pRule);
	void RemoveRuleUnsafe(const CAccessRulePtr& pRule);

	std::recursive_mutex					m_RulesMutex;

	bool									m_UpdateAllRules = true;
	std::map<CFlexGuid, CAccessRulePtr>	m_Rules;

	void OnRuleChanged(const CFlexGuid& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID);

	friend DWORD CALLBACK CAccessManager__CleanUp(LPVOID lpThreadParameter);
	bool					m_bCancelCleanUp = false;
	volatile HANDLE			m_hCleanUpThread = NULL;

	friend DWORD CALLBACK CAccessManager__LoadProc(LPVOID lpThreadParameter);
	friend DWORD CALLBACK CAccessManager__StoreProc(LPVOID lpThreadParameter);
	HANDLE					m_hStoreThread = NULL;

	uint64					m_LastRulePurge = 0;
};