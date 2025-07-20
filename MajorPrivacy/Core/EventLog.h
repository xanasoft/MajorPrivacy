#pragma once
#include "../../Library/API/PrivacyDefs.h"
#include "../../Library/Common/StVariant.h"
#include "../../Library/Status.h"
#include "../Common/QtVariant.h"

class CEventLogEntry
{
public:
	CEventLogEntry() {}
	virtual ~CEventLogEntry() {}

	quint64 GetUID() const { return m_UID; }
	//void SetUID(quint64 UID) { m_UID = UID; }
	quint64 GetTimeStamp() const { return m_TimeStamp; }
	//void SetTimeStamp(quint64 TimeStamp) { m_TimeStamp = TimeStamp; }
	ELogLevels GetEventLevel() const { return m_EventLevel; }
	//void SetEventLevel(ELogLevels Level) { m_EventLevel = Level; }
	int GetEventType() const { return m_EventType; }
	//void SetEventType(int Type) { m_EventType = Type; }
	const QtVariant& GetEventData() const { return m_EventData; }
	//void SetEventData(const QtVariant& Data) { m_EventData = Data; }

	virtual QtVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual NTSTATUS FromVariant(const StVariant& Data);

protected:

	virtual void WriteIVariant(QtVariantWriter& Event, const SVarWriteOpt& Opts) const;
	//virtual void WriteMVariant(QtVariantWriter& Event, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const QtVariant& Data);
	//virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	quint64				m_UID = 0;
	quint64             m_TimeStamp = 0;
	ELogLevels			m_EventLevel = ELogLevels::eNone;
	qint64				m_EventType = 0;
	QtVariant			m_EventData;
};

typedef QSharedPointer<CEventLogEntry> CEventLogEntryPtr;

////////////////////////////////////////////////////////////////////////////
// CEventLog

class CEventLog: QObject
{
	Q_OBJECT
public:
	CEventLog(QObject* parent = nullptr);
	virtual ~CEventLog() {}

	STATUS AddEntry(const QtVariant& Entry);
	STATUS LoadEntries(const QtVariant& Entries);

	QList<CEventLogEntryPtr> GetEntries() const { return m_Entries; }
	void ClearLog() { m_Entries.clear(); }

	static QString GetEventLevelStr(ELogLevels Level);
	static QIcon GetEventLevelIcon(ELogLevels Level);
	static QString GetEventInfoStr(const CEventLogEntryPtr& pEntry);

protected:

	static QString GetFwRuleEventInfoStr(ELogEventType Type, const QtVariant& Data);

	QList<CEventLogEntryPtr> m_Entries;
};