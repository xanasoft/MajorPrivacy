#pragma once
#include "../Programs/ProgramID.h"

struct SAccessStats
{
	TRACK_OBJECT(SAccessStats)

	QString Path;
	uint64	LastAccessTime = 0;
	bool	bBlocked = false;
	uint32	AccessMask = 0;
	uint32	NtStatus = 0;
	bool	IsDirectory = false;
	uint64  AccessCount = 0;

	quint64 Seq = 0; // key in SAccessLog::Changes
};

typedef QSharedPointer<SAccessStats> SAccessStatsPtr;

//
// The access records of one program item. Stats is keyed by the service's node reference, Changes lists
// every entry once, keyed by the sequence number of its last change, so each view can take only what
// changed since it last looked.
//
struct SAccessLog
{
	QMap<quint64, SAccessStatsPtr> Stats;
	QMap<quint64, SAccessStatsPtr> Changes;
	quint64 LastSeq = 0; // never reset, so a cleared and refilled log only has newer numbers
};

quint64 ReadAccessBranch(SAccessLog& Log, const QtVariant& Data, const QString& Path = "");