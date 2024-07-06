#include "pch.h"
#include "AccessEntry.h"
#include "../Library/API/PrivacyAPI.h"


quint64 ReadAccessBranch(QMap<quint64, SAccessStatsPtr>& m_AccessStats, const XVariant& Data, const QString& Path)
{
	uint64 Ref = Data.Find(API_V_ACCESS_REF).To<uint64>(0);
	SAccessStatsPtr& pStats = m_AccessStats[Ref];
	if (!pStats) {
		pStats = SAccessStatsPtr(new SAccessStats());
		pStats->Path.Set(Path, EPathType::eNative);
	}

	quint64 LastActivity = 0;
	Data.ReadRawIMap([&](uint32 Index, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;

		switch (Index) 
		{
		case API_V_ACCESS_NAME: break; //pStats->FullPath = Path + "\\" + Data.AsQStr();

		case API_V_ACCESS_TIME:		pStats->LastAccessTime = Data.To<uint64>(0); break;
		case API_V_ACCESS_BLOCKED:	pStats->bBlocked = Data.To<bool>(false); break;
		case API_V_ACCESS_MASK:		pStats->AccessMask = Data.To<uint32>(0); break;

		case API_V_ACCESS_NODES:
			Data.ReadRawList([&](const CVariant& vData) {
				const XVariant& Data = *(XVariant*)&vData;
				quint64 CurActivity = ReadAccessBranch(m_AccessStats, Data, Path + "\\" + Data.Find(API_V_ACCESS_NAME).AsQStr());
				if (CurActivity > LastActivity)
					LastActivity = CurActivity;
			});
			break;
		}
	});

	if(pStats->LastAccessTime > LastActivity)
		LastActivity = pStats->LastAccessTime;
	return LastActivity;
}