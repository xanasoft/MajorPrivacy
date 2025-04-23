#pragma once
#include "../Programs/TraceLogEntry.h"
#include "../Library/Common/Address.h"
#include "../Library/API/PrivacyDefs.h"

class CResLogEntry: public CTraceLogEntry
{
	//TRACK_OBJECT(CResLogEntry)
public:
#ifdef DEF_USE_POOL
	CResLogEntry(FW::AbstractMemPool* pMem, const CFlexGuid& EnclaveGuid, const std::wstring& NtPath, std::wstring ServiceTag, uint32 AccessMask, EEventStatus Status, uint64 TimeStamp, uint64 PID);
#else
	CResLogEntry(const CFlexGuid& EnclaveGuid, const std::wstring& NtPath, std::wstring ServiceTag, uint32 AccessMask, EEventStatus Status, uint64 TimeStamp, uint64 PID);
#endif

	EEventStatus GetStatus() const { return m_Status; }

	virtual void WriteVariant(StVariantWriter& Entry) const;

protected:

#ifdef DEF_USE_POOL
	FW::StringW m_NtPath;
#else
	std::wstring m_NtPath;
#endif
	uint32 m_AccessMask;
	EEventStatus m_Status;
};

#ifdef DEF_USE_POOL
typedef FW::SharedPtr<CResLogEntry> CResLogEntryPtr;
#else
typedef std::shared_ptr<CResLogEntry> CResLogEntryPtr;
#endif