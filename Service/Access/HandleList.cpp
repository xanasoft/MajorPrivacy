#include "pch.h"
#include "HandleList.h"
#include "../ServiceCore.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Programs/ProgramManager.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/Scoped.h"
#include "../Processes/ProcessList.h"


CHandleList::CHandleList()
{

}

CHandleList::~CHandleList()
{

}

STATUS CHandleList::Init()
{
	m_FileObjectTypeIndex = GetNtObjectTypeNumber(L"File");
	m_KeyObjectTypeIndex = GetNtObjectTypeNumber(L"Key");

	return OK;
}

//DWORD CALLBACK CHandleList__ThreadProc(LPVOID lpThreadParameter)
//{
//#ifdef _DEBUG
//SetThreadDescription(GetCurrentThread(), L"CHandleList__ThreadProc");
//#endif
// 
//	CHandleList* This = (CHandleList*)lpThreadParameter;
//	This->EnumAllHandlesAsync();
//	return 0;
//}

void CHandleList::Update()
{
	//if(m_hThread) return;
	//m_hThread = CreateThread(NULL, 0, CHandleList__ThreadProc, (void *)this, 0, NULL);

	if(!theCore->Driver()->IsConnected())
		return;
	EnumAllHandles();
}

//void CHandleList::EnumAllHandlesAsync()
//{
//	EnumAllHandles();
//	CloseHandle(m_hThread);
//	m_hThread = NULL;
//}

uint64 CHandleList__MakeID(uint64 HandleValue, uint64 UniqueProcessId)
{
	uint64 HandleID = HandleValue;
	HandleID ^= (UniqueProcessId << 32);
	HandleID ^= (UniqueProcessId >> 32);
	return HandleID;
}

STATUS CHandleList::EnumAllHandles()
{
#ifdef _DEBUG
	uint64 start = GetUSTickCount();
#endif

	std::vector<BYTE> Handles;
	NTSTATUS status = MyQuerySystemInformation(Handles, SystemExtendedHandleInformation);
	if (!NT_SUCCESS(status))
		return ERR(status);

	//std::map<ULONG_PTR, CScopedHandle<HANDLE, BOOL (WINAPI*)(HANDLE)>> HandlesMap;

	std::unique_lock Lock(m_Mutex);

	std::unordered_multimap<uint64, CHandlePtr> OldHandles = m_List;

	//Lock.unlock();

	PSYSTEM_HANDLE_INFORMATION_EX handleInfo = (PSYSTEM_HANDLE_INFORMATION_EX)Handles.data();
	for (int i = 0; i < handleInfo->NumberOfHandles; i++)
	{
		PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle = &handleInfo->Handles[i];

		// todo: also reg keys?
		if (handle->ObjectTypeIndex != m_FileObjectTypeIndex /*&& handle->ObjectTypeIndex != m_KeyObjectTypeIndex*/)
			continue;

		uint64 HandleID = CHandleList__MakeID(handle->HandleValue, handle->UniqueProcessId);

		auto F = OldHandles.find(HandleID);
		if(F != OldHandles.end()) {
			OldHandles.erase(F);
			continue; // handle already listed
		}

		//CScopedHandle<HANDLE, BOOL (WINAPI*)(HANDLE)> &hProcess = HandlesMap[handle->UniqueProcessId];
		//if (!hProcess) {
		//	hProcess.Set(OpenProcess(PROCESS_DUP_HANDLE, FALSE, (DWORD)handle->UniqueProcessId), CloseHandle);
		//	if (!hProcess)
		//		continue; // we cant inspect handles belonging to protected processes, todo: use driver to get handle info
		//}

		//std::wstring Name = GetHandleObjectName(hProcess, handle);
		auto Res = theCore->Driver()->GetHandleInfo(handle->UniqueProcessId, handle->HandleValue);
		std::wstring Name = !Res.IsError() ? Res.GetValue()->Path : L"";

		CHandlePtr pHandle = std::make_shared<CHandle>(Name);
		//Lock.lock();
		m_List.insert(std::make_pair(HandleID, pHandle));
		//Lock.unlock();
		
		CProcessPtr pProcess = theCore->ProcessList()->GetProcess(handle->UniqueProcessId, true); // Note: this will add the process and load some basic data if it does not already exist
		pHandle->LinkProcess(pProcess);
		pProcess->AddHandle(pHandle);
	}

	//Lock.lock();

	for(auto I = OldHandles.begin(); I != OldHandles.end(); ++I)
	{
		CHandlePtr pHandle = I->second;
		if (pHandle->CanBeRemoved())
		{
			if(CProcessPtr pProcess = pHandle->GetProcess())
				pProcess->RemoveHandle(pHandle);

			m_List.erase(I->first);
		}
		else if (!pHandle->IsMarkedForRemoval()) 
			pHandle->MarkForRemoval();
	}

	Lock.unlock();

#ifdef _DEBUG
	//DbgPrint("EnumAllHandles took %d ms cycles\r\n", (GetUSTickCount() - start)/1000);
#endif

	return OK;
}

std::set<CHandlePtr> CHandleList::FindHandles(const CProgramID& ID)
{
	if (ID.GetType() == EProgramType::eAllPrograms)
		return GetAllHandles();

	auto Processes = theCore->ProcessList()->FindProcesses(ID);

	std::shared_lock<std::shared_mutex> Lock(m_Mutex);

	std::set<CHandlePtr> Found;

	for (auto P : Processes)
	{
		auto pProcess = P.second;
		auto Handles = pProcess->GetHandleList();
		Found.merge(Handles);
	}

	return Found;
}

std::set<CHandlePtr> CHandleList::GetAllHandles()
{
	std::shared_lock<std::shared_mutex> Lock(m_Mutex);

	std::set<CHandlePtr> All;

	for (auto I : m_List) {
		if(I.second->GetFileName().empty())
			continue; // skip unnamed handles
		All.insert(I.second);
	}
	//std::transform(m_List->begin(), m_List->end(), std::inserter(*All), [](auto I) { return I.second; });
	return All;
}