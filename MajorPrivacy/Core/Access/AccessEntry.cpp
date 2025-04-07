#include "pch.h"
#include "AccessEntry.h"
#include "../Library/API/PrivacyAPI.h"


quint64 ReadAccessBranch(QMap<quint64, SAccessStatsPtr>& m_AccessStats, const QtVariant& Branch, const QString& Path)
{
	QtVariantReader Reader(Branch);

	uint64 Ref = Reader.Find(API_V_ACCESS_REF).To<uint64>(0);
	SAccessStatsPtr& pStats = m_AccessStats[Ref];
	if (!pStats) {
		pStats = SAccessStatsPtr(new SAccessStats());
		QString Name = Reader.Find(API_V_ACCESS_NAME).AsQStr();
		if (!Name.isEmpty()) {
			if (!Path.isEmpty())
				pStats->Path = Path + "\\" + Name;
			else if (Name.size() == 2 && Name[1] == ':')
				pStats->Path = Name;
			else
				pStats->Path = "\\" + Name;
		}
	}

	quint64 LastActivity = 0;
	Reader.ReadRawIndex([&](uint32 Index, const FW::CVariant& vData) {
		const QtVariant& Data = *(QtVariant*)&vData;

		switch (Index) 
		{
		case API_V_ACCESS_NAME: break; //pStats->FullPath = Path + "\\" + Data.AsQStr();

		case API_V_LAST_ACTIVITY:	pStats->LastAccessTime = Data.To<uint64>(0); break;
		case API_V_WAS_BLOCKED:		pStats->bBlocked = Data.To<bool>(false); break;
		case API_V_ACCESS_MASK:		pStats->AccessMask = Data.To<uint32>(0); break;
		case API_V_NT_STATUS:		pStats->NtStatus = Data.To<uint32>(0); break;
		case API_V_IS_DIRECTORY:	pStats->IsDirectory = Data.To<bool>(false); break;

		case API_V_ACCESS_NODES:
			QtVariantReader(Data).ReadRawList([&](const FW::CVariant& vData) {
				quint64 CurActivity = ReadAccessBranch(m_AccessStats, *(QtVariant*)&vData, pStats->Path);
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