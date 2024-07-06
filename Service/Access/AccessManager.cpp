#include "pch.h"
#include "AccessManager.h"
#include "../ServiceCore.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Programs/ProgramManager.h"
#include "HandleList.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Volumes/VolumeManager.h"

CAccessManager::CAccessManager()
{
	m_pHandleList = new CHandleList();
}

CAccessManager::~CAccessManager()
{
	delete m_pHandleList;
}

STATUS CAccessManager::Init()
{
	theCore->Driver()->RegisterRuleEventHandler(ERuleType::eAccess, &CAccessManager::OnRuleChanged, this);
	theCore->Driver()->RegisterForRuleEvents(ERuleType::eAccess);
	LoadRules();

	m_pHandleList->Init();

	return OK;
}

void CAccessManager::Update()
{
	if (m_UpdateAllRules)
		LoadRules();

	if(theCore->Config()->GetBool("Service", "EnumAllOpenFiles", false))
		m_pHandleList->Update();
}

std::map<std::wstring, CAccessRulePtr> CAccessManager::GetAllRules()
{
	std::unique_lock Lock(m_RulesMutex);
	return m_Rules;
}

CAccessRulePtr CAccessManager::GetRule(const std::wstring& Guid)
{
	std::unique_lock Lock(m_RulesMutex);
	auto F = m_Rules.find(Guid);
	if (F != m_Rules.end())
		return F->second;
	return nullptr;
}

STATUS CAccessManager::AddRule(const CAccessRulePtr& pRule)
{
	CVariant Request = pRule->ToVariant(SVarWriteOpt());
	auto Res = theCore->Driver()->Call(API_SET_ACCESS_RULE, Request);
	//if (Res.IsSuccess()) // will be done by the notification event
	//	UpdateRule(pRule, pRule->GetGuid());
	return Res;
}

STATUS CAccessManager::RemoveRule(const std::wstring& Guid)
{
	CVariant Request;
	Request[API_V_RULE_GUID] = Guid;
	auto Res = theCore->Driver()->Call(API_DEL_ACCESS_RULE, Request);
	//if (Res.IsSuccess()) // will be done by the notification event
	//	UpdateRule(nullptr, Guid);
	return Res;
}

//////////////////////////////////////////////////////////////////////////
// Rules

STATUS CAccessManager::LoadRules()
{
	CVariant Request;
	auto Res = theCore->Driver()->Call(API_GET_ACCESS_RULES, Request);
	if(Res.IsError())
		return Res;

	std::unique_lock lock(m_RulesMutex);

	std::map<std::wstring, CAccessRulePtr> OldRules = m_Rules;

	CVariant Rules = Res.GetValue();
	for(uint32 i=0; i < Rules.Count(); i++)
	{
		CVariant Rule = Rules[i];

		std::wstring Guid = Rule[API_V_RULE_GUID].AsStr();

		std::wstring ProgramPath = NormalizeFilePath(Rule[API_V_FILE_PATH].AsStr());

		CProgramID ID;
		ID.SetPath(ProgramPath);

		CAccessRulePtr pRule;
		auto I = OldRules.find(Guid);
		if (I != OldRules.end())
		{
			pRule = I->second;
			OldRules.erase(I);
			if (pRule->GetProgramPath() != ProgramPath) // todo
			{
				RemoveRuleUnsafe(I->second);
				pRule.reset();
			}
		}

		if (!pRule) 
		{
			pRule = std::make_shared<CAccessRule>(ID);
			pRule->FromVariant(Rule);
			AddRuleUnsafe(pRule);
		}
		else
			pRule->FromVariant(Rule);
	}

	for (auto I : OldRules)
		RemoveRuleUnsafe(I.second);

	m_UpdateAllRules = false;
	return OK;
}

void CAccessManager::AddRuleUnsafe(const CAccessRulePtr& pRule)
{
	m_Rules.insert(std::make_pair(pRule->GetGuid(), pRule));

	theCore->ProgramManager()->AddResRule(pRule);
}

void CAccessManager::RemoveRuleUnsafe(const CAccessRulePtr& pRule)
{
	m_Rules.erase(pRule->GetGuid());

	theCore->ProgramManager()->RemoveResRule(pRule);
}

void CAccessManager::UpdateRule(const CAccessRulePtr& pRule, const std::wstring& Guid)
{
	std::unique_lock Lock(m_RulesMutex);

	CAccessRulePtr pOldRule;
	if (!Guid.empty()) {
		auto F = m_Rules.find(Guid);
		if (F != m_Rules.end())
			pOldRule = F->second;
	}

	if (pRule && pOldRule && pRule->GetProgramPath() == pOldRule->GetProgramPath()) // todo
		pOldRule->Update(pRule);
	else {  // when the rule changes the programs it applyes to we remove it and tehn add it
		if (pOldRule) RemoveRuleUnsafe(pOldRule);
		if (pRule) AddRuleUnsafe(pRule);
	}
}

void CAccessManager::OnRuleChanged(const std::wstring& Guid, enum class ERuleEvent Event, enum class ERuleType Type)
{
	ASSERT(Type == ERuleType::eAccess);

	if(Event == ERuleEvent::eAllChanged) {
		LoadRules();
		return;
	}

	CAccessRulePtr pRule;
	if (Event != ERuleEvent::eRemoved) {
		CVariant Request;
		Request[API_V_RULE_GUID] = Guid;
		auto Res = theCore->Driver()->Call(API_GET_ACCESS_RULE, Request);
		if (Res.IsSuccess()) {
			CVariant Rule = Res.GetValue();
			CProgramID ID;
			ID.SetPath(Rule[API_V_FILE_PATH].AsStr());
			pRule = std::make_shared<CAccessRule>(ID);
			pRule->FromVariant(Rule);
		}
	}

	UpdateRule(pRule, Guid);

	theCore->VolumeManager()->OnRuleChanged(Guid, Event, Type);

	CVariant vEvent;
	vEvent[API_V_RULE_GUID] = Guid;
	//vEvent[API_V_RULE_TYPE] = (uint32)ERuleType::eAccess;
	//vEvent[API_V_NAME] = ;
	vEvent[API_V_EVENT] = (uint32)Event;

	theCore->BroadcastMessage(SVC_API_EVENT_RES_RULE_CHANGED, vEvent);
}