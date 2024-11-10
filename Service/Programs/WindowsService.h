#pragma once
#include "ProgramItem.h"
#include "../Processes/Process.h"
#include "../Network/TrafficLog.h"
#include "ProgramFile.h"

typedef std::wstring		TServiceId;	// Service name as string

class CWindowsService : public CProgramItem
{
public:
	CWindowsService(const TServiceId& Id);

	virtual EProgramType GetType() const { return EProgramType::eWindowsService; }

	virtual TServiceId GetSvcTag() const { std::unique_lock lock(m_Mutex); return m_ServiceId; }

	virtual void SetProcess(const CProcessPtr& pProcess);

	virtual void AddExecTarget(const std::shared_ptr<CProgramFile>& pProgram, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	//virtual std::map<uint64, CAccessLog::SExecInfo> GetExecTargets() const { std::unique_lock lock(m_Mutex); return m_AccessLog.GetExecTargets(); }
	virtual CVariant DumpExecStats() const;

	virtual void AddIngressTarget(const std::shared_ptr<CProgramFile>& pProgram, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	//virtual std::map<uint64, CAccessLog::SAccessInfo> GetIngressTargets() const { std::unique_lock lock(m_Mutex); return m_AccessLog.GetIngressTargets(); }
	virtual CVariant DumpIngress() const;

	virtual void AddAccess(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked);
	virtual CVariant StoreAccess(const SVarWriteOpt& Opts) const;
	virtual void LoadAccess(const CVariant& Data);
	virtual CVariant DumpAccess(uint64 LastActivity) const;

	virtual void UpdateLastFwActivity(uint64 TimeStamp, bool bBlocked);

	virtual CTrafficLog* TrafficLog() { return &m_TrafficLog; }

	virtual CVariant StoreTraffic(const SVarWriteOpt& Opts) const;
	virtual void LoadTraffic(const CVariant& Data);

	virtual void ClearLogs();

protected:

	void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const CVariant& Data) override;
	void ReadMValue(const SVarName& Name, const CVariant& Data) override;

	TServiceId						m_ServiceId;

	//
	// Note: A service can have only one process,
	// howeever a process can host multiple services
	//

#ifdef _DEBUG
public:
#endif

	uint64							m_LastExec = 0;

	CProcessPtr						m_pProcess;

	CAccessLog						m_AccessLog;

	CAccessTree						m_AccessTree;

	CTrafficLog						m_TrafficLog;

	uint64							m_LastFwAllowed = 0;
	uint64							m_LastFwBlocked = 0;

private:
	struct SStats
	{
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

typedef std::shared_ptr<CWindowsService> CWindowsServicePtr;
typedef std::weak_ptr<CWindowsService> CWindowsServiceRef;