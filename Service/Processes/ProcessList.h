#pragma once
#include "../Library/Status.h"
#include "Process.h"
#include "Programs/ProgramID.h"

#define NT_OS_KERNEL_PID	4

class CProcessList
{
public:
	CProcessList();
	~CProcessList();

	STATUS Init();

	void Update();

	STATUS EnumProcesses();

	std::map<uint64, CProcessPtr> List() { 
		std::unique_lock Lock(m_Mutex);
		return m_List; 
	}
	CProcessPtr GetProcess(uint64 Pid, bool bCanAdd = false);

	std::map<uint64, CProcessPtr> FindProcesses(const CProgramID& ID);

	class CServiceList* Services() { return m_Services; }

protected:

	void AddProcessUnsafe(const CProcessPtr& pProcess);
	void RemoveProcessUnsafe(const CProcessPtr& pProcess);

	std::wstring GetPathFromCmd(const std::wstring& CommandLine, const std::wstring& FileName, uint64 ParentID);

	NTSTATUS OnProcessDrvEvent(const struct SProcessEvent* pEvent);
	void OnProcessEtwEvent(const struct SEtwProcessEvent* pEvent);

	void TerminateProcess(uint64 Pid, const std::wstring& FileName);

	bool OnProcessStarted(uint64 Pid, uint64 ParentPid, const std::wstring& FileName, const std::wstring& Command, uint64 CreateTime, bool bETW = false);
	void OnProcessStopped(uint64 Pid, uint32 ExitCode);
	
	mutable std::recursive_mutex m_Mutex;

	class CServiceList* m_Services;

	std::map<uint64, CProcessPtr> m_List;
	std::multimap<std::wstring, CProcessPtr> m_Apps;
	std::multimap<std::wstring, CProcessPtr> m_ByPath;
};