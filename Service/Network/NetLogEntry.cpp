#include "pch.h"
#include "NetLogEntry.h"
#include "Firewall/WindowsFirewall.h"
#include "Firewall/WindowsFwLog.h"
#include "ServiceAPI.h"

CNetLogEntry::CNetLogEntry(const struct SWinFwLogEvent* pEvent, EFwEventStates State, const CHostNamePtr& pRemoteHostName, const std::wstring& ServiceTag, const std::wstring& AppSid)
{
	m_State			= State;
	m_pRemoteHostName = pRemoteHostName;
	m_ServiceTag	= ServiceTag;
	m_AppSid		= AppSid;

	m_Action		= pEvent->Type;
	m_Direction		= pEvent->Direction;
	m_ProtocolType	= pEvent->ProtocolType;
	m_LocalAddress	= pEvent->LocalAddress;
	m_LocalPort		= pEvent->LocalPort;
	m_RemoteAddress	= pEvent->RemoteAddress;
	m_RemotePort	= pEvent->RemotePort;
	m_TimeStamp		= pEvent->TimeStamp;

	// todo: m_Realm
}

void CNetLogEntry::WriteVariant(CVariant& Entry) const
{
	CTraceLogEntry::WriteVariant(Entry);

	switch (m_State)
	{
	case EFwEventStates::FromLog:		Entry.Write(SVC_API_EVENT_STATE, SVC_API_EVENT_FROM_LOG); break;
    case EFwEventStates::UnRuled:		Entry.Write(SVC_API_EVENT_STATE, SVC_API_EVENT_UNRULED); break;
    case EFwEventStates::RuleAllowed:	Entry.Write(SVC_API_EVENT_STATE, SVC_API_EVENT_ALLOWED); break;
    case EFwEventStates::RuleBlocked:	Entry.Write(SVC_API_EVENT_STATE, SVC_API_EVENT_BLOCKED); break;
    case EFwEventStates::RuleError:		Entry.Write(SVC_API_EVENT_STATE, SVC_API_EVENT_FAILED); break;
	default:							Entry.Write(SVC_API_EVENT_STATE, SVC_API_EVENT_INVALID); break;
	}

	switch (m_Action) {
	case EFwActions::Allow:				Entry.Write(SVC_API_FW_ACTION, SVC_API_FW_ALLOW); break;
	case EFwActions::Block:				Entry.Write(SVC_API_FW_ACTION, SVC_API_FW_BLOCK); break;
	}

	switch (m_Direction) {
	case EFwDirections::Inbound:		Entry.Write(SVC_API_FW_DIRECTION, SVC_API_FW_INBOUND); break;
	case EFwDirections::Outbound:		Entry.Write(SVC_API_FW_DIRECTION, SVC_API_FW_OUTBOUND); break;
	}
	Entry.Write(SVC_API_FW_PROTOCOL, (uint32)m_ProtocolType);
	Entry.Write(SVC_API_FW_LOCAL_ADDR, m_LocalAddress.ToString());
	Entry.Write(SVC_API_FW_LOCAL_PORT, m_LocalPort);
	Entry.Write(SVC_API_FW_REMOTE_ADDR, m_RemoteAddress.ToString());
	Entry.Write(SVC_API_FW_REMOTE_PORT, m_RemotePort);
	Entry.Write(SVC_API_FW_REMOTE_HOST, m_pRemoteHostName ? m_pRemoteHostName->ToString() : L"");

    //m_Realm // todo
}