#pragma once
#include "../Library/Common/Variant.h"
#include "../Library/API/PrivacyDefs.h"
#include "../Network/Socket.h"

class CTrafficLog
{
	TRACK_OBJECT(CTrafficLog)
public:
	CTrafficLog();
	//~CTrafficLog();

	void AddSocket(const CSocketPtr& pSocket);
	bool RemoveSocket(const CSocketPtr& pSocket, bool bNoCommit = false);

	uint64 GetLastActivity() const	{ std::shared_lock Lock(m_Mutex); return m_Data.LastActivity; }
	uint64 GetUploaded() const		{ std::shared_lock Lock(m_Mutex); return m_Data.Uploaded; }
	uint64 GetDownloaded() const	{ std::shared_lock Lock(m_Mutex); return m_Data.Downloaded; }

	void Clear();

	CVariant StoreTraffic(const SVarWriteOpt& Opts) const;
	void LoadTraffic(const CVariant& Data);

	CVariant ToVariant(uint64 MinLastActivity = 0) const;

protected:

	mutable std::shared_mutex m_Mutex;

	struct STrafficLogEntry
	{
		TRACK_OBJECT(STrafficLogEntry)

		STrafficLogEntry() {}
		STrafficLogEntry(const CSocketPtr& pSocket);

		void Merge(const STrafficLogEntry& Data);

		CVariant ToVariant(const std::wstring& Host) const;

		CAddress IpAddress;
		uint64 LastActivity = 0;
		uint64 Uploaded = 0;
		uint64 Downloaded = 0;
	};
	//typedef std::shared_ptr<STrafficLogEntry> STrafficLogEntryPtr;

	static void CommitTraffic(const CSocketPtr& pSocket, const STrafficLogEntry& Data, std::map<std::wstring, STrafficLogEntry>& TrafficLog);

	//std::map<std::wstring, STrafficLogEntryPtr> m_TrafficLog;
	std::map<std::wstring, STrafficLogEntry> m_TrafficLog;

	std::set<CSocketPtr> m_SocketList;

	STrafficLogEntry m_Data;
};