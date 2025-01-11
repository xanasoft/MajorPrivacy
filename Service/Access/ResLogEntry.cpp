#include "pch.h"
#include "ResLogEntry.h"
#include "../../Library/API/PrivacyAPI.h"

CResLogEntry::CResLogEntry(const CFlexGuid& EnclaveGuid, const std::wstring& NtPath, std::wstring ServiceTag, uint32 AccessMask, EEventStatus Status, uint64 TimeStamp, uint64 PID)
	: CTraceLogEntry(PID)
{
	m_NtPath = NtPath;
	m_AccessMask = AccessMask;
	m_Status = Status;

	m_TimeStamp = TimeStamp;
	m_ServiceTag = ServiceTag;
	m_EnclaveGuid = EnclaveGuid;
}

void CResLogEntry::WriteVariant(CVariant& Entry) const
{
	CTraceLogEntry::WriteVariant(Entry);

	Entry.Write(API_V_FILE_NT_PATH, m_NtPath);
	Entry.Write(API_V_ACCESS_MASK, m_AccessMask);
	Entry.Write(API_V_EVENT_STATUS, (uint32)m_Status);
}