#include "pch.h"
#include "ProgramManager.h"
#include "../ServiceCore.h"
#include "../Processes/ProcessList.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/NtIo.h"
#include "../Library/Helpers/NtObj.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Network/NetworkManager.h"
#include "../Network/Firewall/Firewall.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Common/Exception.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/EvtUtil.h"

#define API_PROGRAMS_FILE_NAME L"Programs.dat"
#define API_PROGRAMS_FILE_VERSION 2

CProgramManager::CProgramManager()
{
	m_InstallationList = new CInstallationList;
	m_PackageList = new CPackageList;

	m_LastTruncateLogs = GetTickCount64();
}

CProgramManager::~CProgramManager()
{
	if (m_hTruncateLogsThread) {
		m_bCancelTruncateLogs = true;
		HANDLE hTruncateLogsThread = InterlockedCompareExchangePointer((PVOID*)&m_hTruncateLogsThread, NULL, m_hTruncateLogsThread);
		if (hTruncateLogsThread) {
			WaitForSingleObject(hTruncateLogsThread, INFINITE);
			NtClose(hTruncateLogsThread);
		}
	}

	delete m_InstallationList;
	delete m_PackageList;

#ifdef _DEBUG
	m_Root.reset();
	m_pAll.reset();
	m_NtOsKernel.reset();

	m_PatternMap.clear();
	m_PackageMap.clear();
	m_ServiceMap.clear();
	m_InstallMap.clear();
	m_PathMap.clear();
	
	//m_Items.clear();
	for (auto I = m_Items.begin(); I != m_Items.end(); ++I) {
		CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(I->second);
		if (pProgram)
			pProgram->ClearLogs(ETraceLogs::eLogMax);
		CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(I->second);
		if (pService)
			pService->ClearLogs(ETraceLogs::eLogMax);
	}

	for (auto I = m_Items.begin(); I != m_Items.end(); ++I) {
		I->second->m_FwRules.clear();
		I->second->m_ProgRules.clear();
		I->second->m_ResRules.clear();
	}

	for (auto I = m_Items.begin(); I != m_Items.end(); ++I) {
		CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(I->second);
		if(pProgram)
			pProgram->m_Processes.clear();
	}

	for (auto I = m_Items.begin(); I != m_Items.end(); ++I) {
		CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(I->second);
		if (pService)
			pService->m_pProcess.reset();
	}

	for (auto I = m_Items.begin(); I != m_Items.end();) {
		CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(I->second);
		CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(I->second);
		if(pProgram || pService)
			++I; 
		else
			I = m_Items.erase(I);
	}

	for (auto I = m_Items.begin(); I != m_Items.end(); ) {
		CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(I->second);
		if (pProgram)
			I = m_Items.erase(I);
		else
			++I;
	}

	for (auto I = m_Items.begin(); I != m_Items.end(); ) {
		CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(I->second);
		if (pService)
			I = m_Items.erase(I);
		else
			++I;
	}

	m_LibraryMap.clear();
	m_Libraries.clear();
#endif
}

STATUS CProgramManager::Init()
{
	m_Root = CProgramListPtr(new CProgramRoot()); 
	m_Root->m_UID = 1;
	m_Items.insert(std::make_pair(m_Root->GetUID(), m_Root));

	m_pAll = CProgramSetPtr(new CAllPrograms());
	m_pAll->SetName(L"All Programs");
	AddItemToRoot(m_pAll);
	m_Items.insert(std::make_pair(m_pAll->GetUID(), m_pAll));

	m_NtOsKernel = CProgramFilePtr(new CProgramFile(theCore->NormalizePath(CProcess::NtOsKernel_exe, false)));
	m_NtOsKernel->SetName(L"System (NT OS Kernel)");
	m_PathMap[MkLower(m_NtOsKernel->GetPath())] = m_NtOsKernel;
	//AddItemToRoot(m_NtOsKernel);
	m_Items.insert(std::make_pair(m_NtOsKernel->GetUID(), m_NtOsKernel));

	WCHAR windir[MAX_PATH + 8];
    GetWindowsDirectoryW(windir, MAX_PATH);
	m_OsDrive = std::wstring(windir, 3);
	m_WinDir = windir;
	m_ProgDir = GetProgramFilesDirectory();

	uint64 uStart = GetTickCount64();
	STATUS Status = Load();
	DbgPrint(L"CProgramManager::Load() took %llu ms\n", GetTickCount64() - uStart);
	if (!Status)
	{
		AddItemToRoot(m_NtOsKernel);

		//AddPattern(GetApplicationDirectory() + L"\\*", L"Major Privacy");

		AddPattern(m_WinDir + L"\\*", L"Windows");
		//AddPattern(m_WinDir + L"\\System32\\svchost.exe", L"Windows Service Host");
		AddPattern(m_WinDir + L"\\System32\\*", L"System32");
		AddPattern(m_WinDir + L"\\SysWOW64\\*", L"System32 (x86)");
		AddPattern(m_WinDir + L"\\SystemApps*", L"System Apps");
		AddPattern(m_ProgDir + L"\\*", L"Program Files");
		AddPattern(m_ProgDir + L" (x86)\\*", L"Program Files (x86)");
		AddPattern(m_ProgDir + L"\\WindowsApps*", L"User Apps");
		AddPattern(m_ProgDir + L" (x86)\\Microsoft\\Edge*", L"Microsoft Edge");
		AddPattern(m_OsDrive + L"ProgramData\\*" + L"\\*", L"ProgramData");
		AddPattern(m_OsDrive + L"Users\\*" + L"\\*", L"Users");

		AddPattern(L"\\\\*", L"Network");
	}
	else
	{
		std::map<CProgramLibraryPtr, std::list<CProgramFilePtr>> LibraryMap;
		for (auto& IItem : m_Items)
		{
			CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(IItem.second);
			if (pProgram)
			{
				if (pProgram->HashInfoUnknown())
				{
					theCore->ThreadPool()->enqueueTask([](const CProgramFilePtr& pProgram) {
						SVerifierInfo VerifyInfo;
						CProcess::MakeVerifyInfo(pProgram->GetPath(), VerifyInfo);
						pProgram->UpdateSignInfo(&VerifyInfo, true);
					}, pProgram);
				}

				for (auto& ILibrary : pProgram->GetLibraries())
				{
					if (ILibrary.second.SignInfo.GetHashStatus() == EHashStatus::eHashUnknown)
					{
						CProgramLibraryPtr pLibrary = GetLibrary(ILibrary.first);
						if(pLibrary)
							LibraryMap[pLibrary].push_back(pProgram);
					}
				}
			}
		}

		for (auto& ILibrary : LibraryMap)
		{
			theCore->ThreadPool()->enqueueTask([](const CProgramLibraryPtr& pLibrary, const std::list<CProgramFilePtr>& Programs) {
				SVerifierInfo VerifyInfo;
				CProcess::MakeVerifyInfo(pLibrary->GetPath(), VerifyInfo);
				for (auto& pProgram : Programs)
					pProgram->AddLibrary(CFlexGuid(), pLibrary, 0, &VerifyInfo);
			}, ILibrary.first, ILibrary.second);
		}
	}

	uStart = GetTickCount64();
	m_InstallationList->Init();
	DbgPrint(L"CProgramManager::Init() took %llu ms\n", GetTickCount64() - uStart);

	uStart = GetTickCount64();
	m_PackageList->Init();
	DbgPrint(L"CProgramManager::Init() took %llu ms\n", GetTickCount64() - uStart);

	theCore->Driver()->RegisterConfigEventHandler(EConfigGroup::eProgramRules, &CProgramManager::OnRuleChanged, this);
	theCore->Driver()->RegisterForConfigEvents(EConfigGroup::eProgramRules);
	uStart = GetTickCount64();
	LoadRules();
	DbgPrint(L"CProgramManager::LoadRules() took %llu ms\n", GetTickCount64() - uStart);

	uStart = GetTickCount64();
	TestMissing();
	DbgPrint(L"CProgramManager::TestMissing() took %llu ms\n", GetTickCount64() - uStart);

	return OK;
}

