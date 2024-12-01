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
	if (m_hCleanUpThread) {
		m_bCancelCleanUp = true;
		HANDLE hCleanUpThread = InterlockedCompareExchangePointer((PVOID*)&m_hCleanUpThread, NULL, m_hCleanUpThread);
		if (hCleanUpThread) {
			WaitForSingleObject(hCleanUpThread, INFINITE);
			NtClose(hCleanUpThread);
		}
	}

	delete m_pHandleList;
}

DWORD CALLBACK CAccessManager__LoadProc(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
	SetThreadDescription(GetCurrentThread(), L"CAccessManager__LoadProc");
#endif

	CAccessManager* This = (CAccessManager*)lpThreadParameter;

	uint64 uStart = GetTickCount64();
	STATUS Status = This->Load();
	DbgPrint(L"CAccessManager::Load() took %llu ms\n", GetTickCount64() - uStart);

	NtClose(This->m_hStoreThread);
	This->m_hStoreThread = NULL;
	return 0;
}

STATUS CAccessManager::Init()
{
	theCore->Driver()->RegisterRuleEventHandler(ERuleType::eAccess, &CAccessManager::OnRuleChanged, this);
	theCore->Driver()->RegisterForRuleEvents(ERuleType::eAccess);
	LoadRules();

	m_hStoreThread = CreateThread(NULL, 0, CAccessManager__LoadProc, (void*)this, 0, NULL);

	m_pHandleList->Init();

	return OK;
}

DWORD CALLBACK CAccessManager__StoreProc(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
	SetThreadDescription(GetCurrentThread(), L"CAccessManager__StoreProc");
#endif

	CAccessManager* This = (CAccessManager*)lpThreadParameter;

	uint64 uStart = GetTickCount64();
	STATUS Status = This->Store();
	DbgPrint(L"CAccessManager::Store() took %llu ms\n", GetTickCount64() - uStart);

	NtClose(This->m_hStoreThread);
	This->m_hStoreThread = NULL;
	return 0;
}

STATUS CAccessManager::StoreAsync()
{
	if (m_hStoreThread) return STATUS_ALREADY_COMMITTED;
	m_hStoreThread = CreateThread(NULL, 0, CAccessManager__StoreProc, (void*)this, 0, NULL);
	return STATUS_SUCCESS;
}

void CAccessManager::Update()
{
	if (m_UpdateAllRules)
		LoadRules();

	if(theCore->Config()->GetBool("Service", "EnumAllOpenFiles", false))
		m_pHandleList->Update();
}

DWORD CALLBACK CAccessManager__CleanUp(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
	SetThreadDescription(GetCurrentThread(), L"CAccessManager__CleanUp");
#endif

	CAccessManager* This = (CAccessManager*)lpThreadParameter;
	
	std::map<uint64, CProgramItemPtr> Items = theCore->ProgramManager()->GetItems();

	uint64 uTotalCounter = 0;
	for (auto pItem : Items)
	{
		if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
			uTotalCounter += pProgram->GetAccessCount();
		else if (CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem.second))
			uTotalCounter += pService->GetAccessCount();
	}

	uint64 uDoneCounter = 0;
	for (auto pItem : Items)
	{
		if(This->m_bCancelCleanUp)
			break;
		uint32 uCounter = 0;
		if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
			pProgram->CleanUpAccessTree(&This->m_bCancelCleanUp, &uCounter);
		else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem.second))
			pService->CleanUpAccessTree(&This->m_bCancelCleanUp, &uCounter);
		uDoneCounter += uCounter;

		//DbgPrint("CAccessManager__CleanUp completed %llu of %llu\r\n", uDoneCounter, uTotalCounter);

		CVariant Event;
		Event[API_V_PROGRESS_FINISHED] = false;
		Event[API_V_PROGRESS_TOTAL] = uTotalCounter;
		Event[API_V_PROGRESS_DONE] = uDoneCounter;
		theCore->BroadcastMessage(SVC_API_EVENT_CLEANUP_PROGRESS, Event);
	}

	if (!This->m_bCancelCleanUp) {
		CVariant Event;
		Event[API_V_PROGRESS_FINISHED] = true;
		theCore->BroadcastMessage(SVC_API_EVENT_CLEANUP_PROGRESS, Event);
	}

	HANDLE hCleanUpThread = InterlockedCompareExchangePointer((PVOID*)&This->m_hCleanUpThread, NULL, This->m_hCleanUpThread);
	if(hCleanUpThread)
		NtClose(hCleanUpThread);
	return 0;
}

STATUS CAccessManager::CleanUp()
{
	if(m_hCleanUpThread) return STATUS_DEVICE_BUSY;
	//m_bCancelCleanUp = false; // only true on destruction
	m_hCleanUpThread = CreateThread(NULL, 0, CAccessManager__CleanUp, (void *)this, 0, NULL);
	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
// Rules

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