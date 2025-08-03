#pragma once
#include "../../Library/API/PrivacyDefs.h"
#include "../../Library/Common/StVariant.h"
#include "../../Library/Status.h"

class CEventLogEntry
{
public:
	CEventLogEntry() {}
	CEventLogEntry(ELogLevels Level, int Type, const StVariant& Data);
	virtual ~CEventLogEntry() {}

	virtual StVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	virtual NTSTATUS FromVariant(const StVariant& Rule);

protected:

	virtual void WriteIVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	//virtual void WriteMVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const StVariant& Data);
	//virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	uint64              m_TimeStamp = 0;
	ELogLevels			m_EventLevel = ELogLevels::eNone;
	int					m_EventType = 0;
	StVariant			m_EventData;
};

typedef std::shared_ptr<CEventLogEntry> CEventLogEntryPtr;

////////////////////////////////////////////////////////////////////////////
// CEventLog

class CEventLog
{
public:
	CEventLog();
	virtual ~CEventLog() {}

	void AddEvent(ELogLevels Level, int Type, const StVariant& Data);
	void ClearEvents();

	STATUS Load();
	STATUS Store();

	STATUS LoadEntries(const StVariant& Entries);
	StVariant SaveEntries(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr);

protected:

	std::shared_mutex m_Mutex;
	std::vector<CEventLogEntryPtr> m_Entries;
};