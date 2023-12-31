#pragma once
#include <qobject.h>

class CDnsCacheEntry: public QObject
{
	Q_OBJECT

public:
	CDnsCacheEntry(QObject *parent = nullptr);

	virtual  quint64 GetCreateTimeStamp() const		{ return m_CreateTimeStamp; }
	virtual QString GetHostName() const				{ return m_HostName; }
	virtual QString GetResolvedString() const		{ return m_ResolvedString; }
	virtual QHostAddress GetAddress() const			{ return m_Address; }
	virtual quint16 GetType() const					{ return m_Type; }
	virtual QString GetTypeString() const;
	static QString GetTypeString(quint16 Type);

	virtual quint64 GetTTL() const					{ return m_TTL > 0 ? m_TTL : 0; }
	virtual quint64 GetDeadTime() const				{ return m_TTL < 0 ? -m_TTL : 0; }
	virtual void SetTTL(quint64 TTL);
	virtual void SubtractTTL(quint64 Delta);
	
	virtual quint64 GetQueryCounter() const			{ return m_QueryCounter; }

	/*virtual void RecordProcess(const QString& ProcessName, quint64 ProcessId, const QWeakPointer<QObject>& pProcess, bool bUpdate = false);
	virtual QMap<QPair<QString, quint64>, CDnsProcRecordPtr> GetProcessRecords() const	{ return m_ProcessRecords; }*/

	virtual void FromVariant(const class XVariant& Variant);

protected:

	uint64			m_CreateTimeStamp = 0;
	uint64			m_RemoveTimeStamp = 0;

	QString			m_HostName;
	QString			m_ResolvedString;
	QHostAddress	m_Address;
	quint16			m_Type = 0;
	qint64			m_TTL = 0;

	quint32			m_QueryCounter = 0;

	//QMap<QPair<QString, quint64>, CDnsProcRecordPtr>	m_ProcessRecords;
};

typedef QSharedPointer<CDnsCacheEntry> CDnsCacheEntryPtr;
typedef QWeakPointer<CDnsCacheEntry> CDnsCacheEntryRef;