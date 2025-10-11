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
};

typedef QSharedPointer<SAccessStats> SAccessStatsPtr;

quint64 ReadAccessBranch(QMap<quint64, SAccessStatsPtr>& m_AccessStats, const QtVariant& Data, const QString& Path = "");