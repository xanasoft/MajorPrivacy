#include "pch.h"
#include "NetLogEntry.h"
#include "../Library/API/PrivacyAPI.h"
#include "FwRule.h"

QString CNetLogEntry::GetStateStr() const 
{ 
	return CFwRule::StateToStr(m_State);
}

QString CNetLogEntry::GetActionStr() const 
{ 
	return CFwRule::ActionToStr(m_Action);
}

QString CNetLogEntry::GetDirectionStr() const 
{ 
	return CFwRule::DirectionToStr(m_Direction);
}

QString CNetLogEntry::GetProtocolTypeStr() const 
{ 
	return CFwRule::ProtocolToStr((EFwKnownProtocols)m_ProtocolType);
}

void CNetLogEntry::ReadValue(uint32 Index, const XVariant& Data)
{
	switch (Index) {
	case API_V_FW_EVENT_STATE:			m_State = (EFwEventStates)Data.To<uint32>(); break;

	case API_V_FW_RULE_ACTION:			m_Action = (EFwActions)Data.To<uint32>(); break;
	case API_V_FW_RULE_DIRECTION:		m_Direction = (EFwDirections)Data.To<uint32>(); break;
	case API_V_FW_RULE_PROTOCOL:		m_ProtocolType = Data; break;

	case API_V_FW_RULE_LOCAL_ADDR:		m_LocalAddress = Data.AsQStr(); break;
	case API_V_FW_RULE_LOCAL_PORT:		m_LocalPort = Data; break;
	case API_V_FW_RULE_REMOTE_ADDR:		m_RemoteAddress = Data.AsQStr(); break;
	case API_V_FW_RULE_REMOTE_PORT:		m_RemotePort = Data; break;
	case API_V_FW_RULE_REMOTE_HOST:		m_RemoteHostName = Data.AsQStr(); break;

	case API_V_FW_ALLOW_RULES:			m_AllowRules = QFlexGuid::ReadVariantSet(Data); break;
	case API_V_FW_BLOCK_RULES:			m_BlockRules = QFlexGuid::ReadVariantSet(Data); break;

	default: CAbstractLogEntry::ReadValue(Index, Data);
	}
}

bool CNetLogEntry::Match(const CAbstractLogEntry* pEntry) const
{
	if (!CAbstractLogEntry::Match(pEntry))
		return false;

	const CNetLogEntry* pNetEntry = dynamic_cast<const CNetLogEntry*>(pEntry);
	if (!pNetEntry)
		return false;

	if (m_State != pNetEntry->m_State)
		return false;
	if (m_Action != pNetEntry->m_Action)
		return false;
	if (m_Direction != pNetEntry->m_Direction)
		return false;
	if (m_ProtocolType != pNetEntry->m_ProtocolType)
		return false;
	if (m_LocalAddress != pNetEntry->m_LocalAddress)
		return false;
	if (m_LocalPort != pNetEntry->m_LocalPort && !(m_Direction == EFwDirections::Outbound && m_ProtocolType == (quint32)EFwKnownProtocols::TCP)) // tcp uses random outgoing ports
		return false;
	if (m_RemoteAddress != pNetEntry->m_RemoteAddress)
		return false;
	if (m_RemotePort != pNetEntry->m_RemotePort)
		return false;
	if (m_RemoteHostName != pNetEntry->m_RemoteHostName)
		return false;

	return true;
}