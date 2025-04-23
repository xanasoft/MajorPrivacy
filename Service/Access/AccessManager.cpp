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

#define API_ACCESS_RECORD_FILE_NAME L"AccessRecord.dat"
#define API_ACCESS_RECORD_FILE_VERSION 1

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
	theCore->Driver()->RegisterConfigEventHandler(EConfigGroup::eAccessRules, &CAccessManager::OnRuleChanged, this);
	theCore->Driver()->RegisterForConfigEvents(EConfigGroup::eAccessRules);
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

	if (theCore->Config()->GetBool("Service", "EnumAllOpenFiles", false))
	{
#if DEF_CORE_TIMER_INTERVAL < 1000
		static int UpdateDivider = 0;
		if (++UpdateDivider % (1000/DEF_CORE_TIMER_INTERVAL) == 0)
#endif
		m_pHandleList->Update();
	}

	// purge rules once per minute
	if(m_LastRulePurge + 60*1000 < GetTickCount64())
		PurgeExpired();
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

		StVariant Event;
		Event[API_V_PROGRESS_FINISHED] = false;
		Event[API_V_PROGRESS_TOTAL] = uTotalCounter;
		Event[API_V_PROGRESS_DONE] = uDoneCounter;
		theCore->BroadcastMessage(SVC_API_EVENT_CLEANUP_PROGRESS, Event);
	}

	if (!This->m_bCancelCleanUp) {
		StVariant Event;
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

std::map<CFlexGuid, CAccessRulePtr> CAccessManager::GetAllRules()
{
	std::unique_lock Lock(m_RulesMutex);
	return m_Rules;
}

CAccessRulePtr CAccessManager::GetRule(const CFlexGuid& Guid)
{
	std::unique_lock Lock(m_RulesMutex);
	auto F = m_Rules.find(Guid);
	if (F != m_Rules.end())
		return F->second;
	return nullptr;
}

RESULT(std::wstring) CAccessManager::AddRule(const CAccessRulePtr& pRule)
{
	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

	StVariant Request = pRule->ToVariant(Opts);
	auto Res = theCore->Driver()->Call(API_SET_ACCESS_RULE, Request);
	//if (Res.IsSuccess()) // will be done by the notification event
	//	UpdateRule(pRule, pRule->GetGuid());
	if(Res.IsError())
		return ERR(Res.GetStatus());
	RETURN(Res.GetValue().AsStr());
}

STATUS CAccessManager::RemoveRule(const CFlexGuid& Guid)
{
	StVariant Request;
	Request[API_V_GUID] = Guid.ToVariant(true);
	auto Res = theCore->Driver()->Call(API_DEL_ACCESS_RULE, Request);
	//if (Res.IsSuccess()) // will be done by the notification event
	//	UpdateRule(nullptr, Guid);
	return Res;
}

STATUS CAccessManager::LoadRules()
{
	StVariant Request;

	auto Res = theCore->Driver()->Call(API_GET_ACCESS_RULES, Request);
	if(Res.IsError())
		return Res;

	std::unique_lock lock(m_RulesMutex);

	std::map<CFlexGuid, CAccessRulePtr> OldRules = m_Rules;

	StVariant Rules = Res.GetValue();
	for(uint32 i=0; i < Rules.Count(); i++)
	{
		StVariant Rule = Rules[i];

		CFlexGuid Guid;
		Guid.FromVariant(Rule[API_V_GUID]);
		std::wstring ProgramPath = theCore->NormalizePath(Rule[API_V_FILE_PATH].AsStr());
		CProgramID ID(ProgramPath);

		CAccessRulePtr pRule;
		auto I = OldRules.find(Guid);
		if (I != OldRules.end())
		{
			pRule = I->second;
			OldRules.erase(I);
			if (theCore->NormalizePath(pRule->GetProgramPath()) != ProgramPath)
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

void CAccessManager::PurgeExpired()
{
	std::unique_lock Lock(m_RulesMutex);
	for (auto I = m_Rules.begin(); I != m_Rules.end(); ) {
		auto J = I++;
		if (J->second->IsExpired()) {
			theCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, 1, StrLine(L"Removed expired Resouce Access Rule: %s", J->second->GetName().c_str()).c_str());
			RemoveRule(J->first);
		}
	}

	m_LastRulePurge = GetTickCount64();
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

void CAccessManager::UpdateRule(const CAccessRulePtr& pRule, const CFlexGuid& Guid)
{
	std::unique_lock Lock(m_RulesMutex);

	CAccessRulePtr pOldRule;
	if (!Guid.IsNull()) {
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

void CAccessManager::OnRuleChanged(const CFlexGuid& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID)
{
	ASSERT(Type == EConfigGroup::eAccessRules);

	if(Event == EConfigEvent::eAllChanged)
		LoadRules();
	else
	{
		CAccessRulePtr pRule;
		if (Event != EConfigEvent::eRemoved) {
			StVariant Request;
			Request[API_V_GUID] = Guid.ToVariant(true);
			auto Res = theCore->Driver()->Call(API_GET_ACCESS_RULE, Request);
			if (Res.IsSuccess()) {
				StVariant Rule = Res.GetValue();
				std::wstring ProgramPath = theCore->NormalizePath(Rule[API_V_FILE_PATH].AsStr());
				CProgramID ID(ProgramPath);
				pRule = std::make_shared<CAccessRule>(ID);
				pRule->FromVariant(Rule);
			}
		}

		CAccessRulePtr pRule2 = pRule;
		if (Event == EConfigEvent::eRemoved)
			pRule2 = GetRule(Guid);
		if (pRule2)
			theCore->VolumeManager()->UpdateRule(pRule2, Event, PID);

		UpdateRule(pRule, Guid);
	}

	StVariant vEvent;
	vEvent[API_V_GUID] = Guid.ToVariant(false);
	//vEvent[API_V_NAME] = ;
	vEvent[API_V_EVENT_TYPE] = (uint32)Event;

	theCore->BroadcastMessage(SVC_API_EVENT_RES_RULE_CHANGED, vEvent);
}

//////////////////////////////////////////////////////////////////////////
// Load/Store

STATUS CAccessManager::Load()
{
	CBuffer Buffer;
	if (!ReadFile(theCore->GetDataFolder() + L"\\" API_ACCESS_RECORD_FILE_NAME, Buffer)) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_ACCESS_RECORD_FILE_NAME L" not found");
		return ERR(STATUS_NOT_FOUND);
	}

	StVariant Data;
	//try {
	auto ret = Data.FromPacket(&Buffer, true);
	//} catch (const CException&) {
	//	return ERR(STATUS_UNSUCCESSFUL);
	//}
	if (ret != StVariant::eErrNone) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_ACCESS_RECORD_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	if (Data[API_S_VERSION].To<uint32>() != API_ACCESS_RECORD_FILE_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_ACCESS_RECORD_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	StVariant List = Data[API_S_ACCESS_LOG];

	for (uint32 i = 0; i < List.Count(); i++)
	{
		StVariant Item = List[i];
	
		CProgramID ID;
		if(!ID.FromVariant(StVariantReader(Item).Find(API_V_PROG_ID)))
			continue;
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

	StVariantWriter List;
	List.BeginList();

	bool bSave = theCore->Config()->GetBool("Service", "SaveAccessRecord", false);

	for (auto pItem : theCore->ProgramManager()->GetItems())
	{
		ESavePreset ePreset = pItem.second->GetSaveTrace();
		if (ePreset == ESavePreset::eDontSave || (ePreset == ESavePreset::eDefault && !bSave))
			continue;

		// StoreAccess saves API_V_PROG_ID
		if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
			List.WriteVariant(pProgram->StoreAccess(Opts));
		else if (CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem.second))
			List.WriteVariant(pService->StoreAccess(Opts));	
	}

	StVariant Data;
	Data[API_S_VERSION] = API_ACCESS_RECORD_FILE_VERSION;
	Data[API_S_ACCESS_LOG] = List.Finish();

	CBuffer Buffer;
	Data.ToPacket(&Buffer);
	WriteFile(theCore->GetDataFolder() + L"\\" API_ACCESS_RECORD_FILE_NAME, Buffer);

	return OK;
}