#pragma once
#include "../Programs/TraceLogEntry.h"
#include "../Library/Common/Address.h"
#include "Firewall/FirewallDefs.h"
#include "Dns/DnsHostName.h"
#include "../Library/Common/FlexGuid.h"

class CNetLogEntry: public CTraceLogEntry
{
	//TRACK_OBJECT(CNetLogEntry)
public:
#ifdef DEF_USE_POOL
	CNetLogEntry(FW::AbstractMemPool* pMem, const struct SWinFwLogEvent* pEvent, EFwEventStates State, const CHostNamePtr& pRemoteHostName, const std::vector<CFlexGuid>& AllowRules, const std::vector<CFlexGuid>& BlockRules, uint64 PID, const std::wstring& ServiceTag = L"", const std::wstring& AppSid = L"");
#else
	CNetLogEntry(const struct SWinFwLogEvent* pEvent, EFwEventStates State, const CHostNamePtr& pRemoteHostName, const std::vector<CFlexGuid>& AllowRules, const std::vector<CFlexGuid>& BlockRules, uint64 PID, const std::wstring& ServiceTag = L"", const std::wstring& AppSid = L"");
#endif

	virtual void WriteVariant(StVariantWriter& Entry) const;

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

	std::vector<CFlexGuid> m_AllowRules;
	std::vector<CFlexGuid> m_BlockRules;
};

#ifdef DEF_USE_POOL
typedef FW::SharedPtr<CNetLogEntry> CNetLogEntryPtr;
#else
typedef std::shared_ptr<CNetLogEntry> CNetLogEntryPtr;
#endif
