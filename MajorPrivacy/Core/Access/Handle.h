#pragma once
#include "../Programs/ProgramID.h"

class CHandle : public QObject
{
	Q_OBJECT
public:
	CHandle(QObject* parent = NULL);

	quint64 GetHandleRef() const			{ return m_HandleRef; }

	QString GetNtPath() const				{ return m_NtPath; }

	quint64 GetProcessId() const			{ return m_ProcessId; }

	void FromVariant(const class XVariant& Handle);

protected:

	quint64						m_HandleRef = 0;

    QString						m_NtPath;

	quint64						m_ProcessId = 0;
};

typedef QSharedPointer<CHandle> CHandlePtr;