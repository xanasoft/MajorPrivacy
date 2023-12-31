#pragma once
#include <QSharedData>
#include "../Library/Common/XVariant.h"
#include "../Library/Common/Pointers.h"

class CAbstractLogEntry : public QSharedData
{
public:
	CAbstractLogEntry() {}
	virtual ~CAbstractLogEntry() {}
	
	//virtual CSharedData* Clone() { return new CAbstractLogEntry(*this); }

	QString GetOwnerService() const { return m_ServiceTag; }
	QString GetAppSid() const { return m_AppSid; }

	virtual quint64 GetUID() const { return m_UID; }
	virtual quint64 GetTimeStamp() const { return m_TimeStamp; }

	virtual void FromVariant(const class XVariant& FwEvent);

protected:
	
	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	quint64				m_UID = 0;

	QString				m_ServiceTag;
	QString				m_AppSid;

    quint64             m_TimeStamp = 0;

};

typedef QExplicitlySharedDataPointer<CAbstractLogEntry> CLogEntryPtr;


enum class ETraceLogs
{
	eExecLog = 0,
	eNetLog,
	eFSLog,
	eRegLog,
	eLogMax
};

struct STraceLogList
{
	QVector<CLogEntryPtr>	Entries;
	int						MissingIndex = -1;
};

