#pragma once
#include "AppPackage.h"

#include "../Processes/Process.h"
#include "TraceLogEntry.h"
#include "../Network/Firewall/FirewallDefs.h"
#include "../Network/TrafficLog.h"
#include "ProgramLibrary.h"
#include "..\Processes\ExecLogEntry.h"
#include "../Access/AccessTree.h"
#include "../Processes/AccessLog.h"

typedef std::wstring		TFilePath;	// normalized Application Binary file apth

class CProgramFile: public CProgramSet
{
	TRACK_OBJECT(CProgramFile)
public:
	CProgramFile(const std::wstring& FileName);

	virtual EProgramType GetType() const { return EProgramType::eProgramFile; }

	virtual void AddProcess(const CProcessPtr& pProcess);
	virtual void RemoveProcess(const CProcessPtr& pProcess);

	virtual std::map<uint64, CProcessPtr> GetProcesses() const	{ std::unique_lock lock(m_Mutex); return m_Processes; }
	virtual std::wstring GetPath() const						{ std::unique_lock lock(m_Mutex); return m_Path; }

	virtual CAppPackagePtr GetAppPackage() const;
	virtual std::list<std::shared_ptr<class CWindowsService>> GetAllServices() const;
	virtual std::shared_ptr<class CWindowsService> GetService(const std::wstring& SvcTag) const;

	virtual void UpdateSignInfo(KPH_VERIFY_AUTHORITY SignAuthority, uint32 SignLevel, uint32 SignPolicy);
	virtual void AddLibrary(const CProgramLibraryPtr& pLibrary, uint64 LoadTime, KPH_VERIFY_AUTHORITY SignAuthority, uint32 SignLevel, uint32 SignPolicy, EEventStatus Status);
	//virtual std::map<uint64, SLibraryInfo> GetLibraries() const { std::unique_lock lock(m_Mutex); return m_Libraries; }
	virtual CVariant DumpLibraries() const;

	virtual CVariant StoreLibraries(const SVarWriteOpt& Opts) const;
	virtual void LoadLibraries(const CVariant& Data);

	virtual void AddExecActor(const std::shared_ptr<CProgramFile>& pActorProgram, const std::wstring& ActorServiceTag, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	virtual void AddExecTarget(const std::wstring& ActorServiceTag, const std::shared_ptr<CProgramFile>& pProgram, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	virtual CVariant DumpExecStats() const;

	virtual void AddIngressActor(const std::shared_ptr<CProgramFile>& pActorProgram, const std::wstring& ActorServiceTag, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	virtual void AddIngressTarget(const std::wstring& ActorServiceTag, const std::shared_ptr<CProgramFile>& pProgram, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	virtual CVariant DumpIngress() const;

	virtual void AddAccess(const std::wstring& ActorServiceTag, const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked);
	virtual CVariant DumpAccess(uint64 LastActivity) const;

	virtual CVariant StoreAccess(const SVarWriteOpt& Opts) const;
	virtual void LoadAccess(const CVariant& Data);

	virtual void UpdateLastFwActivity(uint64 TimeStamp, bool bBlocked);

	virtual CTrafficLog* TrafficLog()							{ return &m_TrafficLog; }

	virtual CVariant StoreTraffic(const SVarWriteOpt& Opts) const;
	virtual void LoadTraffic(const CVariant& Data);

	struct STraceLog
	{
		TRACK_OBJECT(STraceLog)
		std::vector<CTraceLogEntryPtr> Entries;
		size_t IndexOffset = 0;
	};

	virtual uint64 AddTraceLogEntry(const CTraceLogEntryPtr& pLogEntry, ETraceLogs Log);
	virtual STraceLog GetTraceLog(ETraceLogs Log) const;
	virtual void CleanupTraceLog();
	virtual void ClearTraceLog();

	virtual void ClearLogs();

protected:
	friend class CProgramManager;

	void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const CVariant& Data) override;
	void ReadMValue(const SVarName& Name, const CVariant& Data) override;

	std::wstring					m_Path;

	SLibraryInfo::USign				m_SignInfo;

	//
	// Note: A program file can have multiple running processes
	// but a process always has one unique program file, 
	// see CProcess::m_pFileRef for details
	//

	uint64							m_LastExec = 0;

	std::map<uint64, CProcessPtr>	m_Processes; // key: PID

	std::map<uint64, SLibraryInfo>	m_Libraries; // key: Librars UUID

	CAccessLog						m_AccessLog;

	CAccessTree						m_AccessTree;

	CTrafficLog						m_TrafficLog;

	uint64							m_LastFwAllowed = 0;
	uint64							m_LastFwBlocked = 0;

	//
	// Note: log entries don't have mutexes, we thread them as read only objects
	// thay must not be modificed after being added to the list !!!
	//

	STraceLog	m_TraceLogs[(int)ETraceLogs::eLogMax];

private:
	struct SStats
	{
		std::set<uint64> Pids;

		std::set<uint64> SocketRefs;

		uint64 LastNetActivity = 0;

		uint64 LastFwAllowed = 0;
		uint64 LastFwBlocked = 0;

		uint64 Upload = 0;
		uint64 Download = 0;
		uint64 Uploaded = 0;
		uint64 Downloaded = 0;
	};

	void CollectStats(SStats& Stats) const;
};

typedef std::shared_ptr<CProgramFile> CProgramFilePtr;
typedef std::weak_ptr<CProgramFile> CProgramFileRef;