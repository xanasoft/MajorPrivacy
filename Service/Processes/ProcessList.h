#pragma once
#include "../Library/Status.h"
#include "Process.h"
#include "Programs/ProgramID.h"
#include "ExecLogEntry.h"

#define NT_OS_KERNEL_PID	4

class CProcessList
{
	TRACK_OBJECT(CProcessList)
public:
	CProcessList();
	~CProcessList();

	STATUS Init();

	STATUS Load();
	STATUS Store();
	STATUS StoreAsync();

	void Update();

	void Reconfigure();

	STATUS EnumProcesses();

	std::map<SProcessUID, CProcessPtr> List() { std::unique_lock Lock(m_Mutex); return m_ProcessMap; }
	enum EGetMode
	{
		eCanNotAdd = 0, // if process is not listed return null
		eCanAdd, // if process is not listed try to add it when its running, else return null
		eMustAdd // always add process even if its not running
	};
	CProcessPtr GetProcessEx(uint64 Pid, EGetMode Mode);
	CProcessPtr GetProcess(uint64 Pid, bool bCanAdd = false) { return GetProcessEx(Pid, bCanAdd ? eMustAdd : eCanNotAdd); }

	std::map<SProcessUID, CProcessPtr> FindProcesses(const CProgramID& ID);

	class CServiceList* Services() { return m_Services; }

	STATUS SetAccessEventAction(uint64 EventId, EAccessRuleType Action);

protected:

	void AddProcessImpl(const CProcessPtr& pProcess);
	void AddProcessUnsafe(const CProcessPtr& pProcess);
	void RemoveProcessUnsafe(const CProcessPtr& pProcess);

	std::wstring GetPathFromCmd(const std::wstring& CommandLine, const std::wstring& FileName, uint64 ParentID);

	NTSTATUS OnProcessDrvEvent(const struct SProcessEvent* pEvent);
	void OnProcessEtwEvent(const struct SEtwProcessEvent* pEvent);

	//void TerminateProcess(uint64 Pid, const std::wstring& FileName);

	EProgramOnSpawn GetAllowStart(uint64 Pid, uint64 ParentPid, uint64 ActorPid, const std::wstring& ActorServiceTag, /*const std::wstring& EnclaveId,*/ const std::wstring& NtPath, const std::wstring& Command, const struct SVerifierInfo* pVerifyInfo, uint64 CreateTime, const CFlexGuid& RuleGuid, const CFlexGuid& EnclaveGuid, EEventStatus Status, uint32 TimeOut);
	bool OnProcessStarted(uint64 Pid, uint64 ParentPid, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, const std::wstring& NtPath, const std::wstring& Command, const struct SVerifierInfo* pVerifyInfo, uint64 CreateTime, EEventStatus Status, bool bETW = false);
	void OnProcessStopped(uint64 Pid, uint32 ExitCode);

	void OnProcessAccessed(uint64 Pid, uint64 ActorPid, const std::wstring& ActorServiceTag, bool bThread, uint32 AccessMask, uint64 AccessTime, EEventStatus Status);

	EImageOnLoad GetAllowImage(const struct SProcessImageEvent* pImageEvent);
	void OnImageEvent(const struct SProcessImageEvent* pImageEvent);
	
	EAccessRuleType GetResourceAccess(const std::wstring& Path, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, uint32 AccessMask, uint64 AccessTime, const CFlexGuid& RuleGuid, EEventStatus Status, uint32 TimeOut);
	void OnResourceAccessed(const std::wstring& Path, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, uint32 AccessMask, uint64 AccessTime, const CFlexGuid& RuleGuid, EEventStatus Status, NTSTATUS NtStatus, bool IsDirectory);

	void AddExecLogEntry(const std::shared_ptr<CProgramFile>& pProgram, const CExecLogEntryPtr& pLogEntry);

	NTSTATUS OnInjectionRequest(uint64 Pid, EExecDllMode DllMode, const CFlexGuid& EnclaveGuid, const CFlexGuid& RuleGuid);

	mutable std::recursive_mutex m_Mutex;

	bool m_bLogNotFound = false;
	bool m_bLogInvalid = false;
	bool m_bLogPipes = false;

	class CServiceList* m_Services;

	std::map<SProcessUID, CProcessPtr> m_ProcessMap;
	std::map<uint64, CProcessPtr> m_ProcessByPID;
	std::multimap<std::wstring, CProcessPtr> m_Apps;
	std::multimap<std::wstring, CProcessPtr> m_ByPath;

	friend DWORD CALLBACK CProcessList__LoadProc(LPVOID lpThreadParameter);
	friend DWORD CALLBACK CProcessList__StoreProc(LPVOID lpThreadParameter);
	HANDLE					m_hStoreThread = NULL;

	std::mutex m_WaitingEventsMutex;
	struct SWaitingEvent
	{
		std::condition_variable cv;
		EAccessRuleType Action = EAccessRuleType::eNone;
	};
	std::map<uint64, SWaitingEvent*> m_WaitingEvents;
};