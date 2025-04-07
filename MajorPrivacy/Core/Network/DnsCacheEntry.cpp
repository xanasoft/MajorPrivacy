#include "pch.h"
#include "DnsCacheEntry.h"
#include "../../MiscHelpers/Common/Common.h"
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyAPI.h"

CDnsCacheEntry::CDnsCacheEntry(QObject *parent) 
	: QObject(parent)
{
}

QString CDnsCacheEntry::GetTypeString() const
{
	return GetTypeString(GetType());
}

QString CDnsCacheEntry::GetTypeString(quint16 Type)
{
	switch ((EType)Type)
	{
	case EType::A:		return "A";
	case EType::NA:		return "AN";
	case EType::CNAME:	return "CNAME";
	case EType::SOA:	return "SOA";
	case EType::WKS:	return "WKS";
	case EType::PTR:	return "PTR";
	case EType::MX:		return "MX";
	case EType::TXT:	return "TXT";
	case EType::AAAA:	return "AAAA";
	case EType::SRV:	return "SRV";
	default:			return tr("UNKNOWN (%1)").arg(Type);
	}
}

QString CDnsCacheEntry::GetStatusString() const
{
	switch (m_Status)
	{
	case eCached:	return tr("Cached");
	case eAllowed:	return tr("Allowed");
	case eBlocked:	return tr("Blocked");
	default:		return tr("Unknown");
	}
}

/*
void CDnsCacheEntry::RecordProcess(const QString& ProcessName, quint64 ProcessId, const QWeakPointer<QObject>& pProcess, bool bUpdate)
{
	CDnsProcRecordPtr &pRecord = m_ProcessRecords[qMakePair(ProcessName, ProcessId)];
	if (!pRecord)
		pRecord = CDnsProcRecordPtr(new CDnsProcRecord(ProcessName, ProcessId));

	pRecord->Update(pProcess, bUpdate);
}
*/

void CDnsCacheEntry::FromVariant(const class QtVariant& Variant)
{
	QtVariantReader(Variant).ReadRawIndex([&](uint32 Index, const FW::CVariant& vData) {
		const QtVariant& Data = *(QtVariant*)&vData;

		switch (Index)
		{
		case API_V_DNS_HOST:		m_HostName = Data.AsQStr(); break;
		case API_V_DNS_TYPE:		m_Type = Data; break;
		case API_V_DNS_ADDR:		m_Address = QHostAddress(Data.AsQStr()); break;
		case API_V_DNS_DATA:		m_ResolvedString = Data.AsQStr(); break;
		case API_V_DNS_TTL:			m_TTL = Data; break;
		case API_V_DNS_STATUS:		m_Status = (EStatus)Data.To<int>(); break;
		case API_V_DNS_QUERY_COUNT:	m_QueryCounter = Data; break;
		case API_V_CREATE_TIME:		m_CreateTimeStamp = Data; break;
		}
	});
}