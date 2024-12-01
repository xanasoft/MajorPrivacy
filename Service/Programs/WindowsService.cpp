#include "pch.h"
#include "WindowsService.h"
#include "../../Library/API/PrivacyAPI.h"

CWindowsService::CWindowsService(const TServiceId& Id)
{
	m_ID.Set(EProgramType::eWindowsService, Id);
	m_ServiceId = Id;
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

void CWindowsService::AddExecTarget(const std::shared_ptr<CProgramFile>& pProgram, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	m_AccessLog.AddExecTarget(pProgram->GetUID(), CmdLine, CreateTime, bBlocked);
}

CVariant CWindowsService::DumpExecStats() const
{
	SVarWriteOpt Opts;

	CVariant Targets;
	Targets.BeginList();
	m_AccessLog.DumpIngressTargets(Targets, Opts);
	Targets.Finish();

	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROC_TARGETS, Targets);
	Data.Finish();
	return Data;
}

void CWindowsService::AddIngressTarget(const std::shared_ptr<CProgramFile>& pProgram, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	m_AccessLog.AddIngressTarget(pProgram->GetUID(), bThread, AccessMask, AccessTime, bBlocked);
}

CVariant CWindowsService::DumpIngress() const
{
	SVarWriteOpt Opts;

	CVariant Targets;
	Targets.BeginList();
	m_AccessLog.DumpExecTarget(Targets, Opts);
	Targets.Finish();

	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROC_TARGETS, Targets);
	Data.Finish();
	return Data;
}

void CWindowsService::AddAccess(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked)
{
	m_AccessTree.Add(Path, AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked);
}

CVariant CWindowsService::StoreAccess(const SVarWriteOpt& Opts) const
{
	CVariant Data;
	Data[API_V_PROG_ID] = m_ID.ToVariant(Opts);
	Data[API_V_PROG_RESOURCE_ACCESS] = m_AccessTree.StoreTree(Opts);
	Data[API_V_PROG_EXEC_TARGETS] = m_AccessLog.StoreExecTargets(Opts);
	Data[API_V_PROG_INGRESS_TARGETS] = m_AccessLog.StoreIngressTargets(Opts);
	return Data;
}

void CWindowsService::LoadAccess(const CVariant& Data)
{
	m_AccessTree.LoadTree(Data[API_V_PROG_RESOURCE_ACCESS]);
	m_AccessLog.LoadExecTargets(Data[API_V_PROG_EXEC_TARGETS]);
	m_AccessLog.LoadIngressTargets(Data[API_V_PROG_INGRESS_TARGETS]);
}

CVariant CWindowsService::DumpAccess(uint64 LastActivity) const
{
	return m_AccessTree.DumpTree(LastActivity);
}

CVariant CWindowsService::StoreTraffic(const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex);

	CVariant Data;
	Data[API_V_PROG_ID] = m_ID.ToVariant(Opts);
	Data[API_V_PROG_TRAFFIC] = m_TrafficLog.StoreTraffic(Opts);
	return Data;

}

void CWindowsService::LoadTraffic(const CVariant& Data)
{
	std::unique_lock lock(m_Mutex);

	m_TrafficLog.LoadTraffic(Data[API_V_PROG_TRAFFIC]);
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
			if (_wcsicmp(pSocket->GetOwnerServiceName().c_str(), m_ServiceId.c_str()) != 0)
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