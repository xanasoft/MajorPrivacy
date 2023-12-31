#include "pch.h"
#include "TraceLogEntry.h"
#include "ServiceAPI.h"

CVariant CTraceLogEntry::ToVariant() const
{
	CVariant Entry;
	Entry.BeginMap();
	
	WriteVariant(Entry);

	Entry.Finish();
	return Entry;
}

void CTraceLogEntry::WriteVariant(CVariant& Entry) const
{
	Entry.Write(SVC_API_EVENT_UID, (uint64)this);

	if(!m_ServiceTag.empty()) Entry.Write(SVC_API_ID_SVC_TAG, m_ServiceTag);
	if(!m_AppSid.empty()) Entry.Write(SVC_API_ID_APP_SID, m_AppSid);

	Entry.Write(SVC_API_EVENT_TIMESTAMP, m_TimeStamp);
}
