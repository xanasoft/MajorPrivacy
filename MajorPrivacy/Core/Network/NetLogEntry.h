#pragma once
#include "../TraceLogEntry.h"

class CNetLogEntry : public CAbstractLogEntry
{
public:
	CNetLogEntry() {}

	//virtual CSharedData* Clone() { return new CNetLogEntry(*this); }

	QString GetState() const { return m_State; }

	QString GetAction() const { return m_Action; }
	QString GetDirection() const { return m_Direction; }
	quint32	GetProtocolType() const { return m_ProtocolType; }
	QHostAddress GetLocalAddress() const { return m_LocalAddress; }
	quint16 GetLocalPort() const { return m_LocalPort; }
	QHostAddress GetRemoteAddress() const { return m_RemoteAddress; }
	quint16 GetRemotePort() const { return m_RemotePort; }
	QString GetRemoteHostName() const { return m_RemoteHostName; }

	QString GetRealm() const { return m_Realm; }

protected:

	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	QString				m_State;

	QString				m_Action;
    QString				m_Direction;
	quint32				m_ProtocolType = 0;
	QHostAddress        m_LocalAddress;
	quint16             m_LocalPort = 0;
	QHostAddress		m_RemoteAddress;
	quint16             m_RemotePort = 0;
	QString				m_RemoteHostName;

    QString				m_Realm;
};

