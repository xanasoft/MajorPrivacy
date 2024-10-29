#include "pch.h"
#include "TrafficLog.h"
#include "../../Library/API/PrivacyAPI.h"

CTrafficLog::CTrafficLog()
{
}

void CTrafficLog::AddSocket(const CSocketPtr& pSocket)
{
	std::unique_lock Lock(m_Mutex);
	m_SocketList.insert(pSocket);
}

bool CTrafficLog::RemoveSocket(const CSocketPtr& pSocket, bool bNoCommit)
{
	std::unique_lock Lock(m_Mutex);
	if (!m_SocketList.erase(pSocket))
		return false;

	STrafficLogEntry Data(pSocket);
	if(bNoCommit || Data.LastActivity == 0)
		return true;
	
	m_Data.Merge(Data);

	CommitTraffic(pSocket, Data, m_TrafficLog);

	return true;
}

CTrafficLog::STrafficLogEntry::STrafficLogEntry(const CSocketPtr& pSocket)
{
	if (pSocket) {
		LastActivity = pSocket->GetLastActivity();
		Uploaded = pSocket->GetUploaded();
		Downloaded = pSocket->GetDownloaded();
	}
}

void CTrafficLog::STrafficLogEntry::Merge(const STrafficLogEntry& Data)
{
	if(Data.LastActivity > LastActivity)
		LastActivity = Data.LastActivity;
	Uploaded += Data.Uploaded;
	Downloaded += Data.Downloaded;
}

void CTrafficLog::CommitTraffic(const CSocketPtr& pSocket, const STrafficLogEntry& Data, std::map<std::wstring, STrafficLogEntry>& TrafficLog)
{
	std::wstring Host;
	CHostNamePtr pHostName = pSocket->GetRemoteHostName();
	if (pHostName) Host = pHostName->ToString(); // may be empty
	if (Host.empty()) Host = pSocket->GetRemoteAddress().ToWString();

	STrafficLogEntry* pEntry = &TrafficLog[Host];
	//STrafficLogEntryPtr& pEntry = m_TrafficLog[Host];
	//if(!pEntry) pEntry = std::make_shared<STrafficLogEntry>(Host);
	pEntry->Merge(Data);
}

void CTrafficLog::Clear()
{
	std::unique_lock Lock(m_Mutex);
	m_TrafficLog.clear();
}

CVariant CTrafficLog::STrafficLogEntry::ToVariant(const std::wstring& Host) const
{
	CVariant Entry;
	Entry.BeginIMap();

	Entry.Write(API_V_SOCK_RHOST, Host);
	Entry.Write(API_V_SOCK_LAST_ACT, LastActivity);
	Entry.Write(API_V_SOCK_UPLOADED, Uploaded);
	Entry.Write(API_V_SOCK_DOWNLOADED, Downloaded);

	Entry.Finish();
	return Entry;
}

CVariant CTrafficLog::ToVariant(uint64 MinLastActivity) const
{
	std::shared_lock Lock(m_Mutex);

	CVariant TrafficLog;
	TrafficLog.BeginList();

	std::map<std::wstring, STrafficLogEntry> Current;
	for (auto I : m_SocketList) {
		STrafficLogEntry Data(I);
		if(Data.LastActivity == 0)
			continue;
		CommitTraffic(I, Data, Current);
	}

	for(auto I: m_TrafficLog)
	{
		auto F = Current.find(I.first);
		if (F != Current.end())
		{
			if(MinLastActivity > F->second.LastActivity)
				continue;
			F->second.Merge(I.second);
			TrafficLog.WriteVariant(F->second.ToVariant(I.first));
			Current.erase(F);
		}
		else 
		{
			if(MinLastActivity > I.second.LastActivity)
				continue;
			TrafficLog.WriteVariant(I.second.ToVariant(I.first));
		}
	}

	for (auto I : Current) 
	{
		if(MinLastActivity > I.second.LastActivity)
			continue;
		TrafficLog.WriteVariant(I.second.ToVariant(I.first));
	}

	TrafficLog.Finish();
	return TrafficLog;
}