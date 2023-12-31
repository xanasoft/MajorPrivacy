#pragma once
#include "../Library/Status.h"

/*
CProgramItem
  CProgramSet
	CProgramFile (path)
	CAllProgram (special case)
	CProgramList
	  CAppPackage (sid)
	  CAppInstallation (uninstall_key)
	  CProgramPattern (pattern)
	  CProgramGroup (custom)
  CWinService (tag)
*/

#include "AppPackage.h"
//#include "ProgramHash.h"
#include "ProgramPattern.h"
#include "ProgramFile.h"
#include "../Processes/Process.h"
#include "../Processes/ServiceList.h"
#include "AppInstallation.h"
#include "InstallationList.h"
#include "PackageList.h"
#include "../Network/Firewall/FirewallRule.h"

class CProgramManager
{
public:
	CProgramManager();
	~CProgramManager();

	STATUS Init();

	void AddPattern(const std::wstring& Pattern, const std::wstring& Name);

	void AddProcess(const CProcessPtr& pProcess);
	void RemoveProcess(const CProcessPtr& pProcess);
	void AddService(const CProcessPtr& pProcess, const TServiceId& Id);
	void RemoveService(const CProcessPtr& pProcess, const TServiceId& Id);
	void AddService(const CServiceList::SServicePtr& pWinService);
	void RemoveService(const CServiceList::SServicePtr& pWinService);
	void AddPackage(const CPackageList::SPackagePtr& pAppPackage);
	void RemovePackage(const CPackageList::SPackagePtr& pAppPackage);
	void AddInstallation(const CInstallationList::SInstallationPtr& pInstalledApp);
	void RemoveInstallation(const CInstallationList::SInstallationPtr& pInstalledApp);

	void AddFwRule(const CFirewallRulePtr& pFwRule);
	void RemoveFwRule(const CFirewallRulePtr& pFwRule);

	CProgramGroupPtr				GetRoot() const { std::unique_lock lock(m_Mutex); return m_Root; }
	CProgramSetPtr					GetOnlyAll() const { std::unique_lock lock(m_Mutex); return m_pAll; }
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
	std::vector<CProgramSetPtr>		FindPatternGroups(const std::wstring& FileName);
	std::vector<CProgramSetPtr>		FindGroups(const std::wstring& FileName);
	CProgramFilePtr					GetProgramFile(const std::wstring& FileName, EAddFlags AddFlags = eCanAdd);

	//std::map<TAppId, CProgramPatternPtr>	GetPatterns()		{ std::unique_lock lock(m_Mutex); return m_PatternMap; }
	//std::map<TAppId, CAppPackagePtr>		GetApps()			{ std::unique_lock lock(m_Mutex); return m_PackageMap; }
	//std::map<TFilePath, CProgramFilePtr>	GetApplications()	{ std::unique_lock lock(m_Mutex); return m_PathMap; }
	//std::map<TServiceId, CWindowsServicePtr>GetServices()		{ std::unique_lock lock(m_Mutex); return m_ServiceMap; }
	//std::map<TAppId, CAppInstallationPtr>	GetInstallations()	{ std::unique_lock lock(m_Mutex); return m_InstallMap; }

	
	std::map<uint64, CProgramItemPtr>GetItems()	{ std::unique_lock lock(m_Mutex); return m_Items; }

	bool							IsPathReserved(std::wstring FileName) const;

protected:
	
	mutable std::recursive_mutex			m_Mutex;

	void AppendGroupToList(std::vector<CProgramSetPtr>& Groups, const CProgramSetPtr& pGroup);

	bool AddProgramToGroup(const CProgramItemPtr& pProgram, const CProgramSetPtr& pGroup);
	void AddProgramToGroups(const CProgramItemPtr& pProgram, const std::vector<CProgramSetPtr>& Groups);

	bool AddProgramPattern(const CProgramPatternPtr& pPattern, const CProgramSetPtr& pGroup);
	void ApplyPattern(const CProgramPatternPtr& pPattern);

	void CollectData(const CProgramItemPtr& pItem);

	std::map<TPatternId, CProgramPatternPtr>m_PatternMap;
	std::map<TAppId, CAppPackagePtr>		m_PackageMap;
	std::map<TFilePath, CProgramFilePtr>	m_PathMap;
	std::map<TServiceId, CWindowsServicePtr>m_ServiceMap;
	std::map<TInstallId, CAppInstallationPtr>m_InstallMap;

	std::map<uint64, CProgramItemPtr>		m_Items;

	CProgramGroupPtr						m_Root; // default programs root
	CProgramSetPtr							m_pAll; // all programs
	CProgramFilePtr							m_NtOsKernel;

	CInstallationList*						m_InstallationList;
	CPackageList*							m_PackageList;

	std::wstring							m_WinDir;
	std::wstring							m_ProgDir;
};

