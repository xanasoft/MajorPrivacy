#pragma once
#include "../Programs/ProgramID.h"
#include "../../Helpers/FilePath.h"

struct SAccessStats
{
	CFilePath Path;

	uint64	LastAccessTime = 0;
	bool	bBlocked = false;
	uint32	AccessMask = 0;
};

typedef QSharedPointer<SAccessStats> SAccessStatsPtr;

quint64 ReadAccessBranch(QMap<quint64, SAccessStatsPtr>& m_AccessStats, const XVariant& Data, const QString& Path = "");