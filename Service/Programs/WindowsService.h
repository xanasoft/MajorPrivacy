#pragma once
#include "ProgramItem.h"
#include "../Processes/Process.h"
#include "../Network/TrafficLog.h"
#include "ProgramFile.h"

class CWindowsService : public CProgramItem
{
public:
	CWindowsService(const std::wstring& ServiceTag);

	virtual EProgramType GetType() const { return EProgramType::eWindowsService; }

	virtual std::wstring GetServiceTag() const { std::unique_lock lock(m_Mutex); return m_ServiceTag; }

	virtual CProgramFilePtr GetProgramFile() const;

	virtual void SetProcess(const CProcessPtr& pProcess);

	virtual void AddExecChild(
		const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
		const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	//virtual std::map<uint64, CAccessLog::SExecInfo> GetExecChildren() const { std::unique_lock lock(m_Mutex); return m_AccessLog.GetExecChildren(); }
	virtual CVariant DumpExecStats() const;

	virtual void AddIngressTarget(
		const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
		bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	//virtual std::map<uint64, CAccessLog::SAccessInfo> GetIngressTargets() const { std::unique_lock lock(m_Mutex); return m_AccessLog.GetIngressTargets(); }
	virtual CVariant DumpIngress() const;

	virtual void AddAccess(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked);
	virtual CVariant DumpResAccess(uint64 LastActivity) const;

	virtual CVariant StoreIngress(const SVarWriteOpt& Opts) const;
	virtual void LoadIngress(const CVariant& Data);

	virtual CVariant StoreAccess(const SVarWriteOpt& Opts) const;
	virtual void LoadAccess(const CVariant& Data);

	virtual void UpdateLastFwActivity(uint64 TimeStamp, bool bBlocked);

	virtual CTrafficLog* TrafficLog() { return &m_TrafficLog; }

	virtual CVariant StoreTraffic(const SVarWriteOpt& Opts) const;
	virtual void LoadTraffic(const CVariant& Data);

	virtual void ClearLogs(ETraceLogs Log);

	virtual void TruncateAccessLog();

	virtual void CleanUpAccessTree(bool* pbCancel, uint32* puCounter);
	virtual void TruncateAccessTree();
	virtual uint32 GetAccessCount() const { return m_AccessTree.GetAccessCount(); }

	virtual bool IsMissing() const { std::unique_lock lock(m_Mutex); return m_IsMissing != ePresent; }

protected:
	friend class CProgramFile;

	void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const CVariant& Data) override;
	void ReadMValue(const SVarName& Name, const CVariant& Data) override;

	std::wstring					m_ServiceTag;

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

	virtual void StoreExecChildren(CVariant& ExecChildren, const SVarWriteOpt& Opts, const std::wstring& SvcTag = L"") const;
	virtual bool LoadExecChildren(const CVariant& ExecChildren);
	virtual void StoreIngressTargets(CVariant& IngressTargets, const SVarWriteOpt& Opts, const std::wstring& SvcTag = L"") const;
	virtual bool LoadIngressTargets(const CVariant& IngressTargets);

private:
	struct SStats
	{
		std::set<uint64> SocketRefs;

		uint64 LastNetActivity = 0;

		uint64 Upload = 0;
		uint64 Download = 0;
		uint64 Uploaded = 0;
		uint64 Downloaded = 0;
	};

	void CollectStats(SStats& Stats) const;
};

typedef std::shared_ptr<CWindowsService> CWindowsServicePtr;
typedef std::weak_ptr<CWindowsService> CWindowsServiceRef;