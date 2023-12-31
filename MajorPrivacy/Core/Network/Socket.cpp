#include "pch.h"
#include "Socket.h"
#include "../Service/ServiceAPI.h"

CSocket::CSocket(QObject* parent)
	: QObject(parent)
{
}

void CSocket::FromVariant(const class XVariant& Socket)
{
	Socket.ReadRawMap([&](const SVarName& Name, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;

			 if (VAR_TEST_NAME(Name, SVC_API_SOCK_REF))			m_SocketRef = Data;

		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_TYPE))		m_ProtocolType = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_LADDR))		m_LocalAddress = Data.AsQStr();
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_LPORT))		m_LocalPort = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_RADDR))		m_RemoteAddress = Data.AsQStr();
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_RPORT))		m_RemotePort = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_STATE))		m_State = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_LSCOPE))		m_LocalScopeId = Data; // Ipv6
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_RSCOPE))		m_RemoteScopeId = Data; // Ipv6

		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_PID))			m_ProcessId = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_SVC_TAG))		m_OwnerService = Data.AsQStr();

		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_RHOST))		m_RemoteHostName = Data.AsQStr();

		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_CREATED))		m_CreateTimeStamp = Data;

		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_LAST_ACT))	m_LastActivity = Data;

		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_UPLOAD))		m_UploadRate = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_DOWNLOAD))	m_DownloadRate = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_UPLOADED))	m_UploadTotal = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_SOCK_DOWNLOADED))	m_DownloadTotal = Data;

		// else unknown tag

	});
}