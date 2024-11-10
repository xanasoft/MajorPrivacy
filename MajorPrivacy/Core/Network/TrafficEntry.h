#pragma once
#include "../Programs/ProgramID.h"

enum class ENetProtocols
{
	eAny = 0,
	eWeb,
	eTCP,
	eTCP_Server,
	eTCP_Client,
	eUDP,
};

class CTrafficEntry : public QObject
{
	Q_OBJECT
public:
	CTrafficEntry(QObject* parent = NULL);

	void SetHostName(const QString& HostName)	{ m_HostName = HostName; }
	QString GetHostName() const			{ return m_HostName; }
	void SetIpAddress(const QString& IpAddress);
	QString GetIpAddress() const			{ return m_IpAddress; }

	enum ENetType
	{
		eInternet=0,
		eLocalArea,
		eBroadcast,
		eMulticast,
		eLocalHost
	};

	ENetType GetNetType() const			{ return m_Type; }

	quint64 GetLastActivity() const		{ return m_LastActivity; }
	quint64 GetUploadTotal() const		{ return m_UploadTotal; }
	quint64 GetDownloadTotal() const	{ return m_DownloadTotal; }

	void Reset();
	void Merge(const QSharedPointer<CTrafficEntry>& pOther);

	void FromVariant(const class XVariant& TrafficEntry);

protected:

	QString						m_HostName;
	QString						m_IpAddress;
	ENetType					m_Type = eInternet;

	quint64						m_LastActivity = 0;
	quint64						m_UploadTotal = 0;
	quint64						m_DownloadTotal = 0;
};

typedef QSharedPointer<CTrafficEntry> CTrafficEntryPtr;

quint64 CTrafficEntry__LoadList(QMap<QString, CTrafficEntryPtr>& List, const class XVariant& TrafficList);