#pragma once
#include "../Programs/TraceLogEntry.h"
#include "../Library/Common/Address.h"
#include "../Library/API/PrivacyDefs.h"

class CResLogEntry: public CTraceLogEntry
{
	TRACK_OBJECT(CResLogEntry)
public:
	CResLogEntry(const std::wstring& Path, std::wstring ServiceTag, uint32 AccessMask, EEventStatus Status, uint64 TimeStamp, uint64 PID);

	EEventStatus GetStatus() const { return m_Status; }

	virtual void WriteVariant(CVariant& Entry) const;

protected:

	std::wstring m_Path;
	uint32 m_AccessMask;
	EEventStatus m_Status;
};

typedef std::shared_ptr<CResLogEntry> CResLogEntryPtr;