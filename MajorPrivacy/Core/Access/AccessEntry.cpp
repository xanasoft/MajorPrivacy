#include "pch.h"
#include "AccessEntry.h"
#include "../Library/API/PrivacyAPI.h"


quint64 ReadAccessBranch(SAccessLog& Log, const QtVariant& Branch, const QString& Path)
{
	QtVariantReader Reader(Branch);

	uint64 Ref = Reader.Find(API_V_ACCESS_REF).To<uint64>(0);
	SAccessStatsPtr pStats = Log.Stats.value(Ref);
	bool bNew = !pStats;
	if (bNew) {
		pStats = SAccessStatsPtr(new SAccessStats());
		Log.Stats.insert(Ref, pStats);
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

	// the dump also carries the parents of what changed, only a real change gets a new sequence number
	uint64 OldLastAccessTime = pStats->LastAccessTime;
	bool bOldBlocked = pStats->bBlocked;
	uint32 OldAccessMask = pStats->AccessMask;
	uint32 OldNtStatus = pStats->NtStatus;
	bool bOldIsDirectory = pStats->IsDirectory;
	uint64 OldAccessCount = pStats->AccessCount;

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
		case API_V_ACCESS_COUNT:	pStats->AccessCount = Data.To<uint64>(0); break;

		case API_V_ACCESS_NODES:
			QtVariantReader(Data).ReadRawList([&](const FW::CVariant& vData) {
				quint64 CurActivity = ReadAccessBranch(Log, *(QtVariant*)&vData, pStats->Path);
				if (CurActivity > LastActivity)
					LastActivity = CurActivity;
			});
			break;
		}
	});

	if (bNew || pStats->LastAccessTime != OldLastAccessTime || pStats->bBlocked != bOldBlocked || pStats->AccessMask != OldAccessMask
		|| pStats->NtStatus != OldNtStatus || pStats->IsDirectory != bOldIsDirectory || pStats->AccessCount != OldAccessCount)
	{
		if (pStats->Seq)
			Log.Changes.remove(pStats->Seq);
		pStats->Seq = ++Log.LastSeq;
		Log.Changes.insert(pStats->Seq, pStats);
	}

	if(pStats->LastAccessTime > LastActivity)
		LastActivity = pStats->LastAccessTime;
	return LastActivity;
}