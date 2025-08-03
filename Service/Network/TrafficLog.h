#pragma once
#include "../Library/Common/StVariant.h"
#include "../Library/API/PrivacyDefs.h"
#include "../Network/Socket.h"

#include "../Framework/Core/MemoryPool.h"
#include "../Framework/Core/Object.h"
#include "../Framework/Core/Map.h"

class CTrafficLog
{
	TRACK_OBJECT(CTrafficLog)
public:
	CTrafficLog();
	~CTrafficLog();

#ifdef DEF_USE_POOL
	void SetPool(FW::AbstractMemPool* pMem) { m_pMem = pMem; }
#endif

	void AddSocket(const CSocketPtr& pSocket);
	bool RemoveSocket(const CSocketPtr& pSocket, bool bNoCommit = false);

	void SetLastActivity(uint64 TimeStamp)	{ std::unique_lock Lock(m_Mutex); m_LastActivity = TimeStamp; }
	uint64 GetLastActivity() const			{ std::shared_lock Lock(m_Mutex); return m_LastActivity; }
	void SetUploaded(uint64 Uploaded)		{ std::unique_lock Lock(m_Mutex); m_Uploaded = Uploaded; }
	uint64 GetUploaded() const				{ std::shared_lock Lock(m_Mutex); return m_Uploaded; }
	void SetDownloaded(uint64 Downloaded)	{ std::unique_lock Lock(m_Mutex); m_Downloaded = Downloaded; }
	uint64 GetDownloaded() const			{ std::shared_lock Lock(m_Mutex); return m_Downloaded; }

	void Clear();

	StVariant StoreTraffic(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	void LoadTraffic(const StVariant& Data);

	StVariant ToVariant(uint64 MinLastActivity = 0, FW::AbstractMemPool* pMemPool = nullptr) const;

protected:

	mutable std::shared_mutex m_Mutex;

	struct STrafficLogEntry
	{
		//TRACK_OBJECT(STrafficLogEntry)
		STrafficLogEntry() {}
		STrafficLogEntry(const CSocketPtr& pSocket);

		void Merge(const STrafficLogEntry& Data);

		CAddress IpAddress;
		uint64 LastActivity = 0;
		uint64 Uploaded = 0;
		uint64 Downloaded = 0;
	};

#ifdef DEF_USE_POOL
	static void CommitTraffic(const CSocketPtr& pSocket, const STrafficLogEntry& Data, FW::Map<FW::StringW, STrafficLogEntry>& TrafficLog);

	StVariant DumpEntry(const FW::StringW& Host, const STrafficLogEntry& Data, FW::AbstractMemPool* pMemPool = nullptr) const;
#else
	static void CommitTraffic(const CSocketPtr& pSocket, const STrafficLogEntry& Data, std::map<std::wstring, STrafficLogEntry>& TrafficLog);

	StVariant DumpEntry(const std::wstring& Host, const STrafficLogEntry& Data, FW::AbstractMemPool* pMemPool = nullptr) const;
#endif

#ifdef DEF_USE_POOL
	struct STrafficLog : FW::Object
#else
	struct STrafficLog
#endif
	{
#ifdef DEF_USE_POOL
		STrafficLog(FW::AbstractMemPool* pMem) : FW::Object(pMem), TrafficLog(pMem) {}

		FW::Map<FW::StringW, STrafficLogEntry> TrafficLog;
#else
		std::map<std::wstring, STrafficLogEntry> TrafficLog;
#endif
	};

#ifdef DEF_USE_POOL
	typedef FW::SharedPtr<STrafficLog> STrafficLogPtr;
#else
	typedef std::shared_ptr<STrafficLog> STrafficLogPtr;
#endif

	STrafficLogPtr			m_Data;

#ifdef DEF_USE_POOL
	FW::AbstractMemPool*	m_pMem = nullptr;
#endif

	std::set<CSocketPtr>	m_SocketList;

	uint64 m_LastActivity = 0;
	uint64 m_Uploaded = 0;
	uint64 m_Downloaded = 0;
};