void CProgramManager::Update()
{
	if (m_UpdateAllRules)
		LoadRules();

	if(m_LastTruncateLogs + 15*60*1000 < GetTickCount64()) { // every 15 minutes
		m_LastTruncateLogs = GetTickCount64();
		TruncateLogs();

		TestMissing();
	}
}

bool CProgramManager::IsNtOsKrnl(const std::wstring& FilePath) const
{
	return m_NtOsKernel->GetID().GetFilePath() == theCore->NormalizePath(FilePath);
}

void CProgramManager::TestMissing()
{
	auto Items = GetItems();

	for (auto I = Items.begin(); I != Items.end(); ++I)
	{
		CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(I->second);
		if (pProgram)
			pProgram->TestMissing();
	}
}

STATUS CProgramManager::CleanUp(bool bPurgeRules)
{
	CollectSoftware();

	auto Items = GetItems();

	for (auto I = Items.begin(); I != Items.end(); ++I)
	{
		CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(I->second);
		if (pProgram) 
			pProgram->TestMissing();

		if(I->second->IsMissing())
		{
			if (I->second->HasFwRules() || I->second->HasProgRules() || I->second->HasResRules()) {
				if (!bPurgeRules)
					continue;
			}

			// don't remove program files when thay have still registered services
			if (pProgram) {
				std::unique_lock lock(pProgram->m_Mutex);
				bool bContinue = false;
				for (auto pNode : pProgram->m_Nodes) {
					if (!pNode->IsMissing()) {
						bContinue = true;
						continue;
					}
				}
				if (bContinue)
					continue;
			}

			StVariant Data;
			Data[API_V_ID] = I->second->GetID().ToVariant(SVarWriteOpt());
			Data[API_V_NAME] = I->second->GetNameEx();
			theCore->EmitEvent(ELogLevels::eInfo, eLogProgramCleanedUp, Data);

			theCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_PROG_CLEANUP, StrLine(L"Removed no longer existing program: %s", I->second->GetNameEx().c_str()));
			RemoveProgramFrom(I->first, 0, bPurgeRules);
		}
	}

	return OK;
}

STATUS CProgramManager::RemoveAllRules(const CProgramItemPtr& pItem)
{
	auto FwRules = pItem->GetFwRules();
	for (auto I = FwRules.begin(); I != FwRules.end(); ++I) {
		STATUS Status = theCore->NetworkManager()->Firewall()->DelRule((*I)->GetGuidStr());
		if(!Status) return Status;
	}

	auto ProgRules = pItem->GetProgRules();
	for (auto I = ProgRules.begin(); I != ProgRules.end(); ++I) {
		StVariant Request;
		Request[API_V_GUID] = (*I)->GetGuid().ToVariant(true);
		STATUS Status = theCore->Driver()->Call(API_DEL_PROGRAM_RULE, Request);
		if (!Status) return Status;
	}

	auto ResRules = pItem->GetResRules();
	for (auto I = ResRules.begin(); I != ResRules.end(); ++I) {
		StVariant Request;
		Request[API_V_GUID] = (*I)->GetGuid().ToVariant(true);
		STATUS Status = theCore->Driver()->Call(API_DEL_ACCESS_RULE, Request);
		if (!Status) return Status;
	}

	return OK;
}

void CProgramManager::CollectSoftware()
{
	// Note Service List gets updated regularly

	for (auto I = m_InstallMap.begin(); I != m_InstallMap.end(); ++I) {
		if (I->second->m_IsMissing == CProgramItem::eUnknown)
			I->second->m_IsMissing = CProgramItem::eMissing;
	}
	m_InstallationList->Update();

	for (auto I = m_PackageMap.begin(); I != m_PackageMap.end(); ++I) {
		if (I->second->m_IsMissing == CProgramItem::eUnknown)
			I->second->m_IsMissing = CProgramItem::eMissing;
	}
	m_PackageList->Update();
}

STATUS CProgramManager::ReGroup()
{
	CollectSoftware();

	std::unique_lock lock(m_Mutex); 

	for (auto I = m_Items.begin(); I != m_Items.end(); ++I)
	{
		CProgramItemPtr pItem = I->second;
		if(std::dynamic_pointer_cast<CWindowsService>(pItem))
			continue;

		// Detach item from all parent groups
		auto Groups = pItem->GetGroups();
		for (auto I: Groups) {
			CProgramSetPtr pGroup = I.second.lock();
			// Remove from all auto associated groups
			if (pGroup && !std::dynamic_pointer_cast<CProgramGroup>(pGroup))
				RemoveProgramFromGroup(pItem, pGroup);
		}

		// drop item into the root, this will re-attach it to the correct groups
		AddItemToRoot(pItem);
	}

	return OK;
}

DWORD CALLBACK CProgramManager__TruncateLogs(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
	SetThreadDescription(GetCurrentThread(), L"CProgramManager__TruncateLogs");
#endif

	CProgramManager* This = (CProgramManager*)lpThreadParameter;

	std::map<uint64, CProgramItemPtr> Items = This->GetItems();

	uint64 uStart = GetTickCount64();
	for (auto pItem : Items) {
		CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second);
		if (pProgram) {
			pProgram->TruncateTraceLog();
			pProgram->TruncateAccessLog();
			pProgram->TruncateAccessTree();
		}
		else if (CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem.second)) {
			pService->TruncateAccessLog();
			pService->TruncateAccessTree();
		}
	}
	DbgPrint(L"CProgramManager__TruncateLogs took %llu ms\n", GetTickCount64() - uStart);

	HANDLE hTruncateLogsThread = InterlockedCompareExchangePointer((PVOID*)&This->m_hTruncateLogsThread, NULL, This->m_hTruncateLogsThread);
	if(hTruncateLogsThread)
		NtClose(hTruncateLogsThread);
	return 0;
}

