#include "pch.h"
#include "WindowsService.h"
#include "ServiceAPI.h"

CWindowsService::CWindowsService(const TServiceId& Id)
{
	m_ID.Set(CProgramID::eService, Id);
	m_ServiceId = Id;
}

void CWindowsService::SetProcess(const CProcessPtr& pProcess)
{
	std::unique_lock lock(m_Mutex);

	m_pProcess = pProcess;
}

void CWindowsService::WriteVariant(CVariant& Data) const
{
	Data.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_SERVICE);

	CProgramItem::WriteVariant(Data);

	std::set<uint64> SocketRefs;
	uint64 LastActivity = m_TraceLog.GetLastActivity();
	uint64 Upload = 0;
	uint64 Download = 0;
	uint64 Uploaded = m_TraceLog.GetUploaded();
	uint64 Downloaded = m_TraceLog.GetDownloaded();
	if (m_pProcess) 
	{
		std::set<CSocketPtr> Sockets = m_pProcess->GetSocketList();
		for (auto pSocket : Sockets)
		{
			if (_wcsicmp(pSocket->GetOwnerServiceName().c_str(), m_ServiceId.c_str()) != 0)
				continue;

			SocketRefs.insert((uint64)pSocket.get());

			if (pSocket->GetLastActivity() > LastActivity)
				LastActivity = pSocket->GetLastActivity();
			Upload += pSocket->GetUpload();
			Download += pSocket->GetDownload();
			Uploaded += pSocket->GetUploaded();
			Downloaded += pSocket->GetDownloaded();
		}
	}

	Data.Write(SVC_API_PROG_SVC_TAG, m_ServiceId);

	Data.Write(SVC_API_PROG_PROC_PID, m_pProcess ? m_pProcess->GetProcessId() : 0);
	
	Data.Write(SVC_API_PROG_SOCKETS, SocketRefs);

	Data.Write(SVC_API_SOCK_LAST_ACT, LastActivity);
	Data.Write(SVC_API_SOCK_UPLOAD, Upload);
	Data.Write(SVC_API_SOCK_DOWNLOAD, Download);
	Data.Write(SVC_API_SOCK_UPLOADED, Uploaded);
	Data.Write(SVC_API_SOCK_DOWNLOADED, Downloaded);
}