#pragma once
#include "../TraceLogEntry.h"
#include "../../Service\Network\Firewall\FirewallDefs.h"
#include "../../Library/Common/FlexGuid.h"

class CNetLogEntry : public CAbstractLogEntry
{
public:
	CNetLogEntry() {}

	//virtual CSharedData* Clone() { return new CNetLogEntry(*this); }

	EFwEventStates GetState() const { return m_State; }
	QString GetStateStr() const;

	EFwActions GetAction() const { return m_Action; }
	QString GetActionStr() const;

	EFwDirections GetDirection() const { return m_Direction; }
	QString GetDirectionStr() const;

	quint32	GetProtocolType() const { return m_ProtocolType; }
	QString GetProtocolTypeStr() const;
	QHostAddress GetLocalAddress() const { return m_LocalAddress; }
	quint16 GetLocalPort() const { return m_LocalPort; }
	QHostAddress GetRemoteAddress() const { return m_RemoteAddress; }
	quint16 GetRemotePort() const { return m_RemotePort; }
	QString GetRemoteHostName() const { return m_RemoteHostName; }

	QString GetRealm() const { return m_Realm; }

	const QSet<QFlexGuid>& GetAllowRules() const { return m_AllowRules; }
	const QSet<QFlexGuid>& GetBlockRules() const { return m_BlockRules; }

protected:

	virtual void ReadValue(uint32 Index, const XVariant& Data);

	EFwEventStates      m_State = EFwEventStates::Undefined;

	EFwActions			m_Action = EFwActions::Undefined;
	EFwDirections       m_Direction = EFwDirections::Unknown;
	quint32				m_ProtocolType = 0;
	QHostAddress        m_LocalAddress;
	quint16             m_LocalPort = 0;
	QHostAddress		m_RemoteAddress;
	quint16             m_RemotePort = 0;
	QString				m_RemoteHostName;

    QString				m_Realm;

	QSet<QFlexGuid>		m_AllowRules;
	QSet<QFlexGuid>		m_BlockRules;
};

