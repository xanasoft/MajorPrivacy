#include "pch.h"
#include "NetLogEntry.h"
#include "../Service/ServiceAPI.h"


void CNetLogEntry::ReadValue(const SVarName& Name, const XVariant& Data)
{
		 if (VAR_TEST_NAME(Name, SVC_API_EVENT_STATE))		m_State = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, SVC_API_FW_ACTION))		m_Action = Data.AsQStr();
	else if (VAR_TEST_NAME(Name, SVC_API_FW_DIRECTION))		m_Direction = Data.AsQStr();
	else if (VAR_TEST_NAME(Name, SVC_API_FW_PROTOCOL))		m_ProtocolType = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_FW_LOCAL_ADDR))	m_LocalAddress = Data.AsQStr();
	else if (VAR_TEST_NAME(Name, SVC_API_FW_LOCAL_PORT))	m_LocalPort = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_FW_REMOTE_ADDR))	m_RemoteAddress = Data.AsQStr();
	else if (VAR_TEST_NAME(Name, SVC_API_FW_REMOTE_PORT))	m_RemotePort = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_FW_REMOTE_HOST))	m_RemoteHostName = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, SVC_API_EVENT_REALM))		m_Realm = Data.AsQStr();

	else 
		CAbstractLogEntry::ReadValue(Name, Data);
}