#include "pch.h"
#include "TrafficEntry.h"
#include "../Library/API/PrivacyAPI.h"

CTrafficEntry::CTrafficEntry(QObject* parent)
	: QObject(parent)
{
}

void CTrafficEntry::Reset()
{
	m_LastActivity = 0;

	m_UploadTotal = 0;
	m_DownloadTotal = 0;
}

void CTrafficEntry::Merge(const QSharedPointer<CTrafficEntry>& pOther)
{
	if (pOther->m_LastActivity > m_LastActivity)
		m_LastActivity = pOther->m_LastActivity;

	m_UploadTotal += pOther->m_UploadTotal;
	m_DownloadTotal += pOther->m_DownloadTotal;
}

void CTrafficEntry::FromVariant(const class XVariant& TrafficEntry)
{
	TrafficEntry.ReadRawIMap([&](uint32 Index, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;

		switch (Index)
		{
		case API_V_SOCK_RHOST:		m_HostName = Data.AsQStr(); break;
		case API_V_SOCK_RADDR:		SetIpAddress(Data.AsQStr()); break;
		case API_V_SOCK_LAST_NET_ACT: m_LastActivity = Data; break;
		case API_V_SOCK_UPLOADED:	m_UploadTotal = Data; break;
		case API_V_SOCK_DOWNLOADED:	m_DownloadTotal = Data; break;
		}
	});
}

CTrafficEntry::ENetType CTrafficEntry::GetNetType(const QHostAddress& address)
{
    if (address.isNull())
    {
        // Invalid IP address; default to Internet
        return eInternet;
    }
    else if (address.isLoopback())
    {
        return eLocalHost;
    }
    else if (address.protocol() == QAbstractSocket::IPv4Protocol)
    {
        quint32 ipv4 = address.toIPv4Address();

        // Check for broadcast address 255.255.255.255
        if (ipv4 == 0xFFFFFFFF)
        {
            return eBroadcast;
        }
        // Check for multicast addresses 224.0.0.0/4
        else if ((ipv4 & 0xF0000000) == 0xE0000000)
        {
            return eMulticast;
        }
        // Check for local area networks
        else if (
            // 10.0.0.0/8
            (ipv4 & 0xFF000000) == 0x0A000000 ||
            // 172.16.0.0/12
            (ipv4 & 0xFFF00000) == 0xAC100000 ||
            // 192.168.0.0/16
            (ipv4 & 0xFFFF0000) == 0xC0A80000 ||
            // Link-local 169.254.0.0/16
            (ipv4 & 0xFFFF0000) == 0xA9FE0000
            )
        {
            return eLocalArea;
        }
        else
        {
            return eInternet;
        }
    }
    else if (address.protocol() == QAbstractSocket::IPv6Protocol)
    {
        Q_IPV6ADDR ipv6 = address.toIPv6Address();

        // Check for multicast addresses FF00::/8
        if (ipv6[0] == 0xFF)
        {
            return eMulticast;
        }
        // Check for local area networks
        else if (
            // Link-local addresses FE80::/10
            (ipv6[0] == 0xFE && (ipv6[1] & 0xC0) == 0x80) ||
            // Unique local addresses FC00::/7
            ((ipv6[0] & 0xFE) == 0xFC)
            )
        {
            return eLocalArea;
        }
        else
        {
            return eInternet;
        }
    }
    else
    {
        // Default to Internet for other protocols
        return eInternet;
    }
}

void CTrafficEntry::SetIpAddress(const QString& IpAddress)
{ 
	if(m_IpAddress == IpAddress)
		return;

	m_IpAddress = IpAddress;
	
    m_Type = GetNetType(QHostAddress(IpAddress));
}

quint64 CTrafficEntry__LoadList(QMap<QString, CTrafficEntryPtr>& List, const class XVariant& TrafficList)
{
	quint64 LastActivity = 0;

	QMap<QString, CTrafficEntryPtr> OldList = List;

	TrafficList.ReadRawList([&](const CVariant& vData) {
		const XVariant& TrafficEntry = *(XVariant*)&vData;

		QString HostName = TrafficEntry.Find(API_V_SOCK_RHOST).AsQStr();

		CTrafficEntryPtr pEntry = OldList.take(HostName);
		if (!pEntry) {
			pEntry = CTrafficEntryPtr(new CTrafficEntry());
			List.insert(HostName, pEntry);
		}

		pEntry->FromVariant(TrafficEntry);

		if (pEntry->GetLastActivity() > LastActivity)
			LastActivity = pEntry->GetLastActivity();
	});

	// we update the list incrementaly
	//foreach(const QString& HostName, OldList.keys())
	//	List.remove(HostName);

	return LastActivity;
}