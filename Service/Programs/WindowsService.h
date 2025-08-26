#pragma once
#include "ProgramItem.h"
#include "../Processes/Process.h"
#include "../Network/TrafficLog.h"
#include "ProgramFile.h"

class CWindowsService : public CProgramItem
{
public:
	CWindowsService(const std::wstring& ServiceTag);
	~CWindowsService();

	virtual EProgramType GetType() const { return EProgramType::eWindowsService; }

	virtual std::wstring GetServiceTag() const { std::unique_lock lock(m_Mutex); return m_ServiceTag; }

	virtual CProgramFilePtr GetProgramFile() const;

	virtual void SetProcess(const CProcessPtr& pProcess);

	virtual void AddExecChild(
		const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
		const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked);
	//virtual std::map<uint64, CAccessLog::SExecInfo> GetExecChildren() const { std::unique_lock lock(m_Mutex); return m_AccessLog.GetExecChildren(); }
	virtual StVariant DumpExecStats(FW::AbstractMemPool* pMemPool = nullptr) const;

	virtual void AddIngressTarget(
		const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
		bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	//virtual std::map<uint64, CAccessLog::SAccessInfo> GetIngressTargets() const { std::unique_lock lock(m_Mutex); return m_AccessLog.GetIngressTargets(); }
	virtual StVariant DumpIngress(FW::AbstractMemPool* pMemPool = nullptr) const;

	virtual void AddAccess(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked);
	virtual StVariant DumpResAccess(uint64 LastActivity, FW::AbstractMemPool* pMemPool = nullptr) const;

	virtual StVariant StoreIngress(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	virtual void LoadIngress(const StVariant& Data);

	virtual StVariant StoreAccess(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	virtual void LoadAccess(const StVariant& Data);

	virtual void UpdateLastFwActivity(uint64 TimeStamp, bool bBlocked);

	virtual CTrafficLog* TrafficLog()							{ return &m_TrafficLog; }

	virtual StVariant StoreTraffic(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	virtual void LoadTraffic(const StVariant& Data);

	virtual void ClearRecords(ETraceLogs Log);

	virtual void TruncateAccessLog();

	virtual void CleanUpAccessTree(bool* pbCancel, uint32* puCounter);
	virtual void TruncateAccessTree();
	virtual uint32 GetAccessCount() const { return m_AccessTree.GetAccessCount(); }

	virtual bool IsMissing() const { std::unique_lock lock(m_Mutex); return m_IsMissing != ePresent; }

	virtual size_t GetLogMemUsage() const;

protected:
	friend class CProgramFile;

	void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const StVariant& Data) override;
	void ReadMValue(const SVarName& Name, const StVariant& Data) override;

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

	virtual void StoreExecChildren(StVariantWriter& ExecChildren, const SVarWriteOpt& Opts, const std::wstring& SvcTag = L"") const;
	virtual bool LoadExecChildren(const StVariant& ExecChildren);
	virtual void StoreIngressTargets(StVariantWriter& IngressTargets, const SVarWriteOpt& Opts, const std::wstring& SvcTag = L"") const;
	virtual bool LoadIngressTargets(const StVariant& IngressTargets);

#ifdef DEF_USE_POOL
	FW::MemoryPool*					m_pMem = nullptr;
#endif

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