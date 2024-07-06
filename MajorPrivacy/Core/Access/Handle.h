#pragma once
#include "../Programs/ProgramID.h"
#include "../../Helpers/FilePath.h"

class CHandle : public QObject
{
	Q_OBJECT
public:
	CHandle(QObject* parent = NULL);

	quint64 GetHandleRef() const			{ return m_HandleRef; }

	QString GetPath(EPathType Type) const	{ return m_Path.Get(Type); }

	quint64 GetProcessId() const			{ return m_ProcessId; }

	void FromVariant(const class XVariant& Handle);

protected:

	quint64						m_HandleRef = 0;

    CFilePath					m_Path;

	quint64						m_ProcessId = 0;
};

typedef QSharedPointer<CHandle> CHandlePtr;