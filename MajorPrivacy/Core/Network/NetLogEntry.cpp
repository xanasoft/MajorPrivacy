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