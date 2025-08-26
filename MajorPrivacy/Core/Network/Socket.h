#pragma once
#include "../Programs/ProgramID.h"

#ifndef MIB_TCP_STATE
typedef enum {
	MIB_TCP_STATE_CLOSED     =  1,
	MIB_TCP_STATE_LISTEN     =  2,
	MIB_TCP_STATE_SYN_SENT   =  3,
	MIB_TCP_STATE_SYN_RCVD   =  4,
	MIB_TCP_STATE_ESTAB      =  5,
	MIB_TCP_STATE_FIN_WAIT1  =  6,
	MIB_TCP_STATE_FIN_WAIT2  =  7,
	MIB_TCP_STATE_CLOSE_WAIT =  8,
	MIB_TCP_STATE_CLOSING    =  9,
	MIB_TCP_STATE_LAST_ACK   = 10,
	MIB_TCP_STATE_TIME_WAIT  = 11,
	MIB_TCP_STATE_DELETE_TCB = 12,
	//
	// Extra TCP states not defined in the MIB
	//
	MIB_TCP_STATE_RESERVED      = 100
} MIB_TCP_STATE;
#endif

class CSocket : public QObject
{
	Q_OBJECT
public:
	CSocket(QObject* parent = NULL);

	quint64 GetSocketRef() const		{ return m_SocketRef; }

    quint32 GetProtocolType() const		{ return m_ProtocolType; }
	QHostAddress GetLocalAddress() const{ QReadLocker Lock(&m_Mutex); return m_LocalAddress; }
	quint16 GetLocalPort() const		{ return m_LocalPort; }
	QHostAddress GetRemoteAddress() const { QReadLocker Lock(&m_Mutex); return m_RemoteAddress; }
	quint16 GetRemotePort() const		{ return m_RemotePort; }
	quint32 GetState() const			{ return m_State; }
	QString GetStateString() const;
	uint32 GetLocalScopeId() const		{ return m_LocalScopeId; }
    uint32 GetRemoteScopeId() const		{ return m_RemoteScopeId; }

	quint64 GetProcessId() const		{ return m_ProcessId; }
	QString GetOwnerService() const		{ QReadLocker Lock(&m_Mutex); return m_OwnerService; }

	QString GetRemoteHostName() const	{ QReadLocker Lock(&m_Mutex); return m_RemoteHostName; }

	quint64 GetCreateTimeStamp() const	{ return m_CreateTimeStamp; }

	quint64 GetLastActivity() const		{ return m_LastActivity; }
	quint64 GetUploadRate() const		{ return m_UploadRate; }
	quint64 GetDownloadRate() const		{ return m_DownloadRate; }
	quint64 GetUploadTotal() const		{ return m_UploadTotal; }
	quint64 GetDownloadTotal() const	{ return m_DownloadTotal; }

	void FromVariant(const class QtVariant& Socket);

protected:

	mutable QReadWriteLock m_Mutex;

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