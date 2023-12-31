#pragma once
#include "../Programs/ProgramID.h"

class CSocket : public QObject
{
	Q_OBJECT
public:
	CSocket(QObject* parent = NULL);

	quint64 GetSocketRef() const		{ return m_SocketRef; }

    quint32 GetProtocolType() const		{ return m_ProtocolType; }
	QHostAddress GetLocalAddress() const{ return m_LocalAddress; }
	quint16 GetLocalPort() const		{ return m_LocalPort; }
	QHostAddress GetRemoteAddress() const { return m_RemoteAddress; }
	quint16 GetRemotePort() const		{ return m_RemotePort; }
	quint32 GetState() const			{ return m_State; }
	uint32 GetLocalScopeId() const		{ return m_LocalScopeId; }
    uint32 GetRemoteScopeId() const		{ return m_RemoteScopeId; }

	quint64 GetProcessId() const		{ return m_ProcessId; }
	QString GetOwnerService() const		{ return m_OwnerService; }

	QString GetRemoteHostName() const	{ return m_RemoteHostName; }

	quint64 GetCreateTimeStamp() const	{ return m_CreateTimeStamp; }

	quint64 GetLastActivity() const		{ return m_LastActivity; }
	quint64 GetUploadRate() const		{ return m_UploadRate; }
	quint64 GetDownloadRate() const		{ return m_DownloadRate; }
	quint64 GetUploadTotal() const		{ return m_UploadTotal; }
	quint64 GetDownloadTotal() const	{ return m_DownloadTotal; }

	void FromVariant(const class XVariant& Socket);

protected:

	quint64						m_SocketRef = 0;

    quint32						m_ProtocolType = 0;
	QHostAddress				m_LocalAddress;
	quint16						m_LocalPort = 0;
	QHostAddress				m_RemoteAddress;
	quint16						m_RemotePort = 0;
	quint32						m_State = 0;
	uint32						m_LocalScopeId = 0; // Ipv6
    uint32						m_RemoteScopeId = 0; // Ipv6

	quint64						m_ProcessId = 0;
	QString						m_OwnerService;

	QString						m_RemoteHostName;

	quint64						m_CreateTimeStamp = 0;

	quint64						m_LastActivity = 0;
	quint64						m_UploadRate = 0;
	quint64						m_DownloadRate = 0;
	quint64						m_UploadTotal = 0;
	quint64						m_DownloadTotal = 0;
};

typedef QSharedPointer<CSocket> CSocketPtr;