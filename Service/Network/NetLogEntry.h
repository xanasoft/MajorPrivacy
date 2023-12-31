#pragma once
#include "../Programs/TraceLogEntry.h"
#include "../Library/Common/Address.h"
#include "Firewall/FirewallDefs.h"
#include "Dns/DnsHostName.h"

class CNetLogEntry: public CTraceLogEntry
{
public:
	CNetLogEntry(const struct SWinFwLogEvent* pEvent, EFwEventStates State, const CHostNamePtr& pRemoteHostName, const std::wstring& ServiceTag = L"", const std::wstring& AppSid = L"");

	virtual void WriteVariant(CVariant& Entry) const;

protected:

	EFwEventStates      m_State = EFwEventStates::Undefined;

	EFwActions			m_Action = EFwActions::Undefined;
    EFwDirections       m_Direction = EFwDirections::Unknown;
	EFwKnownProtocols   m_ProtocolType = EFwKnownProtocols::Any;
	CAddress            m_LocalAddress;
	uint16              m_LocalPort = 0;
	CAddress            m_RemoteAddress;
	uint16              m_RemotePort = 0;

    EFwRealms			m_Realm = EFwRealms::Undefined;

	CHostNamePtr		m_pRemoteHostName;
};

typedef std::shared_ptr<CNetLogEntry> CNetLogEntryPtr;
