#include "pch.h"
#include "ResLogEntry.h"
#include "../../Library/API/PrivacyAPI.h"

CResLogEntry::CResLogEntry(const std::wstring& Path, std::wstring ServiceTag, uint32 AccessMask, EEventStatus Status, uint64 TimeStamp, uint64 PID)
	: CTraceLogEntry(PID)
{
	m_Path = Path;
	m_AccessMask = AccessMask;
	m_Status = Status;

	m_TimeStamp = TimeStamp;
	m_ServiceTag = ServiceTag;
}

void CResLogEntry::WriteVariant(CVariant& Entry) const
{
	CTraceLogEntry::WriteVariant(Entry);

	Entry.Write(API_V_EVENT_PATH, m_Path);
	Entry.Write(API_V_EVENT_ACCESS, m_AccessMask);
	Entry.Write(API_V_EVENT_ACCESS_STATUS, (uint32)m_Status);
}