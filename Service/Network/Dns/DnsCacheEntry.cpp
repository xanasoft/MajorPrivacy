#include "pch.h"
#include "DnsCacheEntry.h"
#include "../../Library/API/PrivacyAPI.h"

CDnsCacheEntry::CDnsCacheEntry(const std::wstring& HostName, uint16 Type, const CAddress& Address, const std::wstring& ResolvedString)
{
	m_CreateTimeStamp = GetCurTime() * 1000;

	m_HostName = HostName;
	m_Type = Type;
	m_Address = Address;
	m_ResolvedString = ResolvedString;
	m_TTL = 0;

	m_QueryCounter = 0;
}

#ifndef DNS_TYPE_A
#define DNS_TYPE_A          0x0001      //  1
#define DNS_TYPE_AAAA       0x001c      //  28
#define DNS_TYPE_PTR        0x000c      //  12
#define DNS_TYPE_CNAME      0x0005      //  5
#define DNS_TYPE_SRV        0x0021      //  33
#define DNS_TYPE_MX         0x000f      //  15
#endif

std::wstring CDnsCacheEntry::GetTypeString() const
{
	return GetTypeString(GetType());
}

std::wstring CDnsCacheEntry::GetTypeString(uint16 Type)
{
	switch (Type)
	{
	case DNS_TYPE_A:	return L"A";
	case DNS_TYPE_AAAA:	return L"AAAA";
	case DNS_TYPE_PTR:	return L"PTR";
	case DNS_TYPE_CNAME:return L"CNAME";
	case DNS_TYPE_SRV:	return L"SRV";
	case DNS_TYPE_MX:	return L"MX";
	default:			return L"UNKNOWN (" + std::to_wstring(Type) + L")";
	}
}

void CDnsCacheEntry::SetTTL(uint64 TTL)
{
	std::unique_lock Lock(m_Mutex);

	if (m_TTL <= 0) {
		//m_CreateTimeStamp = GetTime() * 1000;
		m_RemoveTimeStamp = 0;
		m_QueryCounter++;
	}

	m_TTL = TTL; 
}

void CDnsCacheEntry::SubtractTTL(uint64 Delta)
{ 
	std::unique_lock Lock(m_Mutex);

	if (m_TTL > 0) // in case we flushed the cache and the entries were gone before the TTL expired
		m_TTL = 0;

	m_TTL -= Delta;	
}

CVariant CDnsCacheEntry::ToVariant() const
{
	CVariant Entry;
	Entry.BeginIMap();
	Entry.Write(API_V_DNS_CACHE_REF, (uint64)this);
	Entry.Write(API_V_DNS_HOST, m_HostName);
	Entry.Write(API_V_DNS_TYPE, m_Type);
	Entry.Write(API_V_DNS_ADDR, m_Address.ToString());
	Entry.Write(API_V_DNS_DATA, m_ResolvedString);
	Entry.Write(API_V_DNS_TTL, m_TTL);
	Entry.Write(API_V_DNS_QUERY_COUNT, m_QueryCounter);
	Entry.Finish();
	return Entry;
}