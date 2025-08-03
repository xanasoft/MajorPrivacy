#include "pch.h"
#include "WindowsService.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Library/Common/Strings.h"

CWindowsService::CWindowsService(const std::wstring& ServiceTag)
{
	m_ID = CProgramID(MkLower(ServiceTag), EProgramType::eWindowsService);
	m_ServiceTag = ServiceTag;

#ifdef DEF_USE_POOL
	m_pMem = FW::MemoryPool::Create();

	m_AccessLog.SetPool(m_pMem);
	m_AccessTree.SetPool(m_pMem);
	m_TrafficLog.SetPool(m_pMem);
#endif
}

CWindowsService::~CWindowsService()
{
#ifdef DEF_USE_POOL
	m_AccessLog.Clear();
	m_AccessTree.Clear();
	m_TrafficLog.Clear();

	FW::MemoryPool::Destroy(m_pMem);
#endif
}

CProgramFilePtr CWindowsService::GetProgramFile() const
{
	std::unique_lock lock(m_Mutex);

	CProgramFilePtr pProgram;
	auto I = m_Groups.begin();
	if (I != m_Groups.end())
		pProgram = std::dynamic_pointer_cast<CProgramFile>(I->second.lock());
	return pProgram;
}

void CWindowsService::SetProcess(const CProcessPtr& pProcess)
{
	std::unique_lock lock(m_Mutex);

	if(pProcess)
		m_LastExec = pProcess->GetCreateTimeStamp();
	m_pProcess = pProcess;
}

void CWindowsService::AddExecChild(
	const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
	const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	m_AccessLog.AddExecChild(pTargetProgram->GetUID(), TargetEnclave, ProcessUID, CmdLine, CreateTime, bBlocked);
}

StVariant CWindowsService::DumpExecStats(FW::AbstractMemPool* pMemPool) const
{
	SVarWriteOpt Opts;

	StVariantWriter ExecChildren(pMemPool);
	ExecChildren.BeginList();
	StoreExecChildren(ExecChildren, Opts);

	StVariantWriter Data(pMemPool);
	Data.BeginIndex();
	Data.WriteVariant(API_V_PROG_EXEC_CHILDREN, ExecChildren.Finish());
	return Data.Finish();
}

void CWindowsService::AddIngressTarget(
	const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
	bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	m_AccessLog.AddIngressTarget(pTargetProgram->GetUID(), TargetEnclave, ProcessUID, bThread, AccessMask, AccessTime, bBlocked);
}

StVariant CWindowsService::DumpIngress(FW::AbstractMemPool* pMemPool) const
{
	SVarWriteOpt Opts;

	StVariantWriter IngressTargets(pMemPool);
	IngressTargets.BeginList();
	StoreIngressTargets(IngressTargets, Opts);

	StVariantWriter Data(pMemPool);
	Data.BeginIndex();
	Data.WriteVariant(API_V_PROG_INGRESS_TARGETS, IngressTargets.Finish());
	return Data.Finish();
}

void CWindowsService::AddAccess(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked)
{
	m_AccessTree.Add(Path, AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked);
}

void CWindowsService::StoreExecChildren(StVariantWriter& ExecChildren, const SVarWriteOpt& Opts, const std::wstring& SvcTag) const
{
	m_AccessLog.StoreAllExecChildren(ExecChildren, CFlexGuid(), Opts, SvcTag);
}

bool CWindowsService::LoadExecChildren(const StVariant& ExecChildren)
{
	StVariantReader(ExecChildren).ReadRawList([&](const StVariant& vData) {
		m_AccessLog.LoadExecChild(vData);
	});
	return true;
}

void CWindowsService::StoreIngressTargets(StVariantWriter& IngressTargets, const SVarWriteOpt& Opts, const std::wstring& SvcTag) const
{
	m_AccessLog.StoreAllIngressTargets(IngressTargets, CFlexGuid(), Opts, SvcTag);
}

bool CWindowsService::LoadIngressTargets(const StVariant& IngressTargets)
{
	StVariantReader(IngressTargets).ReadRawList([&](const StVariant& vData) {
		m_AccessLog.LoadIngressTarget(vData);
	});
	return true;
}