STATUS CProgramManager::TruncateLogs()
{
	if(m_hTruncateLogsThread) return STATUS_DEVICE_BUSY;
	//m_bCancelTruncateLogs = false; // only true on destruction
	m_hTruncateLogsThread = CreateThread(NULL, 0, CProgramManager__TruncateLogs, (void *)this, 0, NULL);
	return STATUS_SUCCESS;
}

CProgramItemPtr CProgramManager::GetItem(uint64 UID) 
{ 
	std::unique_lock lock(m_Mutex); 
	auto F = m_Items.find(UID);
	if (F == m_Items.end())
		return nullptr;
	return F->second;
}

bool CProgramManager::IsPathReserved(std::wstring FileName) const
{
	FileName = theCore->NormalizePath(FileName, false);
	if(FileName.empty())
		return false;
	if (FileName.at(FileName.length() - 1) != L'\\')
		FileName.append(L"\\");
	if (_wcsicmp(FileName.c_str(), (m_WinDir + L"\\").c_str()) == 0)			// "C:\\Windows\\"
		return true;
	if (_wcsicmp(FileName.c_str(), (m_WinDir + L"\\System32\\").c_str()) == 0)	// "C:\\Windows\\System32\\"
		return true;
	if(_wcsicmp(FileName.c_str(), (m_ProgDir + L"\\").c_str()) == 0)			// "C:\\Program Files\\"
		return true;
	if(_wcsicmp(FileName.c_str(), (m_ProgDir + L" (x86)\\").c_str()) == 0)		// "C:\\Program Files (x86)\\"
		return true;
	return false;
}

void CProgramManager::AddPattern(const std::wstring& DosPattern, const std::wstring& Name)
{
	CProgramPatternPtr pPattern = CProgramPatternPtr(new CProgramPattern(DosPattern));
	pPattern->SetName(Name);

	std::unique_lock lock(m_Mutex);
	m_PatternMap.insert(std::make_pair(MkLower(pPattern->GetPattern()), pPattern));
	m_Items.insert(std::make_pair(pPattern->GetUID(), pPattern));
	
	AddItemToRoot(pPattern);
}

CProgramItemPtr CProgramManager::GetProgramByID(const CProgramID& ID, bool bCanAdd)
{
	switch (ID.GetType())
	{
	case EProgramType::eProgramFile:		return GetProgramFile(ID.GetFilePath(), bCanAdd);
	case EProgramType::eFilePattern:		return GetPattern(ID.GetFilePath(), bCanAdd);
	case EProgramType::eAppInstallation:	return GetInstallation(ID.GetRegKey(), bCanAdd);
	case EProgramType::eWindowsService:		return GetService(ID.GetServiceTag(), bCanAdd);
	case EProgramType::eAppPackage:			return GetAppPackage(ID.GetAppContainerSid(), bCanAdd);
	case EProgramType::eProgramGroup:		return GetGroup(ID.GetGuid(), bCanAdd);
	default:
#ifdef _DEBUG
		DebugBreak();
#endif
	case EProgramType::eAllPrograms:		return m_pAll;
	}
}

CProgramGroupPtr CProgramManager::GetGroup(const std::wstring& Guid, bool bCanAdd)
{
	std::wstring Key = MkLower(Guid);

	std::unique_lock lock(m_Mutex);
	if (!bCanAdd) {
		auto F = m_GroupMap.find(Key);
		if (F != m_GroupMap.end())
			return F->second;
		return NULL;
	}
	CProgramGroupPtr& pGroup = m_GroupMap[Key];
	if (!pGroup) {
		pGroup = CProgramGroupPtr(new CProgramGroup(Guid));
		m_Items.insert(std::make_pair(pGroup->GetUID(), pGroup));
		lock.unlock();

		AddItemToRoot(pGroup);

		//BroadcastItemChanged(pGroup, EConfigEvent::eAdded);
	}
	return pGroup;
}

CAppPackagePtr CProgramManager::GetAppPackage(const std::wstring& AppContainserSid, bool bCanAdd)
{
	std::wstring Key = MkLower(AppContainserSid);

	std::unique_lock lock(m_Mutex);
	if (!bCanAdd) {
		auto F = m_PackageMap.find(Key);
		if (F != m_PackageMap.end())
			return F->second;
		return NULL;
	}
	CAppPackagePtr& pAppPackage = m_PackageMap[Key];
	if (!pAppPackage) {
		pAppPackage = CAppPackagePtr(new CAppPackage(AppContainserSid));
		m_Items.insert(std::make_pair(pAppPackage->GetUID(), pAppPackage));
		//pAppPackage->SetInstallPath(GetAppContainerRootPath( // todo:
		// todo: icon
		lock.unlock();

		AddItemToRoot(pAppPackage);

		//BroadcastItemChanged(pAppPackage, EConfigEvent::eAdded);
	}
	return pAppPackage;
}

CWindowsServicePtr CProgramManager::GetService(const std::wstring& ServiceTag, bool bCanAdd)
{
	std::wstring Key = MkLower(ServiceTag);

	std::unique_lock lock(m_Mutex);
	if (!bCanAdd) {
		auto F = m_ServiceMap.find(Key);
		if (F != m_ServiceMap.end())
			return F->second;
		return NULL;
	}
	CWindowsServicePtr& pService = m_ServiceMap[Key];
	if (!pService) {
		pService = CWindowsServicePtr(new CWindowsService(ServiceTag));
		m_Items.insert(std::make_pair(pService->GetUID(), pService));
		lock.unlock();

		CServiceList::SServicePtr pWinService = theCore->ProcessList()->Services()->GetService(Key);
		CProgramSetPtr pParent;
		if (pWinService)
		{
			pService->SetName(pWinService->Name);
			pParent = GetProgramFile(pWinService->BinaryPath);
		}
		else
		{
			//
			// in case of an service which no longer exist we asign it to the all processes item
			// this can occur when loading rules for non existing items
			//

			pService->SetName(ServiceTag);
			pParent = m_pAll; 
		}

		AddProgramToGroup(pService, pParent);

		//BroadcastItemChanged(pService, EConfigEvent::eAdded);
	}
	return pService;
}

CProgramPatternPtr CProgramManager::GetPattern(const std::wstring& DosPattern, bool bCanAdd)
{
	std::wstring Key = MkLower(DosPattern);

	std::unique_lock lock(m_Mutex);
	
	auto F = m_PatternMap.find(Key);
	if (F != m_PatternMap.end())
		return F->second;
	if(!bCanAdd)
		return NULL;
	
	CProgramPatternPtr pPattern = CProgramPatternPtr(new CProgramPattern(DosPattern));
	pPattern->SetName(DosPattern);

	m_PatternMap.insert(std::make_pair(Key, pPattern));
	m_Items.insert(std::make_pair(pPattern->GetUID(), pPattern));
	lock.unlock();

	AddItemToRoot(pPattern);
	
	//BroadcastItemChanged(pPattern, EConfigEvent::eAdded);

	return pPattern;
}

