#include "pch.h"
#include "TraceLogEntry.h"
#include "../Service/ServiceAPI.h"

void CAbstractLogEntry::FromVariant(const class XVariant& FwEvent)
{
	FwEvent.ReadRawMap([&](const SVarName& Name, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;
		ReadValue(Name, Data);
	});
}

void CAbstractLogEntry::ReadValue(const SVarName& Name, const XVariant& Data)
{
		 if (VAR_TEST_NAME(Name, SVC_API_EVENT_UID))		m_UID = Data;

	else if (VAR_TEST_NAME(Name, SVC_API_ID_SVC_TAG))		m_ServiceTag = Data.AsQStr();
	else if (VAR_TEST_NAME(Name, SVC_API_ID_APP_SID))		m_AppSid = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, SVC_API_EVENT_TIMESTAMP))	m_TimeStamp = Data;

	// else unknown tag
}