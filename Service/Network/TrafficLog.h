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

	void SetLastActivity(uint64 TimeStamp)	{ std::unique_lock Lock(m_Mutex); m_LastActivity = TimeStamp; }
	uint64 GetLastActivity() const			{ std::shared_lock Lock(m_Mutex); return m_LastActivity; }
	void SetUploaded(uint64 Uploaded)		{ std::unique_lock Lock(m_Mutex); m_Uploaded = Uploaded; }
	uint64 GetUploaded() const				{ std::shared_lock Lock(m_Mutex); return m_Uploaded; }
	void SetDownloaded(uint64 Downloaded)	{ std::unique_lock Lock(m_Mutex); m_Downloaded = Downloaded; }
	uint64 GetDownloaded() const			{ std::shared_lock Lock(m_Mutex); return m_Downloaded; }

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

	uint64 m_LastActivity = 0;
	uint64 m_Uploaded = 0;
	uint64 m_Downloaded = 0;
};