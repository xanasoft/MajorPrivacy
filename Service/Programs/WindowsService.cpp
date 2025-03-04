#include "pch.h"
#include "WindowsService.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Library/Common/Strings.h"

CWindowsService::CWindowsService(const std::wstring& ServiceTag)
{
	m_ID = CProgramID(MkLower(ServiceTag), EProgramType::eWindowsService);
	m_ServiceTag = ServiceTag;
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

CVariant CWindowsService::DumpExecStats() const
{
	SVarWriteOpt Opts;

	CVariant ExecChildren;
	ExecChildren.BeginList();
	StoreExecChildren(ExecChildren, Opts);
	ExecChildren.Finish();

	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROG_EXEC_CHILDREN, ExecChildren);
	Data.Finish();
	return Data;
}

void CWindowsService::AddIngressTarget(
	const std::shared_ptr<CProgramFile>& pTargetProgram, const CFlexGuid& TargetEnclave, const SProcessUID& ProcessUID, 
	bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	m_AccessLog.AddIngressTarget(pTargetProgram->GetUID(), TargetEnclave, ProcessUID, bThread, AccessMask, AccessTime, bBlocked);
}

CVariant CWindowsService::DumpIngress() const
{
	SVarWriteOpt Opts;

	CVariant IngressTargets;
	IngressTargets.BeginList();
	StoreIngressTargets(IngressTargets, Opts);
	IngressTargets.Finish();

	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROG_INGRESS_TARGETS, IngressTargets);
	Data.Finish();
	return Data;
}

void CWindowsService::AddAccess(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked)
{
	m_AccessTree.Add(Path, AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked);
}

void CWindowsService::StoreExecChildren(CVariant& ExecChildren, const SVarWriteOpt& Opts, const std::wstring& SvcTag) const
{
	m_AccessLog.StoreAllExecChildren(ExecChildren, CFlexGuid(), Opts, SvcTag);
}

bool CWindowsService::LoadExecChildren(const CVariant& ExecChildren)
{
	ExecChildren.ReadRawList([&](const CVariant& vData) {
		m_AccessLog.LoadExecChild(vData);
	});
	return true;
}

void CWindowsService::StoreIngressTargets(CVariant& IngressTargets, const SVarWriteOpt& Opts, const std::wstring& SvcTag) const
{
	m_AccessLog.StoreAllIngressTargets(IngressTargets, CFlexGuid(), Opts, SvcTag);
}

bool CWindowsService::LoadIngressTargets(const CVariant& IngressTargets)
{
	IngressTargets.ReadRawList([&](const CVariant& vData) {
		m_AccessLog.LoadIngressTarget(vData);
	});
	return true;
}

CVariant CWindowsService::StoreIngress(const SVarWriteOpt& Opts) const
{
	CVariant ExecChildren;
	ExecChildren.BeginList();
	StoreExecChildren(ExecChildren, Opts);
	ExecChildren.Finish();

	CVariant IngressTargets;
	IngressTargets.BeginList();
	StoreIngressTargets(IngressTargets, Opts);
	IngressTargets.Finish();

	CVariant Data;
	Data[API_V_PROG_ID] = m_ID.ToVariant(Opts);
	Data[API_V_PROG_EXEC_CHILDREN] = ExecChildren;
	Data[API_V_PROG_INGRESS_TARGETS] = IngressTargets;
	return Data;
}

void CWindowsService::LoadIngress(const CVariant& Data)
{
	LoadExecChildren(Data[API_V_PROG_EXEC_CHILDREN]);
	LoadIngressTargets(Data[API_V_PROG_INGRESS_TARGETS]);
}

CVariant CWindowsService::StoreAccess(const SVarWriteOpt& Opts) const
{
	CVariant Data;
	Data[API_V_PROG_ID] = m_ID.ToVariant(Opts);
	Data[API_V_PROG_RESOURCE_ACCESS] = m_AccessTree.StoreTree(Opts);
	return Data;
}

void CWindowsService::LoadAccess(const CVariant& Data)
{
	m_AccessTree.LoadTree(Data[API_V_PROG_RESOURCE_ACCESS]);
}

CVariant CWindowsService::DumpResAccess(uint64 LastActivity) const
{
	return m_AccessTree.DumpTree(LastActivity);
}

CVariant CWindowsService::StoreTraffic(const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex);

	CVariant Data;
	Data[API_V_PROG_ID] = m_ID.ToVariant(Opts);
	Data[API_V_TRAFFIC_LOG] = m_TrafficLog.StoreTraffic(Opts);
	return Data;

}

void CWindowsService::LoadTraffic(const CVariant& Data)
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