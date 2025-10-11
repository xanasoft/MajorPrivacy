#pragma once
#include "../Programs/ProgramID.h"

class CHandle : public QObject
{
	Q_OBJECT
	TRACK_OBJECT(CHandle)
public:
	CHandle(QObject* parent = NULL);

	quint64 GetHandleRef() const			{ return m_HandleRef; }

	QString GetNtPath() const				{ QReadLocker Lock(&m_Mutex); return m_NtPath; }

	quint64 GetProcessId() const			{ return m_ProcessId; }

	void FromVariant(const class QtVariant& Handle);

protected:

	mutable QReadWriteLock m_Mutex;

	quint64						m_HandleRef = 0;

    QString						m_NtPath;

	quint64						m_ProcessId = 0;
};

typedef QSharedPointer<CHandle> CHandlePtr;