StVariant CWindowsService::StoreIngress(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	StVariantWriter ExecChildren(pMemPool);
	ExecChildren.BeginList();
	StoreExecChildren(ExecChildren, Opts);

	StVariantWriter IngressTargets(pMemPool);
	IngressTargets.BeginList();
	StoreIngressTargets(IngressTargets, Opts);

	StVariantWriter Data(pMemPool);
	Data.BeginIndex();
	Data.WriteVariant(API_V_ID, m_ID.ToVariant(Opts, pMemPool));
	Data.WriteVariant(API_V_PROG_EXEC_CHILDREN, ExecChildren.Finish());
	Data.WriteVariant(API_V_PROG_INGRESS_TARGETS, IngressTargets.Finish());
	return Data.Finish();
}

void CWindowsService::LoadIngress(const StVariant& Data)
{
	LoadExecChildren(Data[API_V_PROG_EXEC_CHILDREN]);
	LoadIngressTargets(Data[API_V_PROG_INGRESS_TARGETS]);
}

StVariant CWindowsService::StoreAccess(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	StVariant Data(pMemPool);
	Data[API_V_ID] = m_ID.ToVariant(Opts, pMemPool);
	Data[API_V_PROG_RESOURCE_ACCESS] = m_AccessTree.StoreTree(Opts, pMemPool);
	return Data;
}

void CWindowsService::LoadAccess(const StVariant& Data)
{
	m_AccessTree.LoadTree(Data[API_V_PROG_RESOURCE_ACCESS]);
}

StVariant CWindowsService::DumpResAccess(uint64 LastActivity, FW::AbstractMemPool* pMemPool) const
{
	return m_AccessTree.DumpTree(LastActivity, pMemPool);
}

StVariant CWindowsService::StoreTraffic(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	std::unique_lock lock(m_Mutex);

	StVariant Data(pMemPool);
	Data[API_V_ID] = m_ID.ToVariant(Opts, pMemPool);
	Data[API_V_TRAFFIC_LOG] = m_TrafficLog.StoreTraffic(Opts, pMemPool);
	return Data;

}

void CWindowsService::LoadTraffic(const StVariant& Data)
{
	std::unique_lock lock(m_Mutex);

	m_TrafficLog.LoadTraffic(Data[API_V_TRAFFIC_LOG]);
}

void CWindowsService::UpdateLastFwActivity(uint64 TimeStamp, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	if (bBlocked) {
		if (TimeStamp > m_LastFwBlocked)
			m_LastFwBlocked = TimeStamp;
	}
	else {
		if (TimeStamp > m_LastFwAllowed)
			m_LastFwAllowed = TimeStamp;
	}
}

void CWindowsService::CollectStats(SStats& Stats) const
{
	Stats.LastNetActivity = m_TrafficLog.GetLastActivity();
	Stats.Uploaded = m_TrafficLog.GetUploaded();
	Stats.Downloaded = m_TrafficLog.GetDownloaded();
	if (m_pProcess) 
	{
		std::set<CSocketPtr> Sockets = m_pProcess->GetSocketList();
		for (auto pSocket : Sockets)
		{
			if (_wcsicmp(pSocket->GetOwnerServiceName().c_str(), m_ServiceTag.c_str()) != 0)
				continue;

			Stats.SocketRefs.insert((uint64)pSocket.get());

			uint64 LastActivity = pSocket->GetLastActivity();
			if (LastActivity > Stats.LastNetActivity)
				Stats.LastNetActivity = LastActivity;

			Stats.Upload += pSocket->GetUpload();
			Stats.Download += pSocket->GetDownload();
			Stats.Uploaded += pSocket->GetUploaded();
			Stats.Downloaded += pSocket->GetDownloaded();
		}
	}
}

void CWindowsService::ClearLogs(ETraceLogs Log)
{
	if(Log == ETraceLogs::eLogMax || Log == ETraceLogs::eNetLog)
		m_TrafficLog.Clear();

	if (Log == ETraceLogs::eLogMax || Log == ETraceLogs::eExecLog) {
		m_AccessLog.Clear();
		m_LastExec = 0;
	}

	if(Log == ETraceLogs::eLogMax || Log == ETraceLogs::eResLog)
		m_AccessTree.Clear();

#ifdef DEF_USE_POOL
	m_pMem->CleanUp();
#endif
}

void CWindowsService::TruncateAccessLog()
{
	m_AccessLog.Truncate();
}

void CWindowsService::CleanUpAccessTree(bool* pbCancel, uint32* puCounter)
{
	m_AccessTree.CleanUp(pbCancel, puCounter);
}

void CWindowsService::TruncateAccessTree()
{
	m_AccessTree.Truncate();
}

size_t CWindowsService::GetLogMemUsage() const
{
#ifdef DEF_USE_POOL
	return m_pMem->GetSize();
#else
	return 0;
#endif
}