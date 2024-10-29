#include "pch.h"
#include "WindowsService.h"
#include "../../Library/API/PrivacyAPI.h"

CWindowsService::CWindowsService(const TServiceId& Id)
{
	m_ID.Set(EProgramType::eWindowsService, Id);
	m_ServiceId = Id;
}

void CWindowsService::SetProcess(const CProcessPtr& pProcess)
{
	std::unique_lock lock(m_Mutex);

	m_pProcess = pProcess;
}

void CWindowsService::AddExecTarget(const std::shared_ptr<CProgramFile>& pProgram, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	CProgramFile::SExecInfo& Info = m_ExecTargets[pProgram->GetUID()];
	Info.bBlocked = bBlocked;
	Info.LastExecTime = CreateTime;
	Info.CommandLine = CmdLine;
}

CVariant CWindowsService::DumpExecStats() const
{
	std::unique_lock lock(m_Mutex);

	CVariant Targets;
	Targets.BeginList();
	for (auto& pItem : m_ExecTargets)
	{
		CVariant vTarget;
		vTarget.BeginIMap();
		vTarget.Write(API_V_PROC_REF, (uint64)&pItem.second);
		vTarget.Write(API_V_PROC_EVENT_TARGET, pItem.first);
		vTarget.Write(API_V_PROC_EVENT_LAST_EXEC, pItem.second.LastExecTime);
		vTarget.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
		vTarget.Write(API_V_PROC_EVENT_CMD_LINE, pItem.second.CommandLine);
		vTarget.Finish();
		Targets.WriteVariant(vTarget);
	}

	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROC_TARGETS, Targets);
	Data.Finish();
	return Data;
}

void CWindowsService::AddIngressTarget(const std::shared_ptr<CProgramFile>& pProgram, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	CProgramFile::SAccessInfo& Info = m_IngressTargets[pProgram->GetUID()];
	Info.bBlocked = bBlocked;
	Info.LastAccessTime = AccessTime;
	if(bThread) Info.ThreadAccessMask |= AccessMask;
	else Info.ProcessAccessMask |= AccessMask;
}

CVariant CWindowsService::DumpIngress() const
{
	std::unique_lock lock(m_Mutex);

	CVariant Targets;
	Targets.BeginList();
	for (auto& pItem : m_IngressTargets)
	{
		CVariant vTarget;
		vTarget.BeginIMap();
		vTarget.Write(API_V_PROC_REF, (uint64)&pItem.second);
		vTarget.Write(API_V_PROC_EVENT_TARGET, pItem.first);
		vTarget.Write(API_V_THREAD_ACCESS_MASK, pItem.second.ThreadAccessMask);
		vTarget.Write(API_V_PROCESS_ACCESS_MASK, pItem.second.ProcessAccessMask);
		vTarget.Write(API_V_PROC_EVENT_LAST_ACCESS, pItem.second.LastAccessTime);
		vTarget.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
		vTarget.Finish();
		Targets.WriteVariant(vTarget);
	}
	Targets.Finish();

	CVariant Data;
	Data.BeginIMap();
	Data.WriteVariant(API_V_PROC_TARGETS, Targets);
	Data.Finish();
	return Data;
}

void CWindowsService::AddAccess(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	m_AccessTree.Add(Path, AccessMask, AccessTime, bBlocked);
}

CVariant CWindowsService::DumpAccess() const
{
	std::unique_lock lock(m_Mutex); 

	return m_AccessTree.DumpTree();
}

void CWindowsService::ClearAccess()
{
	std::unique_lock lock(m_Mutex);

	return m_AccessTree.Clear();
}

void CWindowsService::CollectStats(SStats& Stats) const
{
	Stats.LastActivity = m_TrafficLog.GetLastActivity();
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

			if (pSocket->GetLastActivity() > Stats.LastActivity)
				Stats.LastActivity = pSocket->GetLastActivity();
			Stats.Upload += pSocket->GetUpload();
			Stats.Download += pSocket->GetDownload();
			Stats.Uploaded += pSocket->GetUploaded();
			Stats.Downloaded += pSocket->GetDownloaded();
		}
	}
}

void CWindowsService::ClearLogs()
{
	m_TrafficLog.Clear();

	std::unique_lock lock(m_Mutex);
	m_AccessTree.Clear();

	m_ExecTargets.clear();

	m_IngressTargets.clear();
}