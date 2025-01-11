#include "pch.h"
#include "TraceLogEntry.h"
#include "../Library/API/PrivacyAPI.h"

void CAbstractLogEntry::FromVariant(const class XVariant& FwEvent)
{
	FwEvent.ReadRawIMap([&](uint32 Index, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;
		ReadValue(Index, Data);
	});
}

void CAbstractLogEntry::ReadValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_EVENT_REF:			m_UID = Data; break;
	case API_V_PID:					m_PID = Data; break;
	case API_V_SERVICE_TAG:			m_ServiceTag = Data.AsQStr(); break;
	case API_V_APP_SID:				m_AppSid = Data.AsQStr(); break;
	case API_V_ENCLAVE:				m_EnclaveGuid.FromVariant(Data); break;
	case API_V_EVENT_TIME_STAMP:	m_TimeStamp = Data; break;
	}
}