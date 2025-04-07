#include "pch.h"
#include "TraceLogEntry.h"
#include "../Library/API/PrivacyAPI.h"

void CAbstractLogEntry::FromVariant(const class QtVariant& FwEvent)
{
	QtVariantReader(FwEvent).ReadRawIndex([&](uint32 Index, const FW::CVariant& vData) {
		const QtVariant& Data = *(QtVariant*)&vData;
		ReadValue(Index, Data);
	});
}

void CAbstractLogEntry::ReadValue(uint32 Index, const QtVariant& Data)
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

bool CAbstractLogEntry::Match(const CAbstractLogEntry* pEntry) const
{
	//if (m_PID != pEntry->m_PID)
	//	return false;
	if (m_ServiceTag != pEntry->m_ServiceTag)
		return false;
	if (m_AppSid != pEntry->m_AppSid)
		return false;
	if (m_EnclaveGuid != pEntry->m_EnclaveGuid)
		return false;
	return true;
}