#pragma once
#include "../Library/Common/StVariant.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Library/Common/FlexGuid.h"

#include "../Framework/Core/Object.h"

#ifdef DEF_USE_POOL
class CTraceLogEntry : public FW::Object
#else
class CTraceLogEntry
#endif
{
public:
#ifdef DEF_USE_POOL
	CTraceLogEntry(FW::AbstractMemPool* pMem, uint64 PID) 
		: FW::Object(pMem), m_ServiceTag(pMem), m_AppSid(pMem) { m_PID = PID; }
#else
	CTraceLogEntry(uint64 PID) {m_PID = PID;}
#endif
	virtual ~CTraceLogEntry() {}

	uint64 GetTimeStamp() const { return m_TimeStamp; }

	virtual StVariant ToVariant(FW::AbstractMemPool* pMemPool = nullptr) const;

protected:

	virtual void WriteVariant(StVariantWriter& Entry) const;

	uint64				m_PID = 0;	

#ifdef DEF_USE_POOL
	FW::StringW			m_ServiceTag;
	FW::StringW			m_AppSid;
#else
	std::wstring		m_ServiceTag;
	std::wstring		m_AppSid;
#endif
	CFlexGuid			m_EnclaveGuid;

	uint64              m_TimeStamp = 0;
};

#ifdef DEF_USE_POOL
typedef FW::SharedPtr<CTraceLogEntry> CTraceLogEntryPtr;
#else
typedef std::shared_ptr<CTraceLogEntry> CTraceLogEntryPtr;
#endif
