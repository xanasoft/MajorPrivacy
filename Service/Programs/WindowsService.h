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
	virtual std::map<uint64, CProgramFile::SExecInfo> GetExecTargets() const { std::unique_lock lock(m_Mutex); return m_ExecTargets; }

	virtual void AddIngressTarget(const std::shared_ptr<CProgramFile>& pProgram, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	virtual std::map<uint64, CProgramFile::SAccessInfo> GetIngressTargets() const { std::unique_lock lock(m_Mutex); return m_IngressTargets; }

	virtual void AddAccess(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	virtual CVariant DumpAccess() const;

	virtual CTrafficLog* TrafficLog() { return &m_TrafficLog; }

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

	CProcessPtr						m_pProcess;

	std::map<uint64, CProgramFile::SExecInfo>	m_ExecTargets; // key: Program UUID

	std::map<uint64, CProgramFile::SAccessInfo>	m_IngressTargets; // key: Program UUID

	CAccessTree						m_AccessTree;

	CTrafficLog						m_TrafficLog;

private:
	struct SStats
	{
		std::set<uint64> SocketRefs;
		uint64 LastActivity = 0;
		uint64 Upload = 0;
		uint64 Download = 0;
		uint64 Uploaded = 0;
		uint64 Downloaded = 0;
	};

	void CollectStats(SStats& Stats) const;
};

typedef std::shared_ptr<CWindowsService> CWindowsServicePtr;
typedef std::weak_ptr<CWindowsService> CWindowsServiceRef;