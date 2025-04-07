#pragma once
#include <qobject.h>

class CDnsCacheEntry: public QObject
{
	Q_OBJECT

public:
	enum EStatus
	{
		eNone = 0,
		eCached = 1,
		eAllowed = 2,
		eBlocked = 4,
	};

	enum class EType
	{
		None = 0,
		A = 1,
		NA = 2,
		CNAME = 5,
		SOA = 6,
		WKS = 11,
		PTR = 12,
		MX = 15,
		TXT = 16,
		AAAA = 28,
		SRV = 33,
		ANY = 255,
	};

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

	virtual EStatus GetStatus() const				{ return m_Status; }
	virtual QString GetStatusString() const;
	
	virtual quint64 GetQueryCounter() const			{ return m_QueryCounter; }

	/*virtual void RecordProcess(const QString& ProcessName, quint64 ProcessId, const QWeakPointer<QObject>& pProcess, bool bUpdate = false);
	virtual QMap<QPair<QString, quint64>, CDnsProcRecordPtr> GetProcessRecords() const	{ return m_ProcessRecords; }*/

	virtual void FromVariant(const class QtVariant& Variant);

protected:

	uint64			m_CreateTimeStamp = 0;
	uint64			m_RemoveTimeStamp = 0;

	QString			m_HostName;
	QString			m_ResolvedString;
	QHostAddress	m_Address;
	quint16			m_Type = 0;
	qint64			m_TTL = 0;

	EStatus			m_Status = eNone;

	quint32			m_QueryCounter = 0;

	//QMap<QPair<QString, quint64>, CDnsProcRecordPtr>	m_ProcessRecords;
};

typedef QSharedPointer<CDnsCacheEntry> CDnsCacheEntryPtr;
typedef QWeakPointer<CDnsCacheEntry> CDnsCacheEntryRef;