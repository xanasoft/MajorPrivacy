#include "pch.h"
#include "DnsCacheEntry.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../../Library/Common/XVariant.h"
#include "../Service/ServiceAPI.h"

CDnsCacheEntry::CDnsCacheEntry(QObject *parent) 
	: QObject(parent)
{
}

#ifndef DNS_TYPE_A
#define DNS_TYPE_A          0x0001      //  1
#define DNS_TYPE_AAAA       0x001c      //  28
#define DNS_TYPE_PTR        0x000c      //  12
#define DNS_TYPE_CNAME      0x0005      //  5
#define DNS_TYPE_SRV        0x0021      //  33
#define DNS_TYPE_MX         0x000f      //  15
#endif

QString CDnsCacheEntry::GetTypeString() const
{
	return GetTypeString(GetType());
}

QString CDnsCacheEntry::GetTypeString(quint16 Type)
{
	switch (Type)
	{
		case DNS_TYPE_A:	return "A";
		case DNS_TYPE_AAAA:	return "AAAA";
		case DNS_TYPE_PTR:	return "PTR";
		case DNS_TYPE_CNAME:return "CNAME";
		case DNS_TYPE_SRV:	return "SRV";
		case DNS_TYPE_MX:	return "MX";
		default:			return QString("UNKNOWN (%1)").arg(Type);
	}
}

void CDnsCacheEntry::SetTTL(quint64 TTL)
{
	if (m_TTL <= 0) {
		//m_CreateTimeStamp = GetTime() * 1000;
		m_RemoveTimeStamp = 0;
		m_QueryCounter++;
	}

	m_TTL = TTL; 
}

void CDnsCacheEntry::SubtractTTL(quint64 Delta)
{ 
	if (m_TTL > 0) // in case we flushed the cache and the entries were gone before the TTL expired
		m_TTL = 0;

	m_TTL -= Delta;	
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

void CDnsCacheEntry::FromVariant(const class XVariant& Variant)
{
	Variant.ReadRawMap([&](const SVarName& Name, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;

			 if (VAR_TEST_NAME(Name, SVC_API_NET_HOST))			m_HostName = Data.AsQStr();
		else if (VAR_TEST_NAME(Name, SVC_API_DNS_TYPE))			m_Type = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_DNS_ADDR))			m_Address = QHostAddress(Data.AsQStr());
		else if (VAR_TEST_NAME(Name, SVC_API_DNS_DATA))			m_ResolvedString = Data.AsQStr();
		else if (VAR_TEST_NAME(Name, SVC_API_DNS_TTL))			m_TTL = Data;
		else if (VAR_TEST_NAME(Name, SVC_API_DNS_QUERY_COUNT))	m_QueryCounter = Data;

		// else unknown tag

	});
}