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

	void Update();

	STATUS LoadRules();

	class CHandleList*		HandleList() {return m_pHandleList;}

	std::map<std::wstring, CAccessRulePtr>	GetAllRules();
	CAccessRulePtr GetRule(const std::wstring& Guid);

	RESULT(std::wstring) AddRule(const CAccessRulePtr& pRule);
	STATUS RemoveRule(const std::wstring& Guid);

	std::map<std::wstring, CAccessRulePtr> GetAccessRules() { std::unique_lock lock(m_RulesMutex); return m_Rules; }

protected:

	class CHandleList*		m_pHandleList = NULL;

	//////////////////////////////////////////////////////////////////////////
	// Rules

	void UpdateRule(const CAccessRulePtr& pRule, const std::wstring& Guid);

	void AddRuleUnsafe(const CAccessRulePtr& pRule);
	void RemoveRuleUnsafe(const CAccessRulePtr& pRule);

	std::recursive_mutex					m_RulesMutex;

	bool									m_UpdateAllRules = true;
	std::map<std::wstring, CAccessRulePtr>	m_Rules;

	void OnRuleChanged(const std::wstring& Guid, enum class ERuleEvent Event, enum class ERuleType Type, uint64 PID);
};