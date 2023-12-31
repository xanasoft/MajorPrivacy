#include "pch.h"
#include "ProgramFile.h"
#include "ServiceAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/AppUtil.h"

CProgramFile::CProgramFile(const std::wstring& FileName)
{
	ASSERT(!FileName.empty());
	if (!FileName.empty()) {
		m_ID.Set(CProgramID::eFile, FileName);
		SImageVersionInfoPtr pInfo = GetImageVersionInfoNt(FileName);
		m_Name = pInfo ? pInfo->FileDescription : FileName;
		//m_Icon = NtPathToDosPath(FileName);
		m_Icon = FileName;
	}
	m_FileName = FileName;
}

void CProgramFile::AddProcess(const CProcessPtr& pProcess)
{
	uint64 pid = pProcess->GetProcessId();

	std::unique_lock lock(m_Mutex);

	m_Processes.insert(std::make_pair(pid, pProcess));
}

void CProgramFile::RemoveProcess(const CProcessPtr& pProcess)
{
	uint64 pid = pProcess->GetProcessId();
	uint64 LastActivity = pProcess->GetLastActivity();
	uint64 Uploaded = pProcess->GetUploaded();
	uint64 Downloaded = pProcess->GetDownloaded();

	std::unique_lock lock(m_Mutex);

	m_Processes.erase(pid);
}

CAppPackagePtr CProgramFile::GetAppPackage() const
{
	CAppPackagePtr pApp;
	std::shared_lock lock(m_Mutex); 
	for (auto pGroup : m_Groups) {
		pApp = std::dynamic_pointer_cast<CAppPackage>(pGroup.second.lock());
		if (pApp) // a program file should never belong to more than one app package
			break; 
	}
	return pApp;
}

std::list<CWindowsServicePtr> CProgramFile::GetAllServices() const
{
	std::list<CWindowsServicePtr> Svcs;
	for (auto pNode : m_Nodes) {
		CWindowsServicePtr pSvc = std::dynamic_pointer_cast<CWindowsService>(pNode);
		if (pSvc)
			Svcs.push_back(pSvc);
	}
	return Svcs;
}

CWindowsServicePtr CProgramFile::GetService(const std::wstring& SvcTag) const
{
	for (auto pNode : m_Nodes) {
		CWindowsServicePtr pSvc = std::dynamic_pointer_cast<CWindowsService>(pNode);
		if (pSvc && pSvc->GetSvcTag() == SvcTag)
			return pSvc;
	}
	return nullptr;
}

uint32 CProgramFile::AddTraceLogEntry(const CTraceLogEntryPtr& pLogEntry, ETraceLogs Log)
{
	std::shared_lock lock(m_Mutex); 
	m_TraceLogs[(int)Log].push_back(pLogEntry);
	return (uint32)m_TraceLogs[(int)Log].size() - 1;
}

std::vector<CTraceLogEntryPtr> CProgramFile::GetTraceLog(ETraceLogs Log) const
{ 
	std::shared_lock lock(m_Mutex); 
	return m_TraceLogs[(int)Log]; 
}

void CProgramFile::WriteVariant(CVariant& Data) const
{
	Data.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_FILE);

	CProgramSet::WriteVariant(Data);

	std::set<uint64> Pids;
	std::set<uint64> SocketRefs;
	uint64 LastActivity = m_TraceLog.GetLastActivity();
	uint64 Upload = 0;
	uint64 Download = 0;
	uint64 Uploaded = m_TraceLog.GetUploaded();
	uint64 Downloaded = m_TraceLog.GetDownloaded();
	for (auto I : m_Processes) 
	{
		Pids.insert(I.first);

		if (I.second->GetLastActivity() > LastActivity)
			LastActivity = I.second->GetLastActivity();
		Upload += I.second->GetUpload();
		Download += I.second->GetDownload();
		// Note: this is not the same as the sum of the sockets' upload/download, as it includes data from closed sockets
		//Uploaded += I.second->GetUploaded();
		//Downloaded += I.second->GetDownloaded();

		std::set<CSocketPtr> Sockets = I.second->GetSocketList();
		for (auto pSocket : Sockets) 
		{
			SocketRefs.insert((uint64)pSocket.get());

			//if (pSocket->GetLastActivity() > LastActivity)
			//	LastActivity = pSocket->GetLastActivity();
			//Upload += pSocket->GetUpload();
			//Download += pSocket->GetDownload();
			Uploaded += pSocket->GetUploaded();
			Downloaded += pSocket->GetDownloaded();
		}
	}

	Data.Write(SVC_API_PROG_PROC_PIDS, Pids);

	Data.Write(SVC_API_PROG_FILE, m_FileName);

	Data.Write(SVC_API_PROG_SOCKETS, SocketRefs);

	Data.Write(SVC_API_SOCK_LAST_ACT, LastActivity);
	Data.Write(SVC_API_SOCK_UPLOAD, Upload);
	Data.Write(SVC_API_SOCK_DOWNLOAD, Download);
	Data.Write(SVC_API_SOCK_UPLOADED, Uploaded);
	Data.Write(SVC_API_SOCK_DOWNLOADED, Downloaded);
}