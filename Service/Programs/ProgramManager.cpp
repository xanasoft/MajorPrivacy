#include "pch.h"
#include "ProgramManager.h"
#include "../ServiceCore.h"
#include "../Processes/ProcessList.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Network/NetIsolator.h"
#include "../Network/Firewall/Firewall.h"

CProgramManager::CProgramManager()
{
	m_InstallationList = new CInstallationList;
	m_PackageList = new CPackageList;
}

CProgramManager::~CProgramManager()
{
	delete m_InstallationList;
	delete m_PackageList;
}

STATUS CProgramManager::Init()
{
	m_Root = CProgramGroupPtr(new CProgramGroup()); 
	m_Root->m_UID = 1;
	m_Root->m_ID.Set(CProgramID::eFilePattern, L"");
	m_Items.insert(std::make_pair(m_Root->GetUID(), m_Root));

	m_pAll = CProgramSetPtr(new CAllProgram());
	m_pAll->SetName(L"All Programs");
	AddProgramToGroup(m_pAll, m_Root);
	m_Items.insert(std::make_pair(m_pAll->GetUID(), m_pAll));

	m_NtOsKernel = CProgramFilePtr(new CProgramFile(CProcess::NtOsKernel_exe));
	m_NtOsKernel->SetName(L"System (NT OS Kernel)");
	m_PathMap[NormalizeFilePath(CProcess::NtOsKernel_exe)] = m_NtOsKernel;
	AddProgramToGroup(m_NtOsKernel, m_Root);

	WCHAR windir[MAX_PATH + 8];
    GetWindowsDirectoryW(windir, MAX_PATH);
	m_WinDir = DosPathToNtPath(windir);
	m_ProgDir = DosPathToNtPath(GetProgramFilesDirectory());

	AddPattern(m_WinDir + L"\\*", L"Windows");
	//AddPattern(m_WinDir + L"\\System32\\svchost.exe", L"Windows Service Host");
	AddPattern(m_WinDir + L"\\System32\\*", L"System32");
	AddPattern(m_WinDir + L"\\SysWOW64\\*", L"System32 (x86)");
	AddPattern(m_WinDir + L"\\SystemApps*", L"System Apps");
	AddPattern(m_ProgDir + L"\\WindowsApps*", L"User Apps");
	AddPattern(m_ProgDir + L" (x86)\\Microsoft\\Edge*", L"Microsoft Edge");

	m_InstallationList->Init();
	m_PackageList->Init();

	return OK;
}

