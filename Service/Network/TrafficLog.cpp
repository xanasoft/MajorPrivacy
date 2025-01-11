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
	
	if (Data.LastActivity > m_LastActivity)
		m_LastActivity = Data.LastActivity;
	m_Uploaded += Data.Uploaded;
	m_Downloaded += Data.Downloaded;

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
	if (pEntry->IpAddress.IsNull())
		pEntry->IpAddress = pSocket->GetRemoteAddress();
	//STrafficLogEntryPtr& pEntry = m_TrafficLog[Host];
	//if(!pEntry) pEntry = std::make_shared<STrafficLogEntry>(Host);
	pEntry->Merge(Data);
}

void CTrafficLog::Clear()
{
	std::unique_lock Lock(m_Mutex);

	m_TrafficLog.clear();

	m_LastActivity = 0;
	m_Uploaded = 0;
	m_Downloaded = 0;
}

CVariant CTrafficLog::StoreTraffic(const SVarWriteOpt& Opts) const
{
	std::shared_lock Lock(m_Mutex);

	CVariant TrafficLog;
	TrafficLog.BeginList();

	for (auto I : m_TrafficLog)
		TrafficLog.WriteVariant(I.second.ToVariant(I.first));

	TrafficLog.Finish();
	return TrafficLog;
}

void CTrafficLog::LoadTraffic(const CVariant& Data)
{
	std::unique_lock Lock(m_Mutex);

	for (uint32 i = 0; i < Data.Count(); i++)
	{
		const CVariant& Entry = Data[i];
		STrafficLogEntry &Data = m_TrafficLog[Entry[API_V_SOCK_RHOST].AsStr()];
		Data.IpAddress.FromString(Entry[API_V_SOCK_RADDR]);
		Data.LastActivity = Entry[API_V_SOCK_LAST_NET_ACT];
		Data.Uploaded = Entry[API_V_SOCK_UPLOADED];
		Data.Downloaded = Entry[API_V_SOCK_DOWNLOADED];
	}
}

CVariant CTrafficLog::STrafficLogEntry::ToVariant(const std::wstring& Host) const
{
	CVariant Entry;
	Entry.BeginIMap();

	Entry.Write(API_V_SOCK_RHOST, Host);
	Entry.Write(API_V_SOCK_RADDR, IpAddress.ToString());
	Entry.Write(API_V_SOCK_LAST_NET_ACT, LastActivity);
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