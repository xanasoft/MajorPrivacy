#include "pch.h"
#include "TraceLogEntry.h"
#include "../../Library/API/PrivacyAPI.h"

StVariant CTraceLogEntry::ToVariant(FW::AbstractMemPool* pMemPool) const
{
	StVariantWriter Entry(pMemPool);
	Entry.BeginIndex();
	
	WriteVariant(Entry);

	return Entry.Finish();
}

void CTraceLogEntry::WriteVariant(StVariantWriter& Entry) const
{
	Entry.Write(API_V_EVENT_REF, (uint64)this);

	Entry.Write(API_V_PID, m_PID);

#ifdef DEF_USE_POOL
	if(!m_ServiceTag.IsEmpty()) Entry.Write(API_V_SERVICE_TAG, m_ServiceTag);
	if(!m_AppSid.IsEmpty()) Entry.Write(API_V_APP_SID, m_AppSid);
#else
	if(!m_ServiceTag.empty()) Entry.WriteEx(API_V_SERVICE_TAG, m_ServiceTag);
	if(!m_AppSid.empty()) Entry.WriteEx(API_V_APP_SID, m_AppSid);
#endif
	if(!m_EnclaveGuid.IsNull()) Entry.WriteVariant(API_V_ENCLAVE, m_EnclaveGuid.ToVariant(false, Entry.Allocator()));

	Entry.Write(API_V_EVENT_TIME_STAMP, m_TimeStamp);
}

