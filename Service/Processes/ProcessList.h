#pragma once
#include "../Library/Status.h"
#include "Process.h"
#include "Programs/ProgramID.h"
#include "ExecLogEntry.h"
#include "GpuMonitor.h"

//
// For SProcessInfoPtr, which MakeProcess hands back.
//
#include "../Library/API/DriverAPI.h"

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

	// takes one SystemProcessInformation snapshot per sample interval and books the CPU / I/O deltas, see UsageStats.h
	void UpdateUsage();

	//
	// The usage records are loaded on the load thread, after the ingress records, while the
	// service is already up and answering. A client that asks before they are in gets the
	// current minute, keeps that as its watermark and never asks for the older buckets again,
	// so usage queries are refused until the load is through (see SVC_API_GET_USAGE_STATS).
	//
	bool IsUsageLoaded() const { return m_bUsageLoaded; }

	void Reconfigure();

	STATUS EnumProcesses();

	std::map<SProcessUID, CProcessPtr> List() { std::shared_lock Lock(m_Mutex); return m_ProcessMap; }
	enum EGetMode
	{
		eCanNotAdd = 0, // if process is not listed return null
		eCanAdd, // if process is not listed try to add it when its running, else return null
		eMustAdd // always add process even if its not running
	};
	//
	// Lookup by pid with the pid collision check, for callers that already hold the lock.
	// Returns the newest process known under the pid; it may be one that has terminated,
	// kept listed for its sockets, so callers that need a running one check IsMarkedForRemoval.
	//
	// NotAfterMs, when given, is the time of the event being attributed: only a process created
	// by then can be meant, so one that was handed the pid later is skipped (firewall log events
	// arrive seconds late, see CFirewall::ProcessFwEvent).
	CProcessPtr FindByPidUnsafe(uint64 Pid, const std::wstring& FileNameNt, bool bLogCollision, uint64 NotAfterMs = 0);

	//
	// As GetProcessEx, for callers that already hold the lock. See the note there.
	//
	CProcessPtr GetProcessExUnsafe(uint64 Pid, const std::wstring& FileNameNt, EGetMode Mode, uint64 NotAfterMs = 0);
	CProcessPtr GetProcessUnsafe(uint64 Pid, bool bCanAdd = false) { return GetProcessExUnsafe(Pid, L"", bCanAdd ? eMustAdd : eCanNotAdd); }

	CProcessPtr MakeProcess(uint64 Pid, const std::wstring& FileNameNt, SProcessInfoPtr& Info, bool& bOk, uint64 NotAfterMs = 0);
	CProcessPtr InsertProcessUnsafe(const CProcessPtr& pProcess, bool bOk, const SProcessInfoPtr& Info, EGetMode Mode);
	CProcessPtr ResolveProcessUnsafe(const CProcessPtr& pFound, const CProcessPtr& pProcess, bool bOk, const SProcessInfoPtr& Info, EGetMode Mode);

	CProcessPtr GetProcessEx(uint64 Pid, const std::wstring& FileNameNt, EGetMode Mode, uint64 NotAfterMs = 0);
	CProcessPtr GetProcess(uint64 Pid, bool bCanAdd = false) { return GetProcessEx(Pid, L"", bCanAdd ? eMustAdd : eCanNotAdd); }
	CProcessPtr GetProcess(uint64 Pid, const std::wstring& FileNameNt, bool bCanAdd = false, uint64 NotAfterMs = 0) { return GetProcessEx(Pid, FileNameNt, bCanAdd ? eMustAdd : eCanNotAdd, NotAfterMs); }

	std::map<SProcessUID, CProcessPtr> FindProcesses(const CProgramID& ID);

	class CServiceList* Services() { return m_Services; }

	STATUS SetAccessEventAction(uint64 EventId, EAccessRuleType Action);

protected:

	void AddProcessImpl(const CProcessPtr& pProcess);

	STATUS LoadIngress();
	STATUS StoreIngress();
	STATUS LoadUsage();
	STATUS StoreUsage();

	uint64 m_LastUsageSample = 0;
	std::atomic<bool> m_bUsageLoaded{ false };
	STATUS LoadUsageImpl();
	CGpuMonitor m_GpuMonitor;
	uint64 m_GpuMonitorRetry = 0;	// tick of the last Init attempt, 0 = never tried

	//
	// The slow part of taking on a new process, which must not hold the lock.
	// See the note in EnumProcesses.
	//
	void LoadProcessLibs(const CProcessPtr& pProcess);
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

	//
	// Shared rather than exclusive, and no longer recursive.
	//
	// Nearly every driver event looks a process up by pid and nothing more, and
	// while this was exclusive each of those excluded all the others: it was the
	// single largest thing the event threads were found waiting on, by a wide
	// margin, and none of them was waiting to write.
	//
	// A shared_mutex is not recursive, so anything that already holds it must use
	// the Unsafe form of whatever it calls - see GetProcessExUnsafe and
	// CServiceList::GetServiceUnsafe. Taking it twice on one thread deadlocks
	// outright rather than quietly working, which is the point.
	//
	mutable std::shared_mutex m_Mutex;

	bool m_bLogNotFound = false;
	bool m_bLogInvalid = false;
	bool m_bLogPipes = false;

	class CServiceList* m_Services;

	//
	// Keyed by the process UID, which orders by pid first and creation time second (see
	// SProcessUID), so all processes that ever had a pid are adjacent, newest last. That
	// is what the pid lookups use; a separate pid table, one entry per pid, went stale
	// once a terminated process was kept for its sockets and its number handed on.
	//
	std::map<SProcessUID, CProcessPtr> m_ProcessMap;
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