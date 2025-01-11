#include "pch.h"
#include "ExecLogEntry.h"
#include "../../Library/API/PrivacyAPI.h"

CExecLogEntry::CExecLogEntry(const CFlexGuid& EnclaveGuid, EExecLogRole Role, EExecLogType Type, EEventStatus Status, uint64 MiscID, const CFlexGuid& OtherEnclave, std::wstring ActorServiceTag, uint64 TimeStamp, uint64 PID, uint32 AccessMask)
	: CTraceLogEntry(PID)
{
	m_Role = Role;
	m_Type = Type;
	m_Status = Status;
	m_MiscID = MiscID;
	m_OtherEnclave = OtherEnclave;
	m_AccessMask = AccessMask;

	m_TimeStamp = TimeStamp;
	m_ServiceTag = ActorServiceTag;
	m_EnclaveGuid = EnclaveGuid;
}

void CExecLogEntry::WriteVariant(CVariant& Entry) const
{
	CTraceLogEntry::WriteVariant(Entry);

	Entry.Write(API_V_EVENT_ROLE, (uint32)m_Role);

	Entry.Write(API_V_EVENT_TYPE, (uint32)m_Type);

	Entry.Write(API_V_EVENT_STATUS, (uint32)m_Status);

	Entry.Write(API_V_PROC_MISC_ID, m_MiscID);
	Entry.WriteVariant(API_V_PROC_MISC_ENCLAVE, m_OtherEnclave.ToVariant(false));

	Entry.Write(API_V_ACCESS_MASK, m_AccessMask);
}