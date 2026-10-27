#pragma once
#include "../../../Library/Common/UsageStats.h"
#include "../../Common/QtVariant.h"

//
// GUI side copy of a program item's usage history, see Library/Common/UsageStats.h and
// Service/Programs/UsageLog.h. Filled incrementally through CPrivacyCore::UpdateUsageStats,
// the watermark is the time of the newest bucket we have, which may still be growing.
//

struct SUsageHistory
{
	struct STier
	{
		quint32					Interval = 0;	// seconds per bucket
		QVector<SUsageBucket>	Buckets;		// sorted by Time
		quint32					Watermark = 0;	// Time of the newest bucket
		quint32					Revision = 0;	// bumped on every change, so views can skip rebuilding

		// index of the first bucket with Time >= Time
		int LowerBound(quint32 Time) const;
	};

	STier Tiers[(int)EUsageTier::eMax];

	// merges one API_V_USAGE_LOG value, returns false when the data was not usable
	bool Merge(EUsageTier Tier, const QtVariant& Data);
	void Trim(EUsageTier Tier, quint32 OlderThan);
	void Clear();
};
