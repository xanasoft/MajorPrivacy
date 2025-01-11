#pragma once
#include "../Programs/TraceLogEntry.h"

#include "../../Library/API/PrivacyDefs.h"

class CExecLogEntry: public CTraceLogEntry
{
	TRACK_OBJECT(CExecLogEntry)
public:
	CExecLogEntry(const CFlexGuid& EnclaveGuid, EExecLogRole Role, EExecLogType Type, EEventStatus Status, uint64 MiscID, const CFlexGuid& OtherEnclave, std::wstring ActorServiceTag, uint64 TimeStamp, uint64 PID, uint32 AccessMask = 0);

	EEventStatus GetStatus() const { return m_Status; }

	virtual void WriteVariant(CVariant& Entry) const;

protected:

	EExecLogRole		m_Role = EExecLogRole::eUndefined;
	EExecLogType		m_Type = EExecLogType::eUnknown;
	EEventStatus		m_Status = EEventStatus::eUndefined;
	uint64				m_MiscID = 0; // Image UID or program file UID
	CFlexGuid			m_OtherEnclave;
	uint32				m_AccessMask = 0;

};

typedef std::shared_ptr<CExecLogEntry> CExecLogEntryPtr;
