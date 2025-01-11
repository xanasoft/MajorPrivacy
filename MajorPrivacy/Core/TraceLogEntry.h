#pragma once
#include <QSharedData>
#include "../Library/Common/XVariant.h"
#include "../Library/Common/Pointers.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/Common/FlexGuid.h"

class CAbstractLogEntry : public QSharedData
{
public:
	CAbstractLogEntry() {}
	virtual ~CAbstractLogEntry() {}
	
	//virtual CSharedData* Clone() { return new CAbstractLogEntry(*this); }

	virtual quint64 GetUID() const				{ return m_UID; }

	virtual uint64 GetProcessId() const			{ return m_PID; }

	virtual QString GetOwnerService() const		{ return m_ServiceTag; }
	virtual QString GetAppSid() const			{ return m_AppSid; }

	virtual quint64 GetTimeStamp() const		{ return m_TimeStamp; }

	QFlexGuid GetEnclaveGuid() const			{ return m_EnclaveGuid; }

	virtual void FromVariant(const class XVariant& FwEvent);

protected:
	
	virtual void ReadValue(uint32 Index, const XVariant& Data);

	quint64				m_UID = 0; // event reference

	quint64  			m_PID = 0;

	QString				m_ServiceTag;
	QString				m_AppSid;
	QFlexGuid			m_EnclaveGuid;

    quint64             m_TimeStamp = 0;

};

typedef QExplicitlySharedDataPointer<CAbstractLogEntry> CLogEntryPtr;

struct STraceLogList
{
	QVector<CLogEntryPtr>	Entries;
	quint64					MissingCount = -1;
	quint64					IndexOffset = 0;
	quint64					LastGetLog = 0;
};

