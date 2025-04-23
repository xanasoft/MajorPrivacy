#include "pch.h"
#include "ResLogEntry.h"
#include "../../Library/API/PrivacyAPI.h"

#ifdef DEF_USE_POOL
CResLogEntry::CResLogEntry(FW::AbstractMemPool* pMem, const CFlexGuid& EnclaveGuid, const std::wstring& NtPath, std::wstring ServiceTag, uint32 AccessMask, EEventStatus Status, uint64 TimeStamp, uint64 PID)
	: CTraceLogEntry(pMem, PID)
	, m_NtPath(pMem)
#else
CResLogEntry::CResLogEntry(const CFlexGuid& EnclaveGuid, const std::wstring& NtPath, std::wstring ServiceTag, uint32 AccessMask, EEventStatus Status, uint64 TimeStamp, uint64 PID)
	: CTraceLogEntry(PID)
#endif
{
#ifdef DEF_USE_POOL
	m_NtPath.Assign(NtPath.c_str(), NtPath.length());
#else
	m_NtPath = NtPath;
#endif
	m_AccessMask = AccessMask;
	m_Status = Status;

	m_TimeStamp = TimeStamp;
#ifdef DEF_USE_POOL
	m_ServiceTag.Assign(ServiceTag.c_str(), ServiceTag.length());
#else
	m_ServiceTag = ServiceTag;
#endif
	m_EnclaveGuid = EnclaveGuid;
}

void CResLogEntry::WriteVariant(StVariantWriter& Entry) const
{
	CTraceLogEntry::WriteVariant(Entry);

#ifdef DEF_USE_POOL
	Entry.Write(API_V_FILE_NT_PATH, m_NtPath);
#else
	Entry.WriteEx(API_V_FILE_NT_PATH, m_NtPath);
#endif
	Entry.Write(API_V_ACCESS_MASK, m_AccessMask);
	Entry.Write(API_V_EVENT_STATUS, (uint32)m_Status);
}