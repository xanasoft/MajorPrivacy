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
	virtual std::wstring GetNameEx() const;
	virtual std::wstring GetIcon() const						{ std::unique_lock lock(m_Mutex); return m_IconFile.empty() ? m_Path : m_IconFile; } // use program file for icon when no icon is set

	virtual CAppPackagePtr GetAppPackage() const;
	virtual std::list<std::shared_ptr<class CWindowsService>> GetAllServices() const;
	virtual std::shared_ptr<class CWindowsService> GetService(const std::wstring& SvcTag) const;

	virtual void UpdateSignInfo(const struct SVerifierInfo* pVerifyInfo, bool bUpdateProcesses = false);
	virtual bool HashInfoUnknown() const;
	virtual bool HashInfoUnknown(const CProgramLibraryPtr& pLibrary) const;
	virtual CImageSignInfo GetSignInfo() const { std::unique_lock lock(m_Mutex); return m_SignInfo; }
	virtual void AddLibrary(const CFlexGuid& EnclaveGuid, const CProgramLibraryPtr& pLibrary, uint64 LoadTime = 0, const struct SVerifierInfo* pVerifyInfo = NULL, EEventStatus Status = EEventStatus::eUndefined);
	virtual std::map<uint64, SLibraryInfo> GetLibraries() const { std::unique_lock lock(m_Mutex); return m_Libraries; }
	//virtual SLibraryInfo GetLibrary(uint64 UID) const { std::unique_lock lock(m_Mutex); auto it = m_Libraries.find(UID); return it != m_Libraries.end() ? it->second : SLibraryInfo(); }
	
	
	virtual CVariant DumpLibraries() const;
	virtual CVariant StoreLibraries(const SVarWriteOpt& Opts) const;
	virtual void LoadLibraries(const CVariant& Data);
	virtual void CleanUpLibraries();

	virtual void AddExecParent(const CFlexGuid& TargetEnclave, 
		const std::shared_ptr<CProgramFile>& pActorProgram, const CFlexGuid& ActorEnclave, const SProcessUID& ProcessUID, const std::wstring& ActorServiceTag, 
		const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	virtual void AddExecChild(const CFlexGuid& ActorEnclave, const std::wstring& ActorServiceTag, 
		const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
		const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	virtual CVariant DumpExecStats() const;

	virtual void AddIngressActor(const CFlexGuid& TargetEnclave, 
		const std::shared_ptr<CProgramFile>& pActorProgram, const CFlexGuid& ActorEnclave, const SProcessUID& ProcessUID, const std::wstring& ActorServiceTag, 
		bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	virtual void AddIngressTarget(const CFlexGuid& ActorEnclave, const std::wstring& ActorServiceTag, 
		const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
		bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	virtual CVariant DumpIngress() const;

	virtual void AddAccess(const std::wstring& ActorServiceTag, const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked);
	virtual CVariant DumpResAccess(uint64 LastActivity) const;

	virtual CVariant StoreAccessLog(const SVarWriteOpt& Opts) const;
	virtual void LoadAccessLog(const CVariant& Data);

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
	virtual void TruncateTraceLog();
	virtual void ClearTraceLog(ETraceLogs Log);

	virtual void ClearLogs(ETraceLogs Log);

	virtual void TruncateAccessLog();

	virtual void CleanUpAccessTree(bool* pbCancel, uint32* puCounter);
	virtual void TruncateAccessTree();
	virtual uint32 GetAccessCount() const { return m_AccessTree.GetAccessCount(); }

	virtual void TestMissing();

protected:
	friend class CProgramManager;

	void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const CVariant& Data) override;
	void ReadMValue(const SVarName& Name, const CVariant& Data) override;

	std::wstring					m_Path;

	CImageSignInfo					m_SignInfo;
	
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

	struct SEnclaveRecord
	{
		std::map<uint64, SLibraryInfo> Libraries;

		CAccessLog					AccessLog;
	};

	typedef std::shared_ptr<SEnclaveRecord> SEnclaveRecordPtr;

	SEnclaveRecordPtr GetEnclaveRecord(const CFlexGuid& EnclaveGuid);

	std::map<CFlexGuid, SEnclaveRecordPtr> m_EnclaveRecord;

	virtual void StoreExecParents(CVariant& ExecParents, const SVarWriteOpt& Opts) const;
	virtual bool LoadExecParents(const CVariant& ExecParents);
	virtual void StoreExecChildren(CVariant& ExecChildren, const SVarWriteOpt& Opts) const;
	virtual bool LoadExecChildren(const CVariant& ExecChildren);
	virtual void StoreIngressActors(CVariant& IngressActors, const SVarWriteOpt& Opts) const;
	virtual bool LoadIngressActors(const CVariant& IngressActors);
	virtual void StoreIngressTargets(CVariant& IngressTargets, const SVarWriteOpt& Opts) const;
	virtual bool LoadIngressTargets(const CVariant& IngressTargets);

	//
	// Note: log entries don't have mutexes, we thread them as read only objects
	// thay must not be modificed after being added to the list !!!
	//

	STraceLog						m_TraceLogs[(int)ETraceLogs::eLogMax];

private:
	struct SStats
	{
		std::set<uint64> Pids;

		std::set<uint64> SocketRefs;

		uint64 LastNetActivity = 0;

		uint64 Upload = 0;
		uint64 Download = 0;
		uint64 Uploaded = 0;
		uint64 Downloaded = 0;
	};

	void CollectStats(SStats& Stats) const;
};

typedef std::shared_ptr<CProgramFile> CProgramFilePtr;
typedef std::weak_ptr<CProgramFile> CProgramFileRef;