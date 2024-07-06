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
		case API_V_SOCK_LAST_ACT:	m_LastActivity = Data; break;
		case API_V_SOCK_UPLOADED:	m_UploadTotal = Data; break;
		case API_V_SOCK_DOWNLOADED:	m_DownloadTotal = Data; break;
		}
	});
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