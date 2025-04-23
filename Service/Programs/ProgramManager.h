#pragma once
#include "../Library/Status.h"

/*
CProgramItem
  CProgramSet
	CProgramFile (path)
	CProgramList
	  CAppPackage (sid)
	  CProgramPattern (pattern)
	  	  CAppInstallation (uninstall_key)
	  CProgramGroup (custom)
	  CProgramRoot (special case)
	CAllPrograms (special case)
  CWinService (tag)
*/

#include "AppPackage.h"
//#include "ProgramHash.h"
#include "ProgramPattern.h"
#include "ProgramFile.h"
#include "WindowsService.h"
#include "../Processes/Process.h"
#include "../Processes/ServiceList.h"
#include "AppInstallation.h"
#include "InstallationList.h"
#include "PackageList.h"
#include "../Network/Firewall/FirewallRule.h"
#include "ProgramLibrary.h"
#include "ProgramRule.h"
#include "../Access/AccessRule.h"

class CProgramManager
{
public:
	CProgramManager();
	~CProgramManager();

	STATUS Init();

	STATUS Load();
	STATUS Store();

	void Update();

	void TestMissing();

	STATUS CleanUp(bool bPurgeRules = false);

	STATUS ReGroup();

	STATUS TruncateLogs();

	STATUS LoadRules();

	void AddPattern(const std::wstring& DosPattern, const std::wstring& Name);

	CProgramFilePtr AddProcess(const CProcessPtr& pProcess);
	void RemoveProcess(const CProcessPtr& pProcess);
	void AddService(const CServiceList::SServicePtr& pWinService);
	void RemoveService(const CServiceList::SServicePtr& pWinService);
	void AddPackage(const CPackageList::SPackagePtr& pAppPackage);
	void RemovePackage(const CPackageList::SPackagePtr& pAppPackage);
	void AddInstallation(const CInstallationList::SInstallationPtr& pInstalledApp);
	void RemoveInstallation(const CInstallationList::SInstallationPtr& pInstalledApp);

	RESULT(CProgramItemPtr) CreateProgram(const CProgramID& ID);
	STATUS AddProgramTo(uint64 UID, uint64 ParentUID);
	STATUS RemoveProgramFrom(uint64 UID, uint64 ParentUID, bool bDelRules = false);

	void AddFwRule(const CFirewallRulePtr& pFwRule);
	void RemoveFwRule(const CFirewallRulePtr& pFwRule);

	void AddResRule(const CAccessRulePtr& pFwRule);
	void RemoveResRule(const CAccessRulePtr& pFwRule);

	CProgramListPtr					GetRoot() const { std::unique_lock lock(m_Mutex); return m_Root; }
	CProgramSetPtr					GetAllItem() const { std::unique_lock lock(m_Mutex); return m_pAll; }
	CProgramFilePtr					GetNtOsKernel() const { std::unique_lock lock(m_Mutex); return m_NtOsKernel; }

	CProgramItemPtr					GetProgramByID(const CProgramID& ID, bool bCanAdd = true);

	CProgramGroupPtr				GetGroup(const std::wstring& Guid, bool bCanAdd = true);
	CAppPackagePtr					GetAppPackage(const std::wstring& AppContainserSid, bool bCanAdd = true);
	CWindowsServicePtr				GetService(const std::wstring& ServiceTag, bool bCanAdd = true);
	CProgramPatternPtr				GetPattern(const std::wstring& DosPattern, bool bCanAdd = true);
	CAppInstallationPtr				GetInstallation(const std::wstring& RegKey, bool bCanAdd = true);
	CProgramFilePtr					GetProgramFile(const std::wstring& FileName, bool bCanAdd = true);
	
	std::map<uint64, CProgramItemPtr>GetItems()	{ std::unique_lock lock(m_Mutex); return m_Items; }
	CProgramItemPtr					GetItem(uint64 UID);

	bool							IsPathReserved(std::wstring FileName) const;

	CProgramLibraryPtr				GetLibrary(const std::wstring& Path, bool bCanAdd = true);
	CProgramLibraryPtr				GetLibrary(uint64 Id);
	std::map<uint64, CProgramLibraryPtr>GetLibraries()	{ std::unique_lock lock(m_Mutex); return m_Libraries; }

	std::map<CFlexGuid, CProgramRulePtr> GetProgramRules() { std::unique_lock lock(m_RulesMutex); return m_Rules; }

	bool							IsNtOsKrnl(const std::wstring& FilePath) const;

	size_t							GetLogMemUsage() const;	

protected:

	void CollectSoftware();

	bool AddProgramToGroup(const CProgramItemPtr& pItem, const CProgramSetPtr& pGroup);
	bool RemoveProgramFromGroup(const CProgramItemPtr& pItem, const CProgramSetPtr& pGroup);
	bool RemoveProgramFromGroupEx(const CProgramItemPtr& pItem, const CProgramSetPtr& pGroup);
	void AddItemToRoot(const CProgramItemPtr& pItem);
	bool AddItemToBranch(const CProgramItemPtr& pItem, const CProgramSetPtr& pBranch);
	bool AddItemToBranch2(const std::wstring& FilePath, const CProgramItemPtr& pItem, const CProgramSetPtr& pBranch);
	void TryAddChildren(const CProgramListPtr& pGroup, const CProgramPatternPtr& pPattern, bool bRemove = false);

	//void BroadcastItemChanged(const CProgramItemPtr& pItem, EConfigEvent Event);

	mutable std::recursive_mutex			m_Mutex;

	std::map<std::wstring, CProgramPatternPtr>m_PatternMap;
	std::map<std::wstring, CAppPackagePtr>	m_PackageMap;
	std::map<std::wstring, CProgramFilePtr>	m_PathMap;
	std::map<std::wstring, CWindowsServicePtr>m_ServiceMap;
	std::map<std::wstring, CAppInstallationPtr>m_InstallMap;
	std::map<std::wstring, CProgramGroupPtr>m_GroupMap;

	std::map<uint64, CProgramItemPtr>		m_Items;

	CProgramListPtr							m_Root; // default programs root
	CProgramSetPtr							m_pAll; // all programs
	CProgramFilePtr							m_NtOsKernel;

	CInstallationList*						m_InstallationList;
	CPackageList*							m_PackageList;

	std::wstring							m_OsDrive;
	std::wstring							m_WinDir;
	std::wstring							m_ProgDir;

	std::map<std::wstring, CProgramLibraryPtr> m_LibraryMap;
	std::map<uint64, CProgramLibraryPtr>	m_Libraries;

	uint64									m_LastTruncateLogs = 0;

	STATUS									RemoveAllRules(const CProgramItemPtr& pItem);

	//////////////////////////////////////////////////////////////////////////
	// Rules

	void UpdateRule(const CProgramRulePtr& pRule, const CFlexGuid& Guid);

	void AddRuleUnsafe(const CProgramRulePtr& pRule);
	void RemoveRuleUnsafe(const CProgramRulePtr& pRule);

	std::recursive_mutex					m_RulesMutex;

	bool									m_UpdateAllRules = true;
	std::map<CFlexGuid, CProgramRulePtr> m_Rules;

	void OnRuleChanged(const CFlexGuid& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID);

	//////////////////////////////////////////////////////////////////////////
	// TruncateLogs

	friend DWORD CALLBACK CProgramManager__TruncateLogs(LPVOID lpThreadParameter);
	bool					m_bCancelTruncateLogs = false;
	volatile HANDLE			m_hTruncateLogsThread = NULL;
};

