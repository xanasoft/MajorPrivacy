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
	void Cleanup();

	STATUS LoadRules();

	void AddPattern(const std::wstring& Pattern, const std::wstring& Name);

	void AddProcess(const CProcessPtr& pProcess);
	void RemoveProcess(const CProcessPtr& pProcess);
	void AddService(const CServiceList::SServicePtr& pWinService);
	void RemoveService(const CServiceList::SServicePtr& pWinService);
	void AddPackage(const CPackageList::SPackagePtr& pAppPackage);
	void RemovePackage(const CPackageList::SPackagePtr& pAppPackage);
	void AddInstallation(const CInstallationList::SInstallationPtr& pInstalledApp);
	void RemoveInstallation(const CInstallationList::SInstallationPtr& pInstalledApp);

	RESULT(CProgramItemPtr) CreateProgram(const CProgramID& ID);
	STATUS RemoveProgramFrom(uint64 UID, uint64 ParentUID);

	void AddFwRule(const CFirewallRulePtr& pFwRule);
	void RemoveFwRule(const CFirewallRulePtr& pFwRule);

	void AddResRule(const CAccessRulePtr& pFwRule);
	void RemoveResRule(const CAccessRulePtr& pFwRule);

	CProgramListPtr					GetRoot() const { std::unique_lock lock(m_Mutex); return m_Root; }
	CProgramSetPtr					GetAllItem() const { std::unique_lock lock(m_Mutex); return m_pAll; }
	CProgramFilePtr					GetNtOsKernel() const { std::unique_lock lock(m_Mutex); return m_NtOsKernel; }

	enum EAddFlags
	{
		eCanAdd = 0,
		eDontAdd = 1,
		eDontFill = 2,
	};

	CProgramItemPtr					GetProgramByID(const CProgramID& ID, EAddFlags AddFlags = eCanAdd);

	CAppPackagePtr					GetAppPackage(const TAppId& Id, EAddFlags AddFlags = eCanAdd);
	CWindowsServicePtr				GetService(const TServiceId& Id, EAddFlags AddFlags = eCanAdd);
	CProgramPatternPtr				GetPattern(const TPatternId& Id, EAddFlags AddFlags = eCanAdd);
	CAppInstallationPtr				GetInstallation(const TInstallId& Id, EAddFlags AddFlags = eCanAdd);
	CProgramFilePtr					GetProgramFile(const std::wstring& FileName, EAddFlags AddFlags = eCanAdd);

	//std::map<TAppId, CProgramPatternPtr>	GetPatterns()		{ std::unique_lock lock(m_Mutex); return m_PatternMap; }
	//std::map<TAppId, CAppPackagePtr>		GetApps()			{ std::unique_lock lock(m_Mutex); return m_PackageMap; }
	//std::map<TFilePath, CProgramFilePtr>	GetApplications()	{ std::unique_lock lock(m_Mutex); return m_PathMap; }
	//std::map<TServiceId, CWindowsServicePtr>GetServices()		{ std::unique_lock lock(m_Mutex); return m_ServiceMap; }
	//std::map<TAppId, CAppInstallationPtr>	GetInstallations()	{ std::unique_lock lock(m_Mutex); return m_InstallMap; }

	
	std::map<uint64, CProgramItemPtr>GetItems()	{ std::unique_lock lock(m_Mutex); return m_Items; }
	CProgramItemPtr					GetItem(uint64 UID);

	bool							IsPathReserved(std::wstring FileName) const;

	CProgramLibraryPtr				GetLibrary(const TFilePath& Path, bool bCanAdd = true);
	CProgramLibraryPtr				GetLibrary(uint64 Id);
	std::map<uint64, CProgramLibraryPtr>GetLibraries()	{ std::unique_lock lock(m_Mutex); return m_Libraries; }

	std::map<std::wstring, CProgramRulePtr> GetProgramRules() { std::unique_lock lock(m_RulesMutex); return m_Rules; }

protected:
	
	void CollectData(const CProgramItemPtr& pItem);

	bool AddProgramToGroup(const CProgramItemPtr& pItem, const CProgramSetPtr& pGroup);
	bool RemoveProgramFromGroup(const CProgramItemPtr& pItem, const CProgramSetPtr& pGroup);
	bool RemoveProgramFromGroupEx(const CProgramItemPtr& pItem, const CProgramSetPtr& pGroup);
	void AddItemToRoot(const CProgramItemPtr& pItem);
	bool AddItemToBranch(const CProgramItemPtr& pItem, const CProgramSetPtr& pBranch);
	void TryAddChildren(const CProgramListPtr& pGroup, const CProgramPatternPtr& pPattern, bool bRemove = false);

	mutable std::recursive_mutex			m_Mutex;

	std::map<TPatternId, CProgramPatternPtr>m_PatternMap;
	std::map<TAppId, CAppPackagePtr>		m_PackageMap;
	std::map<TFilePath, CProgramFilePtr>	m_PathMap;
	std::map<TServiceId, CWindowsServicePtr>m_ServiceMap;
	std::map<TInstallId, CAppInstallationPtr>m_InstallMap;

	std::map<uint64, CProgramItemPtr>		m_Items;

	CProgramListPtr							m_Root; // default programs root
	CProgramSetPtr							m_pAll; // all programs
	CProgramFilePtr							m_NtOsKernel;

	CInstallationList*						m_InstallationList;
	CPackageList*							m_PackageList;

	std::wstring							m_OsDrive;
	std::wstring							m_WinDir;
	std::wstring							m_ProgDir;

	std::map<TFilePath, CProgramLibraryPtr>	m_LibraryMap;
	std::map<uint64, CProgramLibraryPtr>	m_Libraries;

	uint64									m_LastCleanup = 0;

	//////////////////////////////////////////////////////////////////////////
	// Rules

	void UpdateRule(const CProgramRulePtr& pRule, const std::wstring& Guid);

	void AddRuleUnsafe(const CProgramRulePtr& pRule);
	void RemoveRuleUnsafe(const CProgramRulePtr& pRule);

	std::recursive_mutex					m_RulesMutex;

	bool									m_UpdateAllRules = true;
	std::map<std::wstring, CProgramRulePtr> m_Rules;

	void OnRuleChanged(const std::wstring& Guid, enum class ERuleEvent Event, enum class ERuleType Type, uint64 PID);
};

