#pragma once
#include "../Library/Common/Variant.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Library/Common/FlexGuid.h"

class CTraceLogEntry
{
public:
	CTraceLogEntry(uint64 PID) {m_PID = PID;}
	virtual ~CTraceLogEntry() {}

	uint64 GetTimeStamp() const { return m_TimeStamp; }

	virtual CVariant ToVariant() const;

protected:

	virtual void WriteVariant(CVariant& Entry) const;

	uint64				m_PID = 0;	

	std::wstring		m_ServiceTag;
	std::wstring		m_AppSid;
	CFlexGuid			m_EnclaveGuid;

	uint64              m_TimeStamp = 0;
};

typedef std::shared_ptr<CTraceLogEntry> CTraceLogEntryPtr;
