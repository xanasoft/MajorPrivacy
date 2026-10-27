#include "pch.h"
#include "UsageLog.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Network/SocketList.h"

CUsageLog::CUsageLog()
{
}

CUsageLog::~CUsageLog()
{
}

//////////////////////////////////////////////////////////////////////////
// STier

const SUsageBucket* CUsageLog::STier::Data() const
{
#ifdef DEF_USE_POOL
	return Buckets.Data();
#else
	return Buckets.data();
#endif
}

size_t CUsageLog::STier::LowerBound(uint32 Time) const
{
	// the buckets are sorted by time
	const SUsageBucket* pData = Data();
	size_t Low = 0, High = Count();
	while (Low < High) {
		size_t Mid = Low + (High - Low) / 2;
		if (pData[Mid].Time < Time)
			Low = Mid + 1;
		else
			High = Mid;
	}
	return Low;
}

void CUsageLog::STier::Add(uint32 Bucket, const SUsageSample& Sample)
{
	size_t uCount = Count();
	if (uCount > 0)
	{
		//
		// Normally the sample belongs to the last bucket or to a new one after it.
		// Should the clock have gone backwards (time sync) the sample is added to
		// the last bucket, the array must stay sorted.
		//
#ifdef DEF_USE_POOL
		SUsageBucket& Last = Buckets[uCount - 1];
#else
		SUsageBucket& Last = Buckets.back();
#endif
		if (Last.Time >= Bucket) {
			Last.Data.Add(Sample);
			return;
		}
	}

	SUsageBucket New;
	New.Time = Bucket;
	New.Data = Sample;
#ifdef DEF_USE_POOL
	Buckets.Append(New);
#else
	Buckets.push_back(New);
#endif
}

void CUsageLog::STier::EraseFront(size_t uCount)
{
	if (uCount == 0)
		return;
	if (uCount >= Count()) {
#ifdef DEF_USE_POOL
		Buckets.Clear();
#else
		Buckets.clear();
#endif
		return;
	}
	Buckets.erase(Buckets.begin(), Buckets.begin() + uCount);
}

//////////////////////////////////////////////////////////////////////////
// CUsageLog

CUsageLog::STierPtr CUsageLog::GetTier(EUsageTier Tier, bool bCanCreate)
{
	STierPtr& pTier = m_Tiers[(int)Tier];
	if (!pTier && bCanCreate) {
#ifdef DEF_USE_POOL
		if (!m_pMem)
			return pTier;
		pTier = m_pMem->New<STier>();
#else
		pTier = std::make_shared<STier>();
#endif
		pTier->Interval = UsageTierInterval(Tier);
	}
	return pTier;
}

void CUsageLog::AddNetworkIO(int Type, uint32 TransferSize, bool bLan)
{
	switch ((CSocketList::EEventType)Type)
	{
	case CSocketList::EEventType::Receive:
		m_PendingDownload.fetch_add(TransferSize, std::memory_order_relaxed);
		if (bLan) m_PendingLanDownload.fetch_add(TransferSize, std::memory_order_relaxed);
		break;
	case CSocketList::EEventType::Send:
		m_PendingUpload.fetch_add(TransferSize, std::memory_order_relaxed);
		if (bLan) m_PendingLanUpload.fetch_add(TransferSize, std::memory_order_relaxed);
		break;
	}
}

void CUsageLog::AddConnections(bool bInbound, uint32 Seconds)
{
	if (bInbound)
		m_PendingConnInbound.fetch_add(Seconds, std::memory_order_relaxed);
	else
		m_PendingConnOutbound.fetch_add(Seconds, std::memory_order_relaxed);
}

void CUsageLog::Commit(uint64 NowMs, const SUsageSample* pSample)
{
	SUsageSample Sample;
	if (pSample)
		Sample = *pSample;
	Sample.NetUpload += m_PendingUpload.exchange(0, std::memory_order_relaxed);
	Sample.NetDownload += m_PendingDownload.exchange(0, std::memory_order_relaxed);
	Sample.LanUpload += m_PendingLanUpload.exchange(0, std::memory_order_relaxed);
	Sample.LanDownload += m_PendingLanDownload.exchange(0, std::memory_order_relaxed);
	Sample.ConnInbound += m_PendingConnInbound.exchange(0, std::memory_order_relaxed);
	Sample.ConnOutbound += m_PendingConnOutbound.exchange(0, std::memory_order_relaxed);

	if (Sample.IsZero())
		return;

	uint32 NowSec = (uint32)(NowMs / 1000);

	std::unique_lock Lock(m_Mutex);

	for (int i = 0; i < (int)EUsageTier::eMax; i++)
	{
		STierPtr pTier = GetTier((EUsageTier)i, true);
		if (!pTier)
			continue;
		pTier->Add(NowSec - (NowSec % pTier->Interval), Sample);

		// the live tier is small and busy, keep it trimmed here rather than only every 15 minutes
		if ((EUsageTier)i == EUsageTier::eSeconds && pTier->Count() > m_SecondsWindow + 60) {
			uint32 Oldest = NowSec > m_SecondsWindow ? (uint32)(NowSec - m_SecondsWindow) : 0;
			pTier->EraseFront(pTier->LowerBound(Oldest));
		}
	}
}

void CUsageLog::Truncate(uint64 NowMs, uint64 SecondsWindow, uint64 RetentionMinutes)
{
	uint32 NowSec = (uint32)(NowMs / 1000);

	std::unique_lock Lock(m_Mutex);

	m_SecondsWindow = SecondsWindow;

	if (STierPtr pTier = m_Tiers[(int)EUsageTier::eSeconds]) {
		uint32 Oldest = NowSec > SecondsWindow ? (uint32)(NowSec - SecondsWindow) : 0;
		pTier->EraseFront(pTier->LowerBound(Oldest));
	}

	if (STierPtr pTier = m_Tiers[(int)EUsageTier::eMinutes]) {
		uint64 Retention = RetentionMinutes * 60;
		uint32 Oldest = NowSec > Retention ? (uint32)(NowSec - Retention) : 0;
		pTier->EraseFront(pTier->LowerBound(Oldest));
	}
}

StVariant CUsageLog::DumpTier(const STier& Tier, size_t From, FW::AbstractMemPool* pMemPool)
{
	size_t uCount = Tier.Count() - From;

	StVariant Data(pMemPool);
	Data[API_V_USAGE_INTERVAL] = Tier.Interval;
	Data[API_V_USAGE_REC_SIZE] = (uint32)sizeof(SUsageBucket);
	Data[API_V_USAGE_DATA] = StVariant(pMemPool, (const byte*)(Tier.Data() + From), uCount * sizeof(SUsageBucket));
	return Data;
}

StVariant CUsageLog::Dump(EUsageTier Tier, uint32 Since, uint64 NowMs, FW::AbstractMemPool* pMemPool) const
{
	std::shared_lock Lock(m_Mutex);

	STierPtr pTier = m_Tiers[(int)Tier];
	if (!pTier || pTier->Count() == 0)
		return StVariant();

	size_t From = pTier->LowerBound(Since);
	if (From >= pTier->Count())
		return StVariant();

	//
	// The client asks with the time of the newest bucket it has, as that one may still
	// be growing. When it is the only bucket in range and it is already closed, the
	// client has its final value and nothing needs to be sent.
	//
	if (From + 1 == pTier->Count()) {
		const SUsageBucket& Last = pTier->Data()[From];
		if (Last.Time == Since && (uint64)Last.Time + pTier->Interval <= NowMs / 1000)
			return StVariant();
	}

	return DumpTier(*pTier, From, pMemPool);
}

StVariant CUsageLog::Store(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	std::shared_lock Lock(m_Mutex);

	STierPtr pTier = m_Tiers[(int)EUsageTier::eMinutes];
	if (!pTier || pTier->Count() == 0)
		return StVariant();

	return DumpTier(*pTier, 0, pMemPool);
}

void CUsageLog::Load(const StVariant& Data)
{
	if (!Data.IsValid())
		return;

	uint32 Interval = Data[API_V_USAGE_INTERVAL].To<uint32>(0);
	uint32 RecSize = Data[API_V_USAGE_REC_SIZE].To<uint32>(0);
	StVariant Blob = Data[API_V_USAGE_DATA];
	if (Interval != UsageTierInterval(EUsageTier::eMinutes) || RecSize < sizeof(uint32) || !Blob.IsValid())
		return;

	const byte* pData = Blob.GetData();
	size_t uSize = Blob.GetSize();
	size_t uCopy = RecSize < sizeof(SUsageBucket) ? RecSize : sizeof(SUsageBucket);

	std::unique_lock Lock(m_Mutex);

	STierPtr pTier = GetTier(EUsageTier::eMinutes, true);
	if (!pTier)
		return;

	//
	// The records are loaded on a thread after the service is up and sampling, so the
	// tier may already hold the bucket of the current minute. Add() keeps the array
	// sorted by folding anything older into the last bucket, which would collapse the
	// whole history into that one bucket. So the records, which are older, go in first
	// and what was already there is added back after them.
	//
	std::vector<SUsageBucket> Current(pTier->Data(), pTier->Data() + pTier->Count());
	pTier->EraseFront(pTier->Count());

#ifdef DEF_USE_POOL
	pTier->Buckets.Reserve(Current.size() + uSize / RecSize);
#else
	pTier->Buckets.reserve(Current.size() + uSize / RecSize);
#endif

	for (size_t Pos = 0; Pos + RecSize <= uSize; Pos += RecSize)
	{
		SUsageBucket Bucket; // zero initialized, unknown (shorter) records keep the defaults
		memcpy(&Bucket, pData + Pos, uCopy);
		if (Bucket.Time == 0)
			continue;
		pTier->Add(Bucket.Time, Bucket.Data);
	}

	for (const SUsageBucket& Bucket : Current)
		pTier->Add(Bucket.Time, Bucket.Data);
}

void CUsageLog::Clear()
{
	std::unique_lock Lock(m_Mutex);

	for (int i = 0; i < (int)EUsageTier::eMax; i++)
#ifdef DEF_USE_POOL
		m_Tiers[i].Clear();
#else
		m_Tiers[i].reset();
#endif

	m_PendingUpload = 0;
	m_PendingDownload = 0;
}

bool CUsageLog::HasData() const
{
	std::shared_lock Lock(m_Mutex);

	for (int i = 0; i < (int)EUsageTier::eMax; i++) {
		if (m_Tiers[i] && m_Tiers[i]->Count() > 0)
			return true;
	}
	return false;
}
