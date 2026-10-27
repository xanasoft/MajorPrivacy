#pragma once
#include "../Library/Common/StVariant.h"
#include "../Library/Common/UsageStats.h"
#include "../Library/API/PrivacyDefs.h"

#include "../Framework/Core/MemoryPool.h"
#include "../Framework/Core/Object.h"
#include "../Framework/Core/Array.h"

//
// Usage history of one program item (program file or service), see Library/Common/UsageStats.h.
//
// CPU and I/O arrive once per sample from CProcessList::UpdateUsage, network bytes arrive
// per event from CSocketList::OnNetworkEvent and are only accumulated here; Commit() moves
// everything into the buckets, so all three metrics share one time base.
//

class CUsageLog
{
	TRACK_OBJECT(CUsageLog)
public:
	CUsageLog();
	~CUsageLog();

#ifdef DEF_USE_POOL
	void SetPool(FW::AbstractMemPool* pMem) { m_pMem = pMem; }
#endif

	// hot path, no lock; bLan: the remote end is a LAN address (IsLanAddress)
	void AddNetworkIO(int Type, uint32 TransferSize, bool bLan);
	void AddConnections(bool bInbound, uint32 Seconds); // connection seconds, see SUsageSample

	// pSample may be NULL, then only the pending network bytes are committed
	void Commit(uint64 NowMs, const SUsageSample* pSample);

	void Truncate(uint64 NowMs, uint64 SecondsWindow, uint64 RetentionMinutes);

	// buckets with Time >= Since, invalid variant when there is nothing new to send
	StVariant Dump(EUsageTier Tier, uint32 Since, uint64 NowMs, FW::AbstractMemPool* pMemPool = nullptr) const;

	StVariant Store(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	void Load(const StVariant& Data);

	void Clear();

	bool HasData() const;

protected:

	mutable std::shared_mutex m_Mutex;

	std::atomic<uint64> m_PendingUpload{ 0 };		// all traffic
	std::atomic<uint64> m_PendingDownload{ 0 };
	std::atomic<uint64> m_PendingLanUpload{ 0 };	// the LAN part of it
	std::atomic<uint64> m_PendingLanDownload{ 0 };
	std::atomic<uint32> m_PendingConnInbound{ 0 };	// connection seconds since the last commit
	std::atomic<uint32> m_PendingConnOutbound{ 0 };

	uint64 m_SecondsWindow = 300; // the live tier is trimmed to this on the fly, Truncate() refreshes it from the config

#ifdef DEF_USE_POOL
	struct STier : FW::Object
#else
	struct STier
#endif
	{
#ifdef DEF_USE_POOL
		STier(FW::AbstractMemPool* pMem) : FW::Object(pMem), Buckets(pMem) {}

		FW::Array<SUsageBucket> Buckets;
#else
		std::vector<SUsageBucket> Buckets;
#endif
		uint32 Interval = 0;

		size_t Count() const { return Buckets.size(); }
		const SUsageBucket* Data() const;
		size_t LowerBound(uint32 Time) const; // index of the first bucket with Time >= Time
		void Add(uint32 Bucket, const SUsageSample& Sample);
		void EraseFront(size_t Count);
	};

#ifdef DEF_USE_POOL
	typedef FW::SharedPtr<STier> STierPtr;
#else
	typedef std::shared_ptr<STier> STierPtr;
#endif

	STierPtr GetTier(EUsageTier Tier, bool bCanCreate);
	static StVariant DumpTier(const STier& Tier, size_t From, FW::AbstractMemPool* pMemPool);

	STierPtr				m_Tiers[(int)EUsageTier::eMax];

#ifdef DEF_USE_POOL
	FW::AbstractMemPool*	m_pMem = nullptr;
#endif
};