bool CProgramManager::IsPathReserved(std::wstring FileName) const
{
	FileName = NormalizeFilePath(FileName);
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

void CProgramManager::AddPattern(const std::wstring& Pattern, const std::wstring& Name)
{
	CProgramPatternPtr pPattern = CProgramPatternPtr(new CProgramPattern(Pattern));
	pPattern->SetName(Name);
	std::vector<CProgramSetPtr> Groups = FindGroups(Pattern);

	std::unique_lock lock(m_Mutex);
	m_PatternMap.insert(std::make_pair(pPattern->GetPattern(), pPattern));
	m_Items.insert(std::make_pair(pPattern->GetUID(), pPattern));
	AddProgramPattern(pPattern, Groups.size() == 1 ? Groups[0] : m_Root);
}

bool CProgramManager::AddProgramToGroup(const CProgramItemPtr& pProgram, const CProgramSetPtr& pGroup)
{
	// todo: check for recursion!!!!!

	std::unique_lock lock1(pProgram->m_Mutex);
	std::unique_lock lock2(pGroup->m_Mutex);

	pProgram->m_Groups.insert(std::make_pair(pGroup.get(), pGroup));
	pGroup->m_Nodes.push_back(pProgram);
	return true;
}

void CProgramManager::AddProgramToGroups(const CProgramItemPtr& pProgram, const std::vector<CProgramSetPtr>& Groups)
{
	if(Groups.empty())
		AddProgramToGroup(pProgram, m_Root);
	else {
		for(auto pGroup: Groups)
			AddProgramToGroup(pProgram, pGroup);
	}
}

bool CProgramManager::AddProgramPattern(const CProgramPatternPtr& pPattern, const CProgramSetPtr& pGroup)
{
	if (!AddProgramToGroup(pPattern, pGroup))
		return false;
	ApplyPattern(pPattern);
	CollectData(pPattern);
	return true;
}

void CProgramManager::ApplyPattern(const CProgramPatternPtr& pPattern)
{
	// todo: sort all existing entries into the pattern
}

CProgramItemPtr CProgramManager::GetProgramByID(const CProgramID& ID, EAddFlags AddFlags)
{
	switch (ID.GetType())
	{
	case CProgramID::eFile:		return GetProgramFile(ID.GetFilePath(), AddFlags);
	case CProgramID::eService:	return GetService(ID.GetServiceTag(), AddFlags);
	case CProgramID::eApp:		return GetAppPackage(ID.GetAppContainerSid(), AddFlags);
	default:
#ifdef _DEBUG
		DebugBreak();
#endif
	case CProgramID::eAll:		return m_pAll;
	}
}

CAppPackagePtr CProgramManager::GetAppPackage(const TAppId& _Id, EAddFlags AddFlags)
{
	TAppId Id = MkLower(_Id);

	std::unique_lock lock(m_Mutex);
	if (AddFlags & eDontAdd) {
		auto F = m_PackageMap.find(Id);
		if (F != m_PackageMap.end())
			return F->second;
		return NULL;
	}
	CAppPackagePtr& pAppPackage = m_PackageMap[Id];
	if (!pAppPackage) {
		pAppPackage = CAppPackagePtr(new CAppPackage(Id));
		m_Items.insert(std::make_pair(pAppPackage->GetUID(), pAppPackage));
		//pAppPackage->SetInstallPath(GetAppContainerRootPath( // todo:
		// todo: icon
		lock.unlock();

		AddProgramToGroup(pAppPackage, GetRoot());
		if(!(AddFlags & eDontFill))
			CollectData(pAppPackage);
	}
	return pAppPackage;
}

CWindowsServicePtr CProgramManager::GetService(const TServiceId& _Id, EAddFlags AddFlags)
{
	TServiceId Id = MkLower(_Id);

	std::unique_lock lock(m_Mutex);
	if (AddFlags & eDontAdd) {
		auto F = m_ServiceMap.find(Id);
		if (F != m_ServiceMap.end())
			return F->second;
		return NULL;
	}
	CWindowsServicePtr& pService = m_ServiceMap[Id];
	if (!pService) {
		pService = CWindowsServicePtr(new CWindowsService(Id));
		m_Items.insert(std::make_pair(pService->GetUID(), pService));
		lock.unlock();

		CServiceList::SServicePtr pWinService = svcCore->ProcessList()->Services()->GetService(Id);
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

			pService->SetName(Id);
			pParent = m_pAll; 
		}

		AddProgramToGroup(pService, pParent);
		if(!(AddFlags & eDontFill))
			CollectData(pService);
	}
	return pService;
}

void CProgramManager::AppendGroupToList(std::vector<CProgramSetPtr>& Groups, const CProgramSetPtr& pGroup)
{
	for (auto I = Groups.begin(); I != Groups.end(); ) 
	{
		if ((*I)->ContainsNode(pGroup)) {
			I = Groups.erase(I);
			continue;
		}

		if ((*I)->GetSpecificity() != 0 && pGroup->GetSpecificity() != 0)
		{
			if((*I)->GetSpecificity() < pGroup->GetSpecificity()) {
				I = Groups.erase(I);
				continue;
			}

			if((*I)->GetSpecificity() > pGroup->GetSpecificity())
				return; // dont add
		}

		I++;
	}

	Groups.push_back(pGroup);
}

std::vector<CProgramSetPtr> CProgramManager::FindPatternGroups(const std::wstring& FileName)
{
	if (FileName.empty())
		return std::vector<CProgramSetPtr>();

	std::unique_lock lock(m_Mutex);

	std::vector<CProgramSetPtr> Groups;

	for(auto pPattern: m_PatternMap)
	{
		if (pPattern.second->MatchFileName(FileName))
			AppendGroupToList(Groups, pPattern.second);
	}

	return Groups;
}

std::vector<CProgramSetPtr> CProgramManager::FindGroups(const std::wstring& FileName)
{
	if (FileName.empty())
		return std::vector<CProgramSetPtr>();

	std::wstring FilePath = NormalizeFilePath(FileName);

	std::vector<CProgramSetPtr> Groups = FindPatternGroups(FilePath);

	std::unique_lock lock(m_Mutex);

	for(auto pInstall: m_InstallMap)
	{
		std::wstring InstallPath = pInstall.second->GetInstallPath();
		if (!InstallPath.empty() && _wcsnicmp(FilePath.c_str(), InstallPath.c_str(), InstallPath.length()) == 0)
			AppendGroupToList(Groups, pInstall.second);
	}

	return Groups;
}

CProgramFilePtr CProgramManager::GetProgramFile(const std::wstring& FileName, EAddFlags AddFlags)
{
	std::wstring fileName = NormalizeFilePath(FileName);

	std::unique_lock lock(m_Mutex);
	if (AddFlags & eDontAdd) {
		auto F = m_PathMap.find(fileName);
		if (F != m_PathMap.end())
			return F->second;
		return NULL;
	}
	CProgramFilePtr& pProgram = m_PathMap[fileName];
	if (!pProgram) {
		pProgram = CProgramFilePtr(new CProgramFile(FileName));
		m_Items.insert(std::make_pair(pProgram->GetUID(), pProgram));
		//pProgram->SetName(GetFileNameFromPath(FileName));
		lock.unlock();

		std::vector<CProgramSetPtr> Groups = FindGroups(FileName);
		AddProgramToGroups(pProgram, Groups);
		if(!(AddFlags & eDontFill))
			CollectData(pProgram);
	}
	return pProgram;
}

void CProgramManager::AddProcess(const CProcessPtr& pProcess)
{
	std::set<TServiceId> Services = pProcess->GetServices();
	if (!Services.empty()) {
		for(auto SvcId: Services)
			AddService(pProcess, SvcId);
		// todo file name
		return;
	}

	std::wstring fileName = pProcess->GetFileName();

	std::unique_lock lock(m_Mutex);
	CProgramFilePtr& pProgram = m_PathMap[NormalizeFilePath(fileName)];
	if (!pProgram) {
		pProgram = CProgramFilePtr(new CProgramFile(fileName));
		m_Items.insert(std::make_pair(pProgram->GetUID(), pProgram));
		//pProgram->SetName(pProcess->GetName());
		// todo: icon, description
	}
	lock.unlock();

	pProgram->AddProcess(pProcess);
	
	pProcess->m_Mutex.lock();
	pProcess->m_pFileRef = pProgram;
	pProcess->m_Mutex.unlock();

	std::vector<CProgramSetPtr> Groups = FindGroups(fileName);

	if (!pProcess->GetAppContainerName().empty())
		AppendGroupToList(Groups, GetAppPackage(pProcess->GetAppContainerSid()));

	auto CurGroups = pProgram->GetGroups();
	if(Groups.empty() && CurGroups.empty())
		AddProgramToGroup(pProgram, GetRoot());
	else {
		for(auto pGroup: Groups) {
			if (CurGroups.find(pGroup.get()) == CurGroups.end())
				AddProgramToGroup(pProgram, pGroup);
		}
	}
	CollectData(pProgram);
}

void CProgramManager::RemoveProcess(const CProcessPtr& pProcess)
{
	std::set<TServiceId> Services = pProcess->GetServices();
	if (!Services.empty()) {
		for(auto SvcId: Services)
			RemoveService(pProcess, SvcId);
	}
	
	pProcess->m_Mutex.lock();
	CProgramFilePtr pProgram = pProcess->m_pFileRef.lock();
	pProcess->m_pFileRef.reset();
	pProcess->m_Mutex.unlock();

	if (pProgram)
		pProgram->RemoveProcess(pProcess);
}

void CProgramManager::AddService(const CProcessPtr& pProcess, const TServiceId& Id)
{
	CWindowsServicePtr pService = GetService(Id);

	pService->SetProcess(pProcess);
}

void CProgramManager::RemoveService(const CProcessPtr& pProcess, const TServiceId& Id)
{
	CWindowsServicePtr pService = GetService(Id, eDontAdd);
	if(pService)
		pService->SetProcess(NULL);
}

void CProgramManager::AddService(const CServiceList::SServicePtr& pWinService)
{
	TServiceId Id = MkLower(pWinService->Id);

	std::unique_lock lock(m_Mutex);
	CWindowsServicePtr& pService = m_ServiceMap[Id];
	if (!pService) {
		pService = CWindowsServicePtr(new CWindowsService(pWinService->Id));
		m_Items.insert(std::make_pair(pService->GetUID(), pService));
		pService->SetName(pWinService->Name);
		lock.unlock();

		CProgramSetPtr pParent = GetProgramFile(pWinService->BinaryPath);

		AddProgramToGroup(pService, pParent);
		CollectData(pService);
	}
}

void CProgramManager::RemoveService(const CServiceList::SServicePtr& pWinService)
{
	std::unique_lock lock(m_Mutex);

}

void CProgramManager::AddPackage(const CPackageList::SPackagePtr& pPackage)
{
	TAppId Id = MkLower(pPackage->PackageSid);

	std::unique_lock lock(m_Mutex);
	CAppPackagePtr& pAppPackage = m_PackageMap[Id];
	if (!pAppPackage) {
		pAppPackage = CAppPackagePtr(new CAppPackage(pPackage->PackageSid, pPackage->PackageName));
		m_Items.insert(std::make_pair(pAppPackage->GetUID(), pAppPackage));
		pAppPackage->SetName(pPackage->PackageDisplayName);
		pAppPackage->SetInstallPath(NormalizeFilePath(pPackage->PackageInstallPath));
		pAppPackage->SetIcon(DosPathToNtPath(pPackage->SmallLogoPath));
		lock.unlock();

		std::vector<CProgramSetPtr> Groups = FindPatternGroups(pAppPackage->GetInstallPath());
		AddProgramToGroups(pAppPackage, Groups);
		CollectData(pAppPackage);
	}
}

void CProgramManager::RemovePackage(const CPackageList::SPackagePtr& pPackage)
{
	std::unique_lock lock(m_Mutex);

	// keep old packages to keep the structure and collected usage data
	// todo: add custom cleanup function
}

void CProgramManager::AddInstallation(const CInstallationList::SInstallationPtr& pInstalledApp)
{
	TInstallId Id = MkLower(pInstalledApp->RegKey);

	std::unique_lock lock(m_Mutex);
	CAppInstallationPtr& pAppInstall = m_InstallMap[Id];
	if (!pAppInstall) {
		pAppInstall = CAppInstallationPtr(new CAppInstallation(pInstalledApp->RegKey));
		m_Items.insert(std::make_pair(pAppInstall->GetUID(), pAppInstall));
		pAppInstall->SetName(pInstalledApp->DisplayName);
		pAppInstall->SetInstallPath(NormalizeFilePath(pInstalledApp->InstallPath));
		pAppInstall->SetIcon(pInstalledApp->DisplayIcon);
		lock.unlock();

		std::vector<CProgramSetPtr> Groups = FindPatternGroups(pAppInstall->GetInstallPath());
		AddProgramToGroups(pAppInstall, Groups);
		CollectData(pAppInstall);
	}
}

void CProgramManager::RemoveInstallation(const CInstallationList::SInstallationPtr& pInstalledApp)
{
	std::unique_lock lock(m_Mutex);

}

void CProgramManager::CollectData(const CProgramItemPtr& pItem)
{
	//
	// Note: when calling this function ensure locks on CProgramManager::m_Mutex have been unlocked
	//

	auto FwRules = svcCore->NetIsolator()->Firewall()->FindRules(pItem->GetID());

	std::unique_lock lock(pItem->m_Mutex);
	for(auto I: FwRules)
		pItem->m_FwRules.insert(I.second);
}

void CProgramManager::AddFwRule(const CFirewallRulePtr& pFwRule)
{
	CProgramItemPtr pItem = GetProgramByID(pFwRule->GetProgramID(), eDontFill);
	//if (!pItem) return;
	std::unique_lock lock(pItem->m_Mutex);
	pItem->m_FwRules.insert(pFwRule);
}

void CProgramManager::RemoveFwRule(const CFirewallRulePtr& pFwRule)
{
	CProgramItemPtr pItem = GetProgramByID(pFwRule->GetProgramID(), eDontAdd);
	if (!pItem) return;
	std::unique_lock lock(pItem->m_Mutex);
	pItem->m_FwRules.erase(pFwRule);
}
