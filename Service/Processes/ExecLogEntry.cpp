#include "pch.h"
#include "ExecLogEntry.h"
#include "../../Library/API/PrivacyAPI.h"

CExecLogEntry::CExecLogEntry(EExecLogRole Role, EExecLogType Type, EEventStatus Status, uint64 MiscID, std::wstring ActorServiceTag, uint64 TimeStamp, uint64 PID, uint32 AccessMask)
	: CTraceLogEntry(PID)
{
	m_Role = Role;
	m_Type = Type;
	m_Status = Status;
	m_MiscID = MiscID;
	m_AccessMask = AccessMask;

	m_TimeStamp = TimeStamp;
	m_ServiceTag = ActorServiceTag;
}

void CExecLogEntry::WriteVariant(CVariant& Entry) const
{
	CTraceLogEntry::WriteVariant(Entry);

	Entry.Write(API_V_PROC_EVENT_ROLE, (uint32)m_Role);

	Entry.Write(API_V_PROC_EVENT_TYPE, (uint32)m_Type);

	Entry.Write(API_V_PROC_EVENT_STATUS, (uint32)m_Status);

	Entry.Write(API_V_PROC_EVENT_MISC, m_MiscID);
	Entry.Write(API_V_PROC_EVENT_ACCESS_MASK, m_AccessMask);
}