CAppInstallationPtr	CProgramManager::GetInstallation(const std::wstring& RegKey, bool bCanAdd)
{
	std::wstring Key = MkLower(RegKey);

	std::unique_lock lock(m_Mutex);
	if (!bCanAdd) {
		auto F = m_InstallMap.find(Key);
		if (F != m_InstallMap.end())
			return F->second;
		return NULL;
	}
	CAppInstallationPtr& pAppInstall = m_InstallMap[Key];
	if (!pAppInstall) {
		pAppInstall = CAppInstallationPtr(new CAppInstallation(RegKey));
		m_Items.insert(std::make_pair(pAppInstall->GetUID(), pAppInstall));
		lock.unlock();

		AddItemToRoot(pAppInstall);

		//BroadcastItemChanged(pAppInstall, EConfigEvent::eAdded);
	}
	return pAppInstall;

}

CProgramFilePtr CProgramManager::GetProgramFile(const std::wstring& FileName, bool bCanAdd)
{
	std::wstring DosFileName = theCore->NormalizePath(FileName, false);
	std::wstring Key = MkLower(DosFileName);

	std::unique_lock lock(m_Mutex);
	if (!bCanAdd) {
		auto F = m_PathMap.find(Key);
		if (F != m_PathMap.end())
			return F->second;
		return NULL;
	}
	CProgramFilePtr& pProgram = m_PathMap[Key];
	if (!pProgram) {
		pProgram = CProgramFilePtr(new CProgramFile(DosFileName));
		//pProgram->SetName(GetFileNameFromPath(FileName));
		m_Items.insert(std::make_pair(pProgram->GetUID(), pProgram));
		lock.unlock();

		AddItemToRoot(pProgram);

		//BroadcastItemChanged(pProgram, EConfigEvent::eAdded);
	}
	return pProgram;
}

CProgramFilePtr CProgramManager::AddProcess(const CProcessPtr& pProcess)
{
	std::set<std::wstring> Services = pProcess->GetServices();
	if (!Services.empty()) {
		for (auto ServiceTag : Services) {
			CWindowsServicePtr pService = GetService(ServiceTag);
			pService->SetProcess(pProcess);
		}
	}

	std::wstring DosFileName = theCore->NormalizePath(pProcess->GetNtFilePath(), false);
	std::wstring Key = MkLower(DosFileName);

	std::unique_lock lock(m_Mutex);
	CProgramFilePtr& pProgram = m_PathMap[Key];
	if (!pProgram) {
		pProgram = CProgramFilePtr(new CProgramFile(DosFileName));
		//pProgram->SetName(pProcess->GetName());
		// todo: icon, description
		m_Items.insert(std::make_pair(pProgram->GetUID(), pProgram));
		lock.unlock();

		if (!pProcess->GetAppContainerSid().empty())
			AddProgramToGroup(pProgram, GetAppPackage(pProcess->GetAppContainerSid()));
		AddItemToRoot(pProgram);

		//BroadcastItemChanged(pProgram, EConfigEvent::eAdded);
	}
	else
		lock.unlock();

	pProgram->AddProcess(pProcess);
	
	pProcess->m_Mutex.lock();
	pProcess->m_pFileRef = pProgram;
	pProcess->m_Mutex.unlock();

	return pProgram;
}

void CProgramManager::RemoveProcess(const CProcessPtr& pProcess)
{
	std::set<std::wstring> Services = pProcess->GetServices();
	if (!Services.empty()) {
		for(auto ServiceTag: Services) {
			CWindowsServicePtr pService = GetService(ServiceTag, false);
			if (pService) pService->SetProcess(NULL);
		}
	}
	
	pProcess->m_Mutex.lock();
	CProgramFilePtr pProgram = pProcess->m_pFileRef.lock();
	pProcess->m_pFileRef.reset();
	pProcess->m_Mutex.unlock();

	if (pProgram)
		pProgram->RemoveProcess(pProcess);
}

void CProgramManager::AddService(const CServiceList::SServicePtr& pWinService)
{
	std::wstring Key = MkLower(pWinService->ServiceTag);

	std::unique_lock lock(m_Mutex);
	CWindowsServicePtr& pService = m_ServiceMap[Key];
	if (!pService) {
		pService = CWindowsServicePtr(new CWindowsService(pWinService->ServiceTag));
		pService->SetName(pWinService->Name);
		m_Items.insert(std::make_pair(pService->GetUID(), pService));
		lock.unlock();

		CProgramSetPtr pParent = GetProgramFile(pWinService->BinaryPath);
		AddProgramToGroup(pService, pParent);

		//BroadcastItemChanged(pService, EConfigEvent::eAdded);
	}
	else
	{
		lock.unlock();

		CProgramFilePtr pOldParent = pService->GetProgramFile();
		CProgramFilePtr pParent = GetProgramFile(pWinService->BinaryPath);
		if (pOldParent != pParent)
		{
			if (pOldParent) RemoveProgramFromGroup(pService, pOldParent);
			AddProgramToGroup(pService, pParent);
		}
	}
	pService->SetMissing(false);
}

void CProgramManager::RemoveService(const CServiceList::SServicePtr& pWinService)
{
	std::wstring Key = MkLower(pWinService->ServiceTag);

	std::unique_lock lock(m_Mutex);
	auto F = m_ServiceMap.find(Key);
	if (F != m_ServiceMap.end())
		F->second->SetMissing(true);
}

void CProgramManager::AddPackage(const CPackageList::SPackagePtr& pPackage)
{
	std::wstring Key = MkLower(pPackage->PackageSid);

	std::unique_lock lock(m_Mutex);
	CAppPackagePtr& pAppPackage = m_PackageMap[Key];
	if (!pAppPackage) {
		pAppPackage = CAppPackagePtr(new CAppPackage(pPackage->PackageSid, pPackage->PackageName));
		pAppPackage->SetName(pPackage->PackageDisplayName);
		pAppPackage->SetPath(theCore->NormalizePath(pPackage->PackageInstallPath, false));
		pAppPackage->SetIcon(pPackage->SmallLogoPath);
		m_Items.insert(std::make_pair(pAppPackage->GetUID(), pAppPackage));
		lock.unlock();

		AddItemToRoot(pAppPackage);

		//BroadcastItemChanged(pAppPackage, EConfigEvent::eAdded);
	}
	else
		pAppPackage->SetMissing(false);
}

void CProgramManager::RemovePackage(const CPackageList::SPackagePtr& pPackage)
{
	std::wstring Key = MkLower(pPackage->PackageSid);

	std::unique_lock lock(m_Mutex);
	auto F = m_PackageMap.find(Key);
	if (F != m_PackageMap.end())
		F->second->SetMissing(true);
}

void CProgramManager::AddInstallation(const CInstallationList::SInstallationPtr& pInstalledApp)
{
	std::wstring Key = MkLower(pInstalledApp->RegKey);

	std::unique_lock lock(m_Mutex);
	CAppInstallationPtr& pAppInstall = m_InstallMap[Key];
	if (!pAppInstall) {
		pAppInstall = CAppInstallationPtr(new CAppInstallation(pInstalledApp->RegKey));
		pAppInstall->SetName(pInstalledApp->DisplayName);
		pAppInstall->SetPath(theCore->NormalizePath(pInstalledApp->InstallPath, false));
		pAppInstall->SetIcon(pInstalledApp->DisplayIcon);
		m_Items.insert(std::make_pair(pAppInstall->GetUID(), pAppInstall));
		lock.unlock();

		AddItemToRoot(pAppInstall);

		//BroadcastItemChanged(pAppInstall, EConfigEvent::eAdded);
	}
	else
		pAppInstall->SetMissing(false);
}

void CProgramManager::RemoveInstallation(const CInstallationList::SInstallationPtr& pInstalledApp)
{
	std::wstring Key = MkLower(pInstalledApp->RegKey);

	std::unique_lock lock(m_Mutex);
	auto F = m_InstallMap.find(Key);
	if (F != m_InstallMap.end())
		F->second->SetMissing(true);
}

RESULT(CProgramItemPtr) CProgramManager::CreateProgram(const CProgramID& ID)
{
	switch (ID.GetType())
	{
	case EProgramType::eProgramGroup: 
	case EProgramType::eProgramFile:
	case EProgramType::eFilePattern:
		if(GetProgramByID(ID, false))
			return ERR(STATUS_ERR_DUPLICATE_PROG);
		break;
	default:
		// other types are not allowed to be created by the user
		return ERR(STATUS_INVALID_PARAMETER);
	}
	CProgramItemPtr pItem = GetProgramByID(ID, true);
	RETURN(pItem);
}

STATUS CProgramManager::AddProgramTo(uint64 UID, uint64 ParentUID)
{
	std::unique_lock lock(m_Mutex);

	auto F = m_Items.find(UID);
	if (F == m_Items.end())
		return ERR(STATUS_ERR_PROG_NOT_FOUND);
	CProgramItemPtr pItem = F->second;

	if(pItem == m_Root || pItem == m_pAll)
		return ERR(STATUS_ERR_PROG_PARENT_NOT_VALID);

	auto P = m_Items.find(ParentUID);
	if (P == m_Items.end())
		return ERR(STATUS_ERR_PROG_PARENT_NOT_FOUND);
	CProgramSetPtr pParent = std::dynamic_pointer_cast<CProgramSet>(P->second);
	if (!pParent || pParent == m_pAll || std::dynamic_pointer_cast<CProgramFile>(pParent)) // adding to file items is not permitted
		return ERR(STATUS_ERR_PROG_PARENT_NOT_VALID);

	// todo: add handling for groupe that have a pattern aprrent !!!

	// add the item to the new group
	if (!AddProgramToGroup(pItem, pParent))
		return ERR(STATUS_ERR_PROG_ALREADY_IN_GROUP);

	// remove teh ittem from root, if its already removed it will be a no-op
	RemoveProgramFromGroup(pItem, m_Root);

	return OK;
}

STATUS CProgramManager::RemoveProgramFrom(uint64 UID, uint64 ParentUID, bool bDelRules)
{
	CProgramItemPtr pItem = GetItem(UID);
	if(!pItem)
		return ERR(STATUS_ERR_PROG_NOT_FOUND);

	if (pItem->GetGroupCount() == 1) // removing from all groups or from last group
	{
		if (pItem->HasFwRules() || pItem->HasProgRules() || pItem->HasResRules())
		{
			if(!bDelRules)
				return ERR(STATUS_ERR_PROG_HAS_RULES);

			STATUS Status = RemoveAllRules(pItem);
			if (!Status) 
				return ERR(STATUS_ERR_PROG_HAS_RULES); // failed to remove rules	
		}
	}

	const CProgramID& ID = pItem->GetID();

	// do not allow to remove auto items
	switch (ID.GetType())
	{
		case EProgramType::eAppInstallation: 
			if(!theCore->Config()->GetBool("Service", "EnumInstallations", true))
				break; // allow removing installs when install enum is off
		case EProgramType::eWindowsService: 
		case EProgramType::eAppPackage: 
			if(pItem->IsMissing())
				break;
		case EProgramType::eAllPrograms:
			return ERR(STATUS_ERR_CANT_REMOVE_AUTO_ITEM);
	}

	// Program Files can host services, dont remove files with services
	// except if all services are missing
	CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem);
	if (pProgram) {
		std::unique_lock lock(pProgram->m_Mutex);
		for (auto pNode : pProgram->m_Nodes) {
			if(!pNode->IsMissing())
				return ERR(STATUS_ERR_CANT_REMOVE_AUTO_ITEM);
		}
	}

	if (ParentUID) {
		auto P = m_Items.find(ParentUID);
		if (P == m_Items.end())
			return ERR(STATUS_ERR_PROG_PARENT_NOT_FOUND);

		if(std::dynamic_pointer_cast<CProgramPattern>(P->second) && pItem->GetGroupCount() > 1)
			return ERR(STATUS_ERR_CANT_REMOVE_FROM_PATTERN); // Can't remove from a pattern unless we are removing it entierly

		CProgramSetPtr pParent = std::dynamic_pointer_cast<CProgramSet>(P->second);
		if (!pParent || !RemoveProgramFromGroupEx(pItem, pParent))
			return ERR(STATUS_ERR_PROG_PARENT_NOT_VALID);

		// Don't really remove the item if it still belogs to some other group
		if (pItem->GetGroupCount() > 0)
			return OK; 
	}
	else
	{
		for(;;) {
			std::unique_lock lock(pItem->m_Mutex);
			auto B = pItem->m_Groups.begin();
			if(B == pItem->m_Groups.end())
				break;
			void* Key = B->first;
			auto pGroup = B->second.lock();
			lock.unlock();
			if (!pGroup || !RemoveProgramFromGroupEx(pItem, pGroup)) {
				lock.lock();
				pItem->m_Groups.erase(Key);
			}
		}
	}

	// remove the item from all maps
	std::unique_lock lock(pItem->m_Mutex);
	switch (ID.GetType())
	{
		case EProgramType::eProgramFile: {
			auto F = m_PathMap.find(ID.GetFilePath());
			if (F != m_PathMap.end())
				m_PathMap.erase(F);
		}
		case EProgramType::eFilePattern: {
			auto F = m_PatternMap.find(ID.GetFilePath());
			if (F != m_PatternMap.end())
				m_PatternMap.erase(F);
			break;
		}
		case EProgramType::eAppInstallation: {
			auto F = m_InstallMap.find(ID.GetRegKey());
			if (F != m_InstallMap.end())
				m_InstallMap.erase(F);
			break;
		}
		case EProgramType::eWindowsService: {
			auto F = m_ServiceMap.find(ID.GetServiceTag());
			if (F != m_ServiceMap.end())
				m_ServiceMap.erase(F);
			break;
		}
		case EProgramType::eAppPackage: {
			auto F = m_PackageMap.find(ID.GetAppContainerSid());
			if (F != m_PackageMap.end())
				m_PackageMap.erase(F);
			break;
		}
	}
	auto F = m_Items.find(UID);
	if (F != m_Items.end())
		m_Items.erase(F);
	return OK;
}

//void CProgramManager::BroadcastItemChanged(const CProgramItemPtr& pItem, EConfigEvent Event)
//{
//	StVariant vEvent;
//	vEvent[API_V_PROG_UID] = pItem->GetUID();
//	vEvent[API_V_EVENT_TYPE] = (uint32)Event;
//
//	theCore->BroadcastMessage(SVC_API_EVENT_PROG_ITEM_CHANGED, vEvent);
//}

//////////////////////////////////////////////////////////////////////////
// Tree Management

bool CProgramManager::AddProgramToGroup(const CProgramItemPtr& pItem, const CProgramSetPtr& pGroup)
{
	//DbgPrint("AddProgramToGroup %S -> %S\n", pItem->GetName().c_str(), pGroup->GetName().c_str());

	if(pItem == pGroup)
		return false; // can't link to itself

	std::unique_lock lock1(pItem->m_Mutex);
	auto I = pItem->m_Groups.find(pGroup.get());
	if(I != pItem->m_Groups.end())
		return false; // already linked
	pItem->m_Groups.insert(std::make_pair(pGroup.get(), pGroup));
	lock1.unlock();

	std::unique_lock lock2(pGroup->m_Mutex);
	pGroup->m_Nodes.push_back(pItem);
	return true;
}

bool CProgramManager::RemoveProgramFromGroup(const CProgramItemPtr& pItem, const CProgramSetPtr& pGroup)
{
	std::unique_lock lock1(pItem->m_Mutex);
	auto I = pItem->m_Groups.find(pGroup.get());
	if(I == pItem->m_Groups.end())
		return false; // not linked
	pItem->m_Groups.erase(I);
	lock1.unlock();

	std::unique_lock lock2(pGroup->m_Mutex);
	for(auto I = pGroup->m_Nodes.begin(); I != pGroup->m_Nodes.end(); I++) {
		if(*I == pItem) {
			pGroup->m_Nodes.erase(I);
			break;
		}
	}
	return true;
}

bool CProgramManager::RemoveProgramFromGroupEx(const CProgramItemPtr& pItem, const CProgramSetPtr& pGroup)
{
	if(!RemoveProgramFromGroup(pItem, pGroup))
		return false;

	CProgramSetPtr pSet = std::dynamic_pointer_cast<CProgramSet>(pItem);
	if (pSet) {
		std::unique_lock lock(m_Mutex);
		for (auto pChild : pSet->m_Nodes) {
			if(!AddItemToBranch(pChild, pGroup) && (pGroup != m_Root || pItem->GetGroupCount() == 0))
				AddProgramToGroup(pChild, pGroup);
		}
	}
	return true;
}

void CProgramManager::AddItemToRoot(const CProgramItemPtr& pItem)
{
	AddItemToBranch(pItem, m_Root);
	if (pItem->GetGroupCount() == 0) {
		AddProgramToGroup(pItem, m_Root);
		if(CProgramPatternPtr pPattern = std::dynamic_pointer_cast<CProgramPattern>(pItem))
			TryAddChildren(m_Root, pPattern, true);
	}
}

bool CProgramManager::AddItemToBranch(const CProgramItemPtr& pItem, const CProgramSetPtr& pBranch)
{
	std::wstring FileName;
	if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
		FileName = pProgram->GetPath();
	else if (CProgramListExPtr pList = std::dynamic_pointer_cast<CProgramListEx>(pItem))
		FileName = pList->GetPath();
	else
		return false;

	std::wstring FilePath = theCore->NormalizePath(FileName);
	if (FilePath.empty())
		return false;

	return AddItemToBranch2(FilePath, pItem, pBranch);
}

bool CProgramManager::AddItemToBranch2(const std::wstring& FilePath, const CProgramItemPtr& pItem, const CProgramSetPtr& pBranch)
{
	int MatchCount = 0;

	std::unique_lock lock(pBranch->m_Mutex);
	for(auto pNode: pBranch->m_Nodes)
	{
		CProgramListExPtr pSubBranch = std::dynamic_pointer_cast<CProgramListEx>(pNode);
		if(pSubBranch && pSubBranch->MatchFileName(FilePath))
		{
			if (!AddItemToBranch2(FilePath, pItem, pSubBranch)) {
				AddProgramToGroup(pItem, pSubBranch);
				if(CProgramPatternPtr pPattern = std::dynamic_pointer_cast<CProgramPattern>(pItem))
					TryAddChildren(pSubBranch, pPattern, true);
			}
			MatchCount++;
		}
	}

	return MatchCount > 0;
}

void CProgramManager::TryAddChildren(const CProgramListPtr& pBranche, const CProgramPatternPtr& pPattern, bool bRemove)
{
	std::unique_lock lock2(pBranche->m_Mutex);
	for(auto I = pBranche->m_Nodes.begin(); I != pBranche->m_Nodes.end(); )
	{
		CProgramItemPtr pItem = *I;
		if (pItem != pPattern)
		{
			std::wstring FileName;
			if(CProgramPatternPtr pPattern = std::dynamic_pointer_cast<CProgramPattern>(pItem))
				FileName = pPattern->GetPattern();
			else if(CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
				FileName = pProgram->GetPath();
			if (!FileName.empty() && pPattern->MatchFileName(FileName)) {
				if (bRemove) {
					std::unique_lock lock1(pItem->m_Mutex);
					pItem->m_Groups.erase(pBranche.get());
					I = pBranche->m_Nodes.erase(I);
				} else
					++I;

				AddProgramToGroup(pItem, pPattern);
				continue;
			}
			else if(CProgramListPtr pSubBranch = std::dynamic_pointer_cast<CProgramList>(pItem))
				TryAddChildren(pSubBranch, pPattern);
		}
		++I;
	}
}

size_t CProgramManager::GetLogMemUsage() const
{
	size_t Size = 0;
	for (auto I = m_Items.begin(); I != m_Items.end(); ++I) {
		CProgramItemPtr pItem = I->second;
		if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
			Size += pProgram->GetLogMemUsage();
		else if (CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
			Size += pService->GetLogMemUsage();
	}
	return Size;
}

//////////////////////////////////////////////////////////////////////////
// Librarys

CProgramLibraryPtr CProgramManager::GetLibrary(const std::wstring& Path, bool bCanAdd)
{
	std::wstring Key = theCore->NormalizePath(Path);

	std::unique_lock lock(m_Mutex);
	auto F = m_LibraryMap.find(Key);
	if (F != m_LibraryMap.end())
		return F->second;
	if (!bCanAdd)
		return NULL;
	CProgramLibraryPtr pLibrary = CProgramLibraryPtr(new CProgramLibrary(Path));
	m_LibraryMap.insert(std::make_pair(Key, pLibrary));
	m_Libraries.insert(std::make_pair(pLibrary->GetUID(), pLibrary));
	return pLibrary;
}

CProgramLibraryPtr CProgramManager::GetLibrary(uint64 Id)
{
	std::unique_lock lock(m_Mutex);
	auto F = m_Libraries.find(Id);
	if (F != m_Libraries.end())
		return F->second;
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// Rules

void CProgramManager::AddFwRule(const CFirewallRulePtr& pFwRule)
{
	CProgramItemPtr pItem = GetProgramByID(pFwRule->GetProgramID());
	//if (!pItem) return;
	std::unique_lock lock(pItem->m_Mutex);
	pItem->m_FwRules.insert(pFwRule);
}

void CProgramManager::RemoveFwRule(const CFirewallRulePtr& pFwRule)
{
	CProgramItemPtr pItem = GetProgramByID(pFwRule->GetProgramID(), false);
	if (!pItem) return;
	std::unique_lock lock(pItem->m_Mutex);
	pItem->m_FwRules.erase(pFwRule);
}

void CProgramManager::AddResRule(const CAccessRulePtr& pResRule)
{
	CProgramItemPtr pItem = GetProgramByID(pResRule->GetProgramID());
	//if (!pItem) return;
	std::unique_lock lock(pItem->m_Mutex);
	pItem->m_ResRules.insert(pResRule);
}

void CProgramManager::RemoveResRule(const CAccessRulePtr& pResRule)
{
	CProgramItemPtr pItem = GetProgramByID(pResRule->GetProgramID(), false);
	if (!pItem) return;
	std::unique_lock lock(pItem->m_Mutex);
	pItem->m_ResRules.erase(pResRule);
}

//////////////////////////////////////////////////////////////////////////
// Exec Rules

STATUS CProgramManager::LoadRules()
{
	StVariant Request;
	auto Res = theCore->Driver()->Call(API_GET_PROGRAM_RULES, Request);
	if(Res.IsError())
		return Res;

	std::unique_lock lock(m_RulesMutex);

	std::map<CFlexGuid, CProgramRulePtr> OldRules = m_Rules;

	StVariant Rules = Res.GetValue();
	for(uint32 i=0; i < Rules.Count(); i++)
	{
		StVariant Rule = Rules[i];

		CFlexGuid Guid;
		Guid.FromVariant(Rule[API_V_GUID]);

		std::wstring ProgramPath = theCore->NormalizePath(Rule[API_V_FILE_PATH].AsStr());
		CProgramID ID(ProgramPath);

		CProgramRulePtr pRule;
		auto I = OldRules.find(Guid);
		if (I != OldRules.end())
		{
			pRule = I->second;
			OldRules.erase(I);
			if (pRule->GetProgramPath() != ProgramPath) // todo
			{
				RemoveRuleUnsafe(pRule);
				pRule.reset();
			}
		}

		if (!pRule) 
		{
			pRule = std::make_shared<CProgramRule>(ID);
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

void CProgramManager::AddRuleUnsafe(const CProgramRulePtr& pRule)
{
	m_Rules.insert(std::make_pair(pRule->GetGuid(), pRule));

	CProgramItemPtr pItem = GetProgramByID(pRule->GetProgramID());
	//if (!pItem) return;
	std::unique_lock lock(pItem->m_Mutex);
	pItem->m_ProgRules.insert(pRule);
}

void CProgramManager::RemoveRuleUnsafe(const CProgramRulePtr& pRule)
{
	m_Rules.erase(pRule->GetGuid());

	CProgramItemPtr pItem = GetProgramByID(pRule->GetProgramID(), false);
	if (!pItem) return;
	std::unique_lock lock(pItem->m_Mutex);
	pItem->m_ProgRules.erase(pRule);
}

void CProgramManager::UpdateRule(const CProgramRulePtr& pRule, const CFlexGuid& Guid)
{
	std::unique_lock Lock(m_RulesMutex);

	CProgramRulePtr pOldRule;
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

void CProgramManager::OnRuleChanged(const CFlexGuid& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID)
{
	ASSERT(Type == EConfigGroup::eProgramRules);

	if(Event == EConfigEvent::eAllChanged)
		LoadRules();
	else
	{
		CProgramRulePtr pRule;
		if (Event != EConfigEvent::eRemoved) {
			StVariant Request;
			Request[API_V_GUID] = Guid.ToVariant(true);
			auto Res = theCore->Driver()->Call(API_GET_PROGRAM_RULE, Request);
			if (Res.IsSuccess()) {
				StVariant Rule = Res.GetValue();
				std::wstring ProgramPath = theCore->NormalizePath(Rule[API_V_FILE_PATH].AsStr());
				CProgramID ID(ProgramPath);
				pRule = std::make_shared<CProgramRule>(ID);
				pRule->FromVariant(Rule);
			}
		}

		UpdateRule(pRule, Guid);
	}

	StVariant vEvent;
	vEvent[API_V_GUID] = Guid.ToVariant(false);
	//vEvent[API_V_NAME] = ;
	vEvent[API_V_EVENT_TYPE] = (uint32)Event;

	theCore->BroadcastMessage(SVC_API_EVENT_EXEC_RULE_CHANGED, vEvent);
}

std::map<CFlexGuid, CProgramRulePtr> CProgramManager::GetAllRules()
{
	std::unique_lock lock(m_RulesMutex); 
	return m_Rules;
}

CProgramRulePtr CProgramManager::GetRule(const CFlexGuid& Guid)
{
	std::unique_lock Lock(m_RulesMutex);
	auto F = m_Rules.find(Guid);
	if (F != m_Rules.end())
		return F->second;
	return nullptr;
}

RESULT(std::wstring) CProgramManager::AddRule(const CProgramRulePtr& pRule)
{
	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

	StVariant Request = pRule->ToVariant(Opts);
	auto Res = theCore->Driver()->Call(API_SET_PROGRAM_RULE, Request);
	//if (Res.IsSuccess()) // will be done by the notification event
	//	UpdateRule(pRule, pRule->GetGuid());
	if(Res.IsError())
		return ERR(Res.GetStatus());
	RETURN(Res.GetValue().AsStr());
}

STATUS CProgramManager::RemoveRule(const CFlexGuid& Guid)
{
	StVariant Request;
	Request[API_V_GUID] = Guid.ToVariant(true);
	auto Res = theCore->Driver()->Call(API_DEL_PROGRAM_RULE, Request);
	//if (Res.IsSuccess()) // will be done by the notification event
	//	UpdateRule(nullptr, Guid);
	return Res;
}

//////////////////////////////////////////////////////////////////////////
// Load/Store

STATUS CProgramManager::Load()
{
	CBuffer Buffer;
	if (!ReadFile(theCore->GetDataFolder() + L"\\" API_PROGRAMS_FILE_NAME, Buffer)) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_PROGRAMS_FILE_NAME L" not found");
		return ERR(STATUS_NOT_FOUND);
	}

	StVariant Data;
	//try {
	auto ret = Data.FromPacket(&Buffer, true);
	//} catch (const CException&) {
	//	return ERR(STATUS_UNSUCCESSFUL);
	//}
	if (ret != StVariant::eErrNone) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_PROGRAMS_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	if (Data[API_S_VERSION].To<uint32>() != API_PROGRAMS_FILE_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_PROGRAMS_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	std::unique_lock lock(m_Mutex);

	//StVariant Librarys = Data[API_S_LIBRARIES];

	//for (uint32 i = 0; i < Librarys.Count(); i++)
	//{
	//	const StVariant& Library = Librarys[i];
	//
	//	std::wstring Path = Library[API_S_FILE_PATH].AsStr();
	//	CProgramLibraryPtr pLibrary = GetLibrary(Path, eCanAdd);
	//	if (pLibrary)
	//		pLibrary->FromVariant(Library);
	//}

	StVariant Programs = Data[API_S_PROGRAMS];

	std::map<CProgramSetPtr, std::list<CProgramID>> Tree;

	for (uint32 i = 0; i < Programs.Count(); i++)
	{
		const StVariant& Item = Programs[i];

		//CProgramID ID;
		//ID.FromVariant(Item.Find(API_S_ID));

		CProgramItemPtr pItem;

		SVarWriteOpt::EFormat Format;
		EProgramType Type = CProgramID::ReadType(Item, Format);
		bool IsMap = (Format == SVarWriteOpt::eMap);

		StVariantReader Reader(Item);

		if (Type == EProgramType::eProgramFile) 
		{
			std::wstring FileName = theCore->NormalizePath(IsMap ? Reader.Find(API_S_FILE_PATH) : Reader.Find(API_V_FILE_PATH), false);
			std::wstring Key = MkLower(FileName);
			CProgramFilePtr& pFile = m_PathMap[Key];
			if (!pFile) {
				pFile = CProgramFilePtr(new CProgramFile(FileName, false));
				m_Items.insert(std::make_pair(pFile->GetUID(), pFile));
			}
			pItem = pFile;
		}
		else if (Type == EProgramType::eFilePattern)
		{
			std::wstring Pattern = IsMap ? Reader.Find(API_S_PROG_PATTERN) : Reader.Find(API_V_PROG_PATTERN);
			std::wstring Key = MkLower(Pattern);
			CProgramPatternPtr& pPattern = m_PatternMap[Key];
			if (!pPattern) {
				pPattern = CProgramPatternPtr(new CProgramPattern(Pattern));
				m_Items.insert(std::make_pair(pPattern->GetUID(), pPattern));
			}
			pItem = pPattern;
		}
		else if (Type == EProgramType::eAppInstallation)
		{
			std::wstring RegKey = IsMap ? Reader.Find(API_S_REG_KEY) : Reader.Find(API_V_REG_KEY);
			std::wstring Key = MkLower(RegKey);
			CAppInstallationPtr& pInstall = m_InstallMap[Key];
			if (!pInstall) {
				pInstall = CAppInstallationPtr(new CAppInstallation(RegKey));
				m_Items.insert(std::make_pair(pInstall->GetUID(), pInstall));
			}
			pItem = pInstall;
		}
		else if (Type == EProgramType::eWindowsService)	
		{
			std::wstring ServiceTag = IsMap ? Reader.Find(API_S_SERVICE_TAG) : Reader.Find(API_V_SERVICE_TAG);
			std::wstring Key = MkLower(ServiceTag);
			CWindowsServicePtr& pService = m_ServiceMap[Key];
			if (!pService) {
				pService = CWindowsServicePtr(new CWindowsService(ServiceTag));
				m_Items.insert(std::make_pair(pService->GetUID(), pService));
			}
			pItem = pService;
		}
		else if (Type == EProgramType::eAppPackage)	
		{
			std::wstring AppContainerSid = IsMap ? Reader.Find(API_S_APP_SID) : Reader.Find(API_V_APP_SID);
			std::wstring Key = MkLower(AppContainerSid);
			CAppPackagePtr& pPackage = m_PackageMap[Key];
			if (!pPackage) {
				pPackage = CAppPackagePtr(new CAppPackage(AppContainerSid));
				m_Items.insert(std::make_pair(pPackage->GetUID(), pPackage));
			}
			pItem = pPackage;
		}
		else if (Type == EProgramType::eProgramGroup)
		{
			StVariant ID = IsMap ? Reader.Find(API_S_ID) : Reader.Find(API_V_ID);
			std::wstring Guid = IsMap ? StVariantReader(ID).Find(API_S_APP_SID) : StVariantReader(ID).Find(API_V_APP_SID);
			std::wstring Key = MkLower(Guid);
			CProgramGroupPtr& pGroup = m_GroupMap[Key];
			if (!pGroup) {
				pGroup = CProgramGroupPtr(new CProgramGroup(Guid));
				m_Items.insert(std::make_pair(pGroup->GetUID(), pGroup));
			}
			pItem = pGroup;
		}
		else if (Type == EProgramType::eAllPrograms)		
			pItem = m_pAll;
		else if (Type == EProgramType::eProgramRoot)
			pItem = m_Root;
		else 
			continue;

		pItem->FromVariant(Item); // todo: check for errors and handle

		// We have to differ the association of the items to the groups until all items are loaded
		auto pSet = std::dynamic_pointer_cast<CProgramSet>(pItem);
		if (pSet) 
		{
			auto Items = IsMap ? Reader.Find(API_S_PROG_ITEMS) : Reader.Find(API_V_PROG_ITEMS);
			std::list<CProgramID> ItemIDs;
			StVariantReader(Items).ReadRawList([&](const StVariant& vData) {
				CProgramID ID;
				if (ID.FromVariant((vData.GetType() == VAR_TYPE_MAP) ? StVariantReader(vData).Find(API_S_ID) : StVariantReader(vData).Find(API_V_ID))) {
					if(ID == pSet->GetID())
						return;
					ItemIDs.push_back(ID);
				}
			});
			if(!ItemIDs.empty()) Tree[pSet] = ItemIDs;
		}
	}

	// now we can associate the items to the groups
	for (auto I : Tree) {
		auto pSet = I.first;
		for (auto ID : I.second) {
			auto pItem = GetProgramByID(ID, false);
			if (pItem) AddProgramToGroup(pItem, pSet);
		}
	}

	// add all flaoting items to the root
	for (auto I : m_Items)
	{
		if(I.second == m_Root)
			continue;
		if(I.second->GetGroupCount() == 0)
			AddItemToRoot(I.second);
	}

	return OK;
}

STATUS CProgramManager::Store()
{
	std::unique_lock lock(m_Mutex);

	SVarWriteOpt Opts;
	Opts.Format = SVarWriteOpt::eMap;
	Opts.Flags = SVarWriteOpt::eSaveToFile | SVarWriteOpt::eTextGuids;

	StVariant Programs;
	for (auto I : m_Items)
		Programs.Append(I.second->ToVariant(Opts));

	//StVariant Libraries;
	//for (auto I : m_Libraries)
	//	Libraries.Append(I.second->ToVariant(Opts));

	StVariant Data;
	Data[API_S_VERSION] = API_PROGRAMS_FILE_VERSION;
	Data[API_S_PROGRAMS] = Programs;
	//Data[API_S_LIBRARIES] = Libraries;

	CBuffer Buffer;
	Data.ToPacket(&Buffer);
	WriteFile(theCore->GetDataFolder() + L"\\" API_PROGRAMS_FILE_NAME, Buffer);

	return OK;
}