#include "pch.h"
#include "DnsCacheEntry.h"
#include "../../Library/API/PrivacyAPI.h"

CDnsCacheEntry::CDnsCacheEntry(const std::wstring& HostName, uint16 Type, const CAddress& Address, const std::wstring& ResolvedString, EStatus Status)
{
	m_CreateTimeStamp = GetCurTime() * 1000;

	m_HostName = HostName;
	m_Type = Type;
	m_Address = Address;
	m_ResolvedString = ResolvedString;
	m_TTL = 0;

	m_Status = Status;

	m_QueryCounter = 0;
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

void CDnsCacheEntry::SubtractTTL(uint64 Delta, bool bKill)
{ 
	std::unique_lock Lock(m_Mutex);

	if (bKill && m_TTL > 0) // in case we flushed the cache and the entries were gone before the TTL expired
		m_TTL = 0;

	m_TTL -= Delta;	
}

StVariant CDnsCacheEntry::ToVariant(FW::AbstractMemPool* pMemPool) const
{
	StVariantWriter Entry(pMemPool);
	Entry.BeginIndex();
	Entry.Write(API_V_DNS_CACHE_REF, (uint64)this);
	Entry.WriteEx(API_V_DNS_HOST, m_HostName);
	Entry.Write(API_V_DNS_TYPE, m_Type);
	Entry.WriteEx(API_V_DNS_ADDR, m_Address.ToString());
	Entry.WriteEx(API_V_DNS_DATA, m_ResolvedString);
	Entry.Write(API_V_DNS_TTL, m_TTL);
	Entry.Write(API_V_DNS_STATUS, (int)m_Status);
	Entry.Write(API_V_DNS_QUERY_COUNT, m_QueryCounter);
	Entry.Write(API_V_CREATE_TIME, m_CreateTimeStamp);
	return Entry.Finish();
}