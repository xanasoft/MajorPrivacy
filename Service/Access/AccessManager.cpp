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
#include "../Library/Common/FileIO.h"

#define API_ACCESS_LOG_FILE_NAME L"AccessLog.dat"
#define API_ACCESS_LOG_FILE_VERSION 1

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

	Load();

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

RESULT(std::wstring) CAccessManager::AddRule(const CAccessRulePtr& pRule)
{
	CVariant Request = pRule->ToVariant(SVarWriteOpt());
	auto Res = theCore->Driver()->Call(API_SET_ACCESS_RULE, Request);
	//if (Res.IsSuccess()) // will be done by the notification event
	//	UpdateRule(pRule, pRule->GetGuid());
	if(Res.IsError())
		return ERR(Res.GetStatus());
	RETURN(Res.GetValue().AsStr());
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

void CAccessManager::OnRuleChanged(const std::wstring& Guid, enum class ERuleEvent Event, enum class ERuleType Type, uint64 PID)
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

	theCore->VolumeManager()->OnRuleChanged(Guid, Event, Type, PID);

	CVariant vEvent;
	vEvent[API_V_RULE_GUID] = Guid;
	//vEvent[API_V_RULE_TYPE] = (uint32)ERuleType::eAccess;
	//vEvent[API_V_NAME] = ;
	vEvent[API_V_EVENT] = (uint32)Event;

	theCore->BroadcastMessage(SVC_API_EVENT_RES_RULE_CHANGED, vEvent);
}

//////////////////////////////////////////////////////////////////////////
// Load/Store

STATUS CAccessManager::Load()
{
	CBuffer Buffer;
	if (!ReadFile(theCore->GetDataFolder() + L"\\" API_ACCESS_LOG_FILE_NAME, 0, Buffer)) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_ACCESS_LOG_FILE_NAME L" not found");
		return ERR(STATUS_NOT_FOUND);
	}

	CVariant Data;
	//try {
	auto ret = Data.FromPacket(&Buffer, true);
	//} catch (const CException&) {
	//	return ERR(STATUS_UNSUCCESSFUL);
	//}
	if (ret != CVariant::eErrNone) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_ACCESS_LOG_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	if (Data[API_S_VERSION].To<uint32>() != API_ACCESS_LOG_FILE_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_ACCESS_LOG_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	CVariant List = Data[API_S_ACCESS_LOG];

	for (uint32 i = 0; i < List.Count(); i++)
	{
		CVariant Item = List[i];
	
		CProgramID ID;
		ID.FromVariant(Item.Find(API_V_PROG_ID));
		CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
		if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
			pProgram->LoadAccess(Item);
		else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
			pService->LoadAccess(Item);
	}

	return OK;
}

STATUS CAccessManager::Store()
{
	SVarWriteOpt Opts;
	Opts.Format = SVarWriteOpt::eIndex;
	Opts.Flags = SVarWriteOpt::eSaveToFile;

	CVariant List;
	for (auto pItem : theCore->ProgramManager()->GetItems()) 
	{
		// StoreAccess saves API_V_PROG_ID
		if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
			List.Append(pProgram->StoreAccess(Opts));
		else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem.second))
			List.Append(pService->StoreAccess(Opts));
	}

	CVariant Data;
	Data[API_S_VERSION] = API_ACCESS_LOG_FILE_VERSION;
	Data[API_S_ACCESS_LOG] = List;

	CBuffer Buffer;
	Data.ToPacket(&Buffer);
	WriteFile(theCore->GetDataFolder() + L"\\" API_ACCESS_LOG_FILE_NAME, 0, Buffer);

	return OK;
}