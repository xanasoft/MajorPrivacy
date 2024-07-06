#include "pch.h"
#include "TraceLogEntry.h"
#include "../../Library/API/PrivacyAPI.h"

CVariant CTraceLogEntry::ToVariant() const
{
	CVariant Entry;
	Entry.BeginIMap();
	
	WriteVariant(Entry);

	Entry.Finish();
	return Entry;
}

void CTraceLogEntry::WriteVariant(CVariant& Entry) const
{
	Entry.Write(API_V_EVENT_UID, (uint64)this);

	Entry.Write(API_V_PID, m_PID);

	if(!m_ServiceTag.empty()) Entry.Write(API_V_SVC_TAG, m_ServiceTag);
	if(!m_AppSid.empty()) Entry.Write(API_V_APP_SID, m_AppSid);

	Entry.Write(API_V_EVENT_TIME_STAMP, m_TimeStamp);
}
