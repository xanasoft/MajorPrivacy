#include "pch.h"
#include "TrafficLog.h"
#include "../../Library/API/PrivacyAPI.h"

CTrafficLog::CTrafficLog()
{
}

CTrafficLog::~CTrafficLog()
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

	if (!m_Data) {
#ifdef DEF_USE_POOL
		if(!m_pMem)
			return true;
		m_Data = m_pMem->New<STrafficLog>();
#else
		m_Data = std::make_shared<STrafficLog>();
#endif
	}

	CommitTraffic(pSocket, Data, m_Data->TrafficLog);

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

#ifdef DEF_USE_POOL
void CTrafficLog::CommitTraffic(const CSocketPtr& pSocket, const STrafficLogEntry& Data, FW::Map<FW::StringW, STrafficLogEntry>& TrafficLog)
#else
void CTrafficLog::CommitTraffic(const CSocketPtr& pSocket, const STrafficLogEntry& Data, std::map<std::wstring, STrafficLogEntry>& TrafficLog)
#endif
{
	std::wstring Host;
	CHostNamePtr pHostName = pSocket->GetRemoteHostName();
	if (pHostName) Host = pHostName->ToString(); // may be empty
	if (Host.empty()) Host = pSocket->GetRemoteAddress().ToWString();

#ifdef DEF_USE_POOL
	STrafficLogEntry* pEntry = &TrafficLog[FW::StringW(TrafficLog.Allocator(), Host.c_str(), Host.length())];
#else
	STrafficLogEntry* pEntry = &TrafficLog[Host];
#endif
	if (pEntry->IpAddress.IsNull())
		pEntry->IpAddress = pSocket->GetRemoteAddress();
	//STrafficLogEntryPtr& pEntry = m_TrafficLog[Host];
	//if(!pEntry) pEntry = std::make_shared<STrafficLogEntry>(Host);
	pEntry->Merge(Data);
}

void CTrafficLog::Clear()
{
	std::unique_lock Lock(m_Mutex);

	m_Data.reset();

	m_LastActivity = 0;
	m_Uploaded = 0;
	m_Downloaded = 0;
}

StVariant CTrafficLog::StoreTraffic(const SVarWriteOpt& Opts) const
{
	std::shared_lock Lock(m_Mutex);

	if (!m_Data)
		return StVariant();

	StVariantWriter TrafficLog;
	TrafficLog.BeginList();

#ifdef DEF_USE_POOL
	for (auto I = m_Data->TrafficLog.begin(); I != m_Data->TrafficLog.end(); ++I)
		TrafficLog.WriteVariant(DumpEntry(I.Key(), I.Value()));
#else
	for (auto I : m_Data->TrafficLog)
		TrafficLog.WriteVariant(DumpEntry(I.first, I.second));
#endif

	return TrafficLog.Finish();
}

void CTrafficLog::LoadTraffic(const StVariant& Data)
{
	std::unique_lock Lock(m_Mutex);

	if (!Data.IsValid())
		return;

	if (!m_Data) {
#ifdef DEF_USE_POOL
		if(!m_pMem)
			return;
		m_Data = m_pMem->New<STrafficLog>();
#else
		m_Data = std::make_shared<STrafficLog>();
#endif
	}

	for (uint32 i = 0; i < Data.Count(); i++)
	{
		const StVariant& Entry = Data[i];
#ifdef DEF_USE_POOL
		FW::StringW Host(m_pMem);
		Host = Entry[API_V_SOCK_RHOST].ToStringW();
		STrafficLogEntry* Data = &m_Data->TrafficLog[Host];
#else
		STrafficLogEntry* Data = &m_Data->TrafficLog[Entry[API_V_SOCK_RHOST].AsStr()];
#endif
		Data->IpAddress.FromString(Entry[API_V_SOCK_RADDR]);
		Data->LastActivity = Entry[API_V_SOCK_LAST_NET_ACT];
		Data->Uploaded = Entry[API_V_SOCK_UPLOADED];
		Data->Downloaded = Entry[API_V_SOCK_DOWNLOADED];
	}
}

#ifdef DEF_USE_POOL
StVariant CTrafficLog::DumpEntry(const FW::StringW& Host, const STrafficLogEntry& Data) const
#else
StVariant CTrafficLog::DumpEntry(const std::wstring& Host, const STrafficLogEntry& Data) const
#endif
{
	StVariantWriter Entry;
	Entry.BeginIndex();

	Entry.WriteEx(API_V_SOCK_RHOST, Host);
	Entry.WriteEx(API_V_SOCK_RADDR, Data.IpAddress.ToString());
	Entry.Write(API_V_SOCK_LAST_NET_ACT, Data.LastActivity);
	Entry.Write(API_V_SOCK_UPLOADED, Data.Uploaded);
	Entry.Write(API_V_SOCK_DOWNLOADED, Data.Downloaded);

	return Entry.Finish();
}

StVariant CTrafficLog::ToVariant(uint64 MinLastActivity) const
{
	std::shared_lock Lock(m_Mutex);

	if (!m_Data)
		return StVariant();

	StVariantWriter TrafficLog;
	TrafficLog.BeginList();

#ifdef DEF_USE_POOL
	FW::Map<FW::StringW, STrafficLogEntry> Current(m_Data->TrafficLog.Allocator());
#else
	std::map<std::wstring, STrafficLogEntry> Current;
#endif
	for (auto I : m_SocketList) {
		STrafficLogEntry Data(I);
		if(Data.LastActivity == 0)
			continue;
		CommitTraffic(I, Data, Current);
	}

#ifdef DEF_USE_POOL
	for (auto I = m_Data->TrafficLog.begin(); I != m_Data->TrafficLog.end(); ++I)
#else
	for (auto I: m_Data->TrafficLog)
#endif
	{
#ifdef DEF_USE_POOL
		auto F = Current.find(I.Key());
#else
		auto F = Current.find(I.first);
#endif
		if (F != Current.end())
		{
#ifdef DEF_USE_POOL
			if (MinLastActivity > F->LastActivity)
				continue;
			F->Merge(*I);
			TrafficLog.WriteVariant(DumpEntry(I.Key(), F.Value()));
#else
			if(MinLastActivity > F->second.LastActivity)
				continue;
			F->second.Merge(I.second);
			TrafficLog.WriteVariant(DumpEntry(I.first, F->second));
#endif
			Current.erase(F);
		}
		else 
		{
#ifdef DEF_USE_POOL
			if (MinLastActivity > I.Value().LastActivity)
				continue;
			TrafficLog.WriteVariant(DumpEntry(I.Key(), I.Value()));
#else
			if(MinLastActivity > I.second.LastActivity)
				continue;
			TrafficLog.WriteVariant(DumpEntry(I.first, I.second));
#endif
		}
	}

#ifdef DEF_USE_POOL
	for (auto I = Current.begin(); I != Current.end(); ++I)
#else
	for (auto I : Current) 
#endif
	{
#ifdef DEF_USE_POOL
		if (MinLastActivity > I.Value().LastActivity)
			continue;
		TrafficLog.WriteVariant(DumpEntry(I.Key(), I.Value()));
#else
		if(MinLastActivity > I.second.LastActivity)
			continue;
		TrafficLog.WriteVariant(DumpEntry(I.first, I.second));
#endif
	}

	return TrafficLog.Finish();
}