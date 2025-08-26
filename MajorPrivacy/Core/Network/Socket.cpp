#include "pch.h"
#include "Socket.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Service/Network/Firewall/FirewallDefs.h"

CSocket::CSocket(QObject* parent)
	: QObject(parent)
{
}

QString CSocket::GetStateString() const
{
	if (m_ProtocolType != (quint32)EFwKnownProtocols::TCP)
	{
		if(m_State == MIB_TCP_STATE_CLOSED)
			return tr("Closed");
		return tr("Open");
	}

	// all these are TCP states
	switch (m_State)
	{
	case MIB_TCP_STATE_CLOSED:		return tr("Closed");
	case MIB_TCP_STATE_LISTEN:		return tr("Listen");
	case MIB_TCP_STATE_SYN_SENT:	return tr("SYN sent");
	case MIB_TCP_STATE_SYN_RCVD:	return tr("SYN received");
	case MIB_TCP_STATE_ESTAB:		return tr("Established");
	case MIB_TCP_STATE_FIN_WAIT1:	return tr("FIN wait 1");
	case MIB_TCP_STATE_FIN_WAIT2:	return tr("FIN wait 2");
	case MIB_TCP_STATE_CLOSE_WAIT:	return tr("Close wait");
	case MIB_TCP_STATE_CLOSING:		return tr("Closing");
	case MIB_TCP_STATE_LAST_ACK:	return tr("Last ACK");
	case MIB_TCP_STATE_TIME_WAIT:	return tr("Time wait");
	case MIB_TCP_STATE_DELETE_TCB:	return tr("Delete TCB");
	case -1u:						return tr("Blocked");
	default:						return tr("Unknown %1").arg(m_State);
	}
}

void CSocket::FromVariant(const class QtVariant& Socket)
{
	QWriteLocker Lock(&m_Mutex);

	QtVariantReader(Socket).ReadRawIndex([&](uint32 Index, const FW::CVariant& vData) {
		const QtVariant& Data = *(QtVariant*)&vData;

		switch (Index)
		{
		case API_V_SOCK_REF:		m_SocketRef = Data; break;

		case API_V_SOCK_TYPE:		m_ProtocolType = Data; break;
		case API_V_SOCK_LADDR:		m_LocalAddress = QHostAddress(Data.AsQStr()); break;
		case API_V_SOCK_LPORT:		m_LocalPort = Data; break;
		case API_V_SOCK_RADDR:		m_RemoteAddress = QHostAddress(Data.AsQStr()); break;
		case API_V_SOCK_RPORT:		m_RemotePort = Data; break;
		case API_V_SOCK_STATE:		m_State = Data; break;
		case API_V_SOCK_LSCOPE:		m_LocalScopeId = Data; break; // Ipv6
		case API_V_SOCK_RSCOPE:		m_RemoteScopeId = Data; break; // Ipv6

		case API_V_PID:				m_ProcessId = Data; break;
		case API_V_SERVICE_TAG:			m_OwnerService = Data.AsQStr(); break;

		case API_V_SOCK_RHOST:		m_RemoteHostName = Data.AsQStr(); break;

		case API_V_CREATE_TIME:		m_CreateTimeStamp = Data; break;

		case API_V_SOCK_LAST_NET_ACT: m_LastActivity = Data; break;

		case API_V_SOCK_UPLOAD:		m_UploadRate = Data; break;
		case API_V_SOCK_DOWNLOAD:	m_DownloadRate = Data; break;
		case API_V_SOCK_UPLOADED:	m_UploadTotal = Data; break;
		case API_V_SOCK_DOWNLOADED:	m_DownloadTotal = Data; break;
		}
	});
}