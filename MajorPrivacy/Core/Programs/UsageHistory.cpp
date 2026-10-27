#include "pch.h"
#include "UsageHistory.h"
#include "../../../Library/API/PrivacyAPI.h"

int SUsageHistory::STier::LowerBound(quint32 Time) const
{
	int Low = 0, High = Buckets.count();
	while (Low < High) {
		int Mid = Low + (High - Low) / 2;
		if (Buckets[Mid].Time < Time)
			Low = Mid + 1;
		else
			High = Mid;
	}
	return Low;
}

bool SUsageHistory::Merge(EUsageTier eTier, const QtVariant& Data)
{
	if (eTier >= EUsageTier::eMax || !Data.IsValid())
		return false;

	quint32 Interval = Data[API_V_USAGE_INTERVAL].To<quint32>(0);
	quint32 RecSize = Data[API_V_USAGE_REC_SIZE].To<quint32>(0);
	QByteArray Blob = Data[API_V_USAGE_DATA].AsQBytes();
	if (Interval == 0 || RecSize < sizeof(quint32))
		return false;

	STier& Tier = Tiers[(int)eTier];
	if (Tier.Interval != Interval) {
		// the service changed the bucket length, what we have does not line up any more
		Tier.Buckets.clear();
		Tier.Watermark = 0;
		Tier.Interval = Interval;
	}

	size_t uCopy = RecSize < sizeof(SUsageBucket) ? RecSize : sizeof(SUsageBucket);
	const char* pData = Blob.constData();
	int uSize = Blob.size();

	bool bChanged = false;
	for (int Pos = 0; Pos + (int)RecSize <= uSize; Pos += RecSize)
	{
		SUsageBucket Bucket; // zero initialized, unknown (shorter) records keep the defaults
		memcpy(&Bucket, pData + Pos, uCopy);
		if (Bucket.Time == 0)
			continue;

		//
		// Normally the service sends the newest bucket we already have (it may still be growing)
		// followed by newer ones, so this is a replace of the last entry and appends. A bucket
		// older than that can only show up right after a clear, it is inserted in order.
		//
		int Count = Tier.Buckets.count();
		if (Count > 0 && Tier.Buckets[Count - 1].Time == Bucket.Time)
			Tier.Buckets[Count - 1] = Bucket;
		else if (Count == 0 || Tier.Buckets[Count - 1].Time < Bucket.Time)
			Tier.Buckets.append(Bucket);
		else {
			int Index = Tier.LowerBound(Bucket.Time);
			if (Index < Count && Tier.Buckets[Index].Time == Bucket.Time)
				Tier.Buckets[Index] = Bucket;
			else
				Tier.Buckets.insert(Index, Bucket);
		}

		if (Bucket.Time > Tier.Watermark)
			Tier.Watermark = Bucket.Time;
		bChanged = true;
	}

	if (bChanged)
		Tier.Revision++;
	return true;
}

void SUsageHistory::Trim(EUsageTier eTier, quint32 OlderThan)
{
	if (eTier >= EUsageTier::eMax)
		return;
	STier& Tier = Tiers[(int)eTier];
	int Index = Tier.LowerBound(OlderThan);
	if (Index <= 0)
		return;
	Tier.Buckets.remove(0, Index);
	Tier.Revision++;
}

void SUsageHistory::Clear()
{
	for (int i = 0; i < (int)EUsageTier::eMax; i++) {
		Tiers[i].Buckets.clear();
		Tiers[i].Watermark = 0;
		Tiers[i].Revision++;
	}
}
