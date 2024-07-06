#include "pch.h"
#include <WinDNS.h>
#include "DnsInspector.h"
#include "ServiceCore.h"
#include "../../Etw/EtwEventMonitor.h"
#include "..\Library\Common\Strings.h"
#include "..\Library\Common\DbgHelp.h"
#include "..\..\Processes\ProcessList.h"

#pragma comment(lib, "dnsapi.lib")


typedef struct _DNS_CACHE_ENTRY
{
	struct _DNS_CACHE_ENTRY* Next;  // Pointer to next entry
	PCWSTR Name;                    // DNS Record Name
	USHORT Type;                    // DNS Record Type
	USHORT DataLength;              // Not referenced
	ULONG Flags;                    // DNS Record Flags
} DNS_CACHE_ENTRY, *PDNS_CACHE_ENTRY;

typedef DNS_STATUS (WINAPI* _DnsGetCacheDataTable)(
	_Inout_ PDNS_CACHE_ENTRY* DnsCacheEntry
	);

typedef BOOL (WINAPI* _DnsFlushResolverCache)(
	VOID
	);

typedef BOOL (WINAPI* _DnsFlushResolverCacheEntry)(
	_In_ PCWSTR Name
	);

struct SDnsInspector
{
	HINSTANCE DnsApiHandle = NULL;
	_DnsGetCacheDataTable DnsGetCacheDataTable_I = NULL;
	_DnsFlushResolverCache DnsFlushResolverCache_I = NULL;
	_DnsFlushResolverCacheEntry DnsFlushResolverCacheEntry_I = NULL;
};


CDnsInspector::CDnsInspector()
{
	m = new SDnsInspector();
}

CDnsInspector::~CDnsInspector()
{
	m_bRunning = false;
	if (m_Resolver) {
		m_Resolver->join();
		m_Resolver.reset();
	}
	if(m->DnsApiHandle)
		FreeLibrary(m->DnsApiHandle);
	delete m;
}

STATUS CDnsInspector::Init()
{
	theCore->EtwEventMonitor()->RegisterDnsHandler(&CDnsInspector::OnEtwDnsEvent, this);

	if (m->DnsApiHandle = LoadLibrary(L"dnsapi.dll"))
	{
		*(void**)&m->DnsGetCacheDataTable_I = GetProcAddress(m->DnsApiHandle, "DnsGetCacheDataTable");
		*(void**)&m->DnsFlushResolverCache_I = GetProcAddress(m->DnsApiHandle, "DnsFlushResolverCache");
		*(void**)&m->DnsFlushResolverCacheEntry_I = GetProcAddress(m->DnsApiHandle, "DnsFlushResolverCacheEntry_W");
	}

	return OK;
}

void CDnsInspector::OnEtwDnsEvent(const struct SEtwDnsEvent* pEvent)
{
	if (pEvent->Results.empty())
		return;

#ifdef _DEBUG
	//DbgPrint(L"ETW Dns Event for %s\n", pEvent->HostName.c_str());
#endif

	CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pEvent->ProcessId, true); // Note: this will add the process and load some basic data if it does not already exist
	if(pProcess)
		pProcess->DnsLog()->OnEtwDnsEvent(pEvent);
}

std::multimap<std::wstring, CDnsCacheEntryPtr>::iterator FindDnsEntry(std::multimap<std::wstring, CDnsCacheEntryPtr> &Entries, const std::wstring& HostName, const CAddress& Address)
{
	for (std::multimap<std::wstring, CDnsCacheEntryPtr>::iterator I = Entries.find(HostName); I != Entries.end() && I->first == HostName; I++)
	{
		if (I->second->GetAddress() == Address)
			return I;
	}

	// no matching entry
	return Entries.end();
}

std::multimap<std::wstring, CDnsCacheEntryPtr>::iterator FindDnsEntry(std::multimap<std::wstring, CDnsCacheEntryPtr> &Entries, const std::wstring& HostName, int Type, const std::wstring& ResolvedString)
{
	for (std::multimap<std::wstring, CDnsCacheEntryPtr>::iterator I = Entries.find(HostName); I != Entries.end() && I->first == HostName; I++)
	{
		if (I->second->GetType() == Type && I->second->GetResolvedString() == ResolvedString)
			return I;
	}

	// no matching entry
	return Entries.end();
}

CAddress RevDnsHost2Address(const std::wstring& HostName)
{
	// 10.0.0.1 -> 1.0.0.10.in-addr.arpa
	// 2001:db8::1 -> 1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa

	std::vector<std::wstring> HostStr = SplitStr(HostName, L".");
	if (HostStr.size() >= 32 + 2 && HostStr[32] == L"ip6" && HostStr[33] == L"arpa")
	{
		std::vector<std::wstring> AddrStr;
		for (int i = 32 - 1; i > 0; i -= 4)
			AddrStr.push_back(HostStr[i] + HostStr[i - 1] + HostStr[i - 2] + HostStr[i - 3]);
		return CAddress(JoinStr(AddrStr, L":"));
	}
	else if (HostStr.size() >= 4 + 2 && HostStr[4] == L"in-addr" && HostStr[5] == L"arpa")
	{
		return CAddress(JoinStr(std::vector<std::wstring>(HostStr.rbegin()+2, HostStr.rend()), L"."));
	}
	return CAddress();
}

//#define DNS_SCRAPE

#ifdef DNS_SCRAPE
BOOLEAN RecordSetContains(_In_ PDNS_RECORD Head, _In_ PDNS_RECORD Target)
{
	while (Head)
	{
		if (DnsRecordCompare(Head, Target))
			return TRUE;

		Head = Head->pNext;
	}

	return FALSE;
}

PDNS_RECORD TraverseDnsCacheTable(SDnsInspector* m)
{
	USHORT typeList[] = { DNS_TYPE_A, DNS_TYPE_AAAA, DNS_TYPE_MX, DNS_TYPE_SRV, DNS_TYPE_PTR }; // Only interested in these queries, to boost traversing performance

	PDNS_CACHE_ENTRY dnsCacheDataTable = NULL;
	if (!m->DnsGetCacheDataTable_I(&dnsCacheDataTable))
		goto CleanupExit;

	PDNS_RECORD root = NULL;
	for (PDNS_CACHE_ENTRY tablePtr = dnsCacheDataTable; tablePtr; tablePtr = tablePtr->Next)
	{
		for (USHORT i = 0; i < ARRSIZE(typeList); i++)
		{
			PDNS_RECORD dnsQueryResultPtr;
			if (DnsQuery(tablePtr->Name, typeList[i], DNS_QUERY_NO_WIRE_QUERY | 32768 /*Undocumented flags*/, NULL, &dnsQueryResultPtr, NULL) == ERROR_SUCCESS)
				//if (DnsQuery(tablePtr->Name, tablePtr->Type, DNS_QUERY_NO_WIRE_QUERY | 32768 /*Undocumented flags*/, NULL, &dnsQueryResultPtr, NULL) == ERROR_SUCCESS)
			{
				for (PDNS_RECORD dnsRecordPtr = dnsQueryResultPtr; dnsRecordPtr; dnsRecordPtr = dnsRecordPtr->pNext)
				{
					if (!RecordSetContains(root, dnsRecordPtr))
					{
						PDNS_RECORD temp = root;
						root = DnsRecordCopy(dnsRecordPtr);
						root->pNext = temp;
					}
				}

				DnsRecordListFree(dnsQueryResultPtr, DnsFreeRecordList);
			}
		}
	}

CleanupExit:

	if (dnsCacheDataTable)
		DnsRecordListFree(dnsCacheDataTable, DnsFreeRecordList);

	return root;
}
#endif

static const CAddress LocalHost("127.0.0.1");
static const CAddress LocalHostV6("::1");

void CDnsInspector::Update()
{
	static int x = 0;
	if (x)
		return;

#ifdef DNS_SCRAPE
	PDNS_RECORD dnsRecordRootPtr = TraverseDnsCacheTable(m); 
#else
	PDNS_CACHE_ENTRY dnsCacheDataTable = NULL;
	if (!m->DnsGetCacheDataTable_I(&dnsCacheDataTable))
		return;
#endif

//#define DUMP_DNS

	// Copy the emtry std::map Map
	std::multimap<std::wstring, CDnsCacheEntryPtr> OldEntries = GetDnsCache();

#ifdef DNS_SCRAPE
	for (PDNS_RECORD dnsRecordPtr = dnsRecordRootPtr; dnsRecordPtr != NULL; dnsRecordPtr = dnsRecordPtr->pNext)
	{
#else
	for (PDNS_CACHE_ENTRY tablePtr = dnsCacheDataTable; tablePtr != NULL; tablePtr = tablePtr->Next)
	{
		//std::wstring HostName_ = std::wstring::fromWCharArray(tablePtr->Name);
		//uint16 Type_ = tablePtr->Type;

#ifdef DUMP_DNS
		DbgPrint(L"Dns Table Entry: %s Type: %s\n", tablePtr->Name, CDnsCacheEntry::GetTypeString(tablePtr->Type).c_str());
#endif

		PDNS_RECORD dnsQueryResultPtr;

		DNS_STATUS ret = DnsQuery(tablePtr->Name, tablePtr->Type, DNS_QUERY_NO_WIRE_QUERY | 32768 /*Undocumented flags*/, NULL, &dnsQueryResultPtr, NULL);
		if (ret != ERROR_SUCCESS)
			continue;

		for (PDNS_RECORD dnsRecordPtr = dnsQueryResultPtr; dnsRecordPtr; dnsRecordPtr = dnsRecordPtr->pNext)
		{

#ifdef DUMP_DNS
			DbgPrint(L"Dns Query Result: %s Type: %s\n", dnsRecordPtr->pName, CDnsCacheEntry::GetTypeString(dnsRecordPtr->wType).c_str());
#endif
#endif	
			std::wstring HostName = dnsRecordPtr->pName;
			uint16 Type = dnsRecordPtr->wType;
			CAddress Address;
			std::wstring ResolvedString;

			std::multimap<std::wstring, CDnsCacheEntryPtr>::iterator I;
			if (Type == DNS_TYPE_A || Type == DNS_TYPE_AAAA)
			{
				switch (Type)
				{
				case DNS_TYPE_A:	Address = CAddress(_ntohl(dnsRecordPtr->Data.A.IpAddress)); break;
				case DNS_TYPE_AAAA:	Address = CAddress(dnsRecordPtr->Data.AAAA.Ip6Address.IP6Byte); break;
				}
				ResolvedString = Address.ToWString();

				if (Address == LocalHost || Address == LocalHostV6)
					continue;

				I = FindDnsEntry(OldEntries, HostName, Address);

				// Note the table may contain duplicates, filter them out
				if (I == OldEntries.end())
				{
					std::shared_lock Lock(m_Mutex);
					if (FindDnsEntry(m_DnsCache, HostName, Address) != m_DnsCache.end())
						continue;
				}
			}
			else
			{
				switch (Type)
				{
				case DNS_TYPE_PTR:		ResolvedString = dnsRecordPtr->Data.PTR.pNameHost;	break;
					Address = RevDnsHost2Address(HostName);
					// we don't care for entries without a valid address
					if (Address.IsNull())
						continue;
					//case DNS_TYPE_DNAME:	ResolvedString = dnsRecordPtr->Data.DNAME.pNameHost; break; // entire zone
				case DNS_TYPE_CNAME:	ResolvedString = dnsRecordPtr->Data.CNAME.pNameHost; break; // one host
				case DNS_TYPE_SRV:		ResolvedString = std::wstring(dnsRecordPtr->Data.SRV.pNameTarget) + L":" + std::to_wstring((uint16)dnsRecordPtr->Data.SRV.wPort); break;
				case DNS_TYPE_MX:		ResolvedString = dnsRecordPtr->Data.MX.pNameExchange; break;
				default:
					continue;
				}

				I = FindDnsEntry(OldEntries, HostName, Type, ResolvedString);

				// Note the table may contain duplicates, filter them out
				if (I == OldEntries.end())
				{
					std::shared_lock Lock(m_Mutex);
					if (FindDnsEntry(m_DnsCache, HostName, Type, ResolvedString) != m_DnsCache.end())
						continue;
				}
			}

#ifdef DUMP_DNS
			DbgPrint(L"\tDns Query Result: %s Type: %s %s\n", HostName.c_str(), CDnsCacheEntry::GetTypeString(Type).c_str(), ResolvedString.c_str());
#endif

			CDnsCacheEntryPtr pEntry;
			if (I == OldEntries.end())
			{
				pEntry = CDnsCacheEntryPtr(new CDnsCacheEntry(HostName, Type, Address, ResolvedString));
				std::unique_lock Lock(m_Mutex);
				m_DnsCache.insert(std::make_pair(HostName, pEntry));
				if(!Address.IsNull())
					m_AddressCache.insert(std::make_pair(Address, pEntry));
				else
					m_RedirectionCache.insert(std::make_pair(ResolvedString, pEntry));
			}
			else
			{
				pEntry = I->second;
				OldEntries.erase(I);
			}

			pEntry->SetTTL(dnsRecordPtr->dwTtl * 1000); // we need that in ms

#ifndef DNS_SCRAPE
		}

		DnsRecordListFree(dnsQueryResultPtr, DnsFreeRecordList);
#endif
	}


	// keep DNS results cached for  long time, as we are using them to monitor with wha domains a programm was communicating
	uint64 CurTick = GetCurTick();
	uint64 TimeToSubtract = CurTick - m_LastCacheTraverse;
	m_LastCacheTraverse = CurTick;

	uint64 DnsPersistenceTime = theCore->Config()->GetUInt64("Service", "DnsPersistenceTime", 60*60*1000); // 60 minutes
	for (std::multimap<std::wstring, CDnsCacheEntryPtr>::iterator I = OldEntries.begin(); I != OldEntries.end(); )
	{
		if (I->second->GetDeadTime() < DnsPersistenceTime)
		{
			I->second->SubtractTTL(TimeToSubtract); // continue TTL countdown
			I = OldEntries.erase(I);
		}
		else
			I++;
	}

	std::unique_lock Lock(m_Mutex);
	for(std::multimap<std::wstring, CDnsCacheEntryPtr>::iterator I = OldEntries.begin(); I != OldEntries.end(); ++I)
	{
		CDnsCacheEntryPtr pEntry = I->second;
		if (pEntry->CanBeRemoved())
		{
			mmap_erase(m_DnsCache, I->first, pEntry);
			CAddress Address = pEntry->GetAddress();
			if (!Address.IsNull())
				mmap_erase(m_AddressCache, Address, pEntry);
			else
				mmap_erase(m_RedirectionCache, pEntry->GetResolvedString(), pEntry);
		}
		else if (!pEntry->IsMarkedForRemoval())
			pEntry->MarkForRemoval();
	}
	Lock.unlock();

#ifdef DNS_SCRAPE
	if (dnsRecordRootPtr)
		DnsRecordListFree(dnsRecordRootPtr, DnsFreeRecordList);
#else
	if (dnsCacheDataTable)
		DnsRecordListFree(dnsCacheDataTable, DnsFreeFlat);
#endif
}

bool CDnsInspector::ResolveHost(const CAddress& Address, const CHostNamePtr& pHostName)
{
	if (Address.IsNull()) // || Address == CAddress::AnyIPv4 || Address == CAddress::AnyIPv6)
		return false;

	// if its the local host return imminetly
	if (Address == LocalHost || Address == LocalHostV6)
		return false;

	// Check if we have a valid reverse DNS entry in the cache
	std::shared_lock Lock(m_Mutex);
	//int ValidReverseEntries = 0;
	std::vector<std::wstring> RevHostNames;
	for (std::multimap<CAddress, CDnsCacheEntryPtr>::iterator I = m_AddressCache.find(Address); I != m_AddressCache.end() && I->first == Address; ++I)
	{
		//qDebug() << I.key().toString() << I.value()->GetResolvedString();
		if (I->second->GetType() != DNS_TYPE_PTR)
			continue;

		if (I->second->GetTTL() == 0)
		{
			if (I->second->GetDeadTime() > min(10*1000 * (I->second->GetQueryCounter() - 1), 300*1000))
				continue;
		}

		//ValidReverseEntries++;
		RevHostNames.push_back(I->second->GetResolvedString());
	}
	Lock.unlock();

	// if we dont have a valid entry start a lookup job and finisch asynchroniously
	//if (ValidReverseEntries == 0)
	if(RevHostNames.size() == 0 && theCore->Config()->GetBool("Service", "UserReverseDns", false))
	{
		std::unique_lock Lock(m_JobMutex);

		// check if this address is knowwn to have no results to avoid repetitve pointeless dns queries
		auto F = m_NoResultBlackList.find(Address);
		if (F != m_NoResultBlackList.end()) {
			if (F->second > GetCurTick())
				return false;
			m_NoResultBlackList.erase(F);
		}

		// start resolver if needed
		if (!m_bRunning) {
			m_bRunning = true;
			m_Resolver = std::shared_ptr<std::thread>(new std::thread([this]() { ProcessJobList(); }));
		}

		// check if already scheduled
		for(auto I = m_JobList.find(Address); I != m_JobList.end() && I->first == Address; ++I) {
			if(I->second == pHostName)
				return false;
		}

		// insert job
		m_JobList.insert(std::make_pair(Address, pHostName));

		return false;
	}

	std::wstring HostName = GetHostNameSmart(Address, RevHostNames);
	if (HostName.empty()) return false;
	auto ptr = std::dynamic_pointer_cast<CResHostName>(pHostName);
	if (ptr) ptr->Resolve(HostName, EDnsSource::eCachedQuery, GetCurTimeStamp()); // todo fix me: use proepr tiemstamp from when the entry was caches
	return true;
}

std::wstring CDnsInspector::GetHostNamesSmart(const std::wstring& HostName, int Limit)
{
	// WARNING: this function is not thread safe

	std::multimap<uint64, std::wstring> Domins;
	if (Limit > 0)  // in case 1.a.com -> 2.a.com -> 3.a.com -> 1.a.com
	{
		// use maps to sort newest entries (biggest creation timestamp) first
		for (std::multimap<std::wstring, CDnsCacheEntryPtr>::iterator I = m_RedirectionCache.find(HostName); I != m_RedirectionCache.end() && I->first == HostName; ++I)
		{
			CDnsCacheEntryPtr pEntry = I->second;
			uint16 Type = pEntry->GetType();
			if (Type == DNS_TYPE_CNAME)
				Domins.insert(std::make_pair(-1 - pEntry->GetCreateTimeStamp(), GetHostNamesSmart(pEntry->GetHostName(), Limit - 1)));
		}
	}
	if (Domins.empty())
		return HostName;

	// (bla.1.com, blup.1.com) -> 1.com
	std::wstring HostNames;
	if (Domins.size() == 1)
		HostNames = Domins.begin()->second;
	else if (Domins.size() > 1)
	{
		HostNames = L"(";
		for (auto I : Domins)
			HostNames += I.second + L", ";
		HostNames = HostNames.substr(0, HostNames.length() - 2);
		HostNames += L")";
	}
	HostNames += L" -> " + HostName;
	return HostNames;
}

std::wstring CDnsInspector::GetHostNameSmart(const CAddress& Address, const std::vector<std::wstring>& RevHostNames)
{

	std::multimap<uint64, CDnsCacheEntryPtr> Entries;
	//if (theConf->GetBool("Options/MonitorDnsCache", false))
	{
		std::shared_lock Lock(m_Mutex);

		// use maps to sort newest entries (biggest creation timestamp) first
		for (std::multimap<CAddress, CDnsCacheEntryPtr>::iterator I = m_AddressCache.find(Address); I != m_AddressCache.end() && I->first == Address; ++I)
		{
			CDnsCacheEntryPtr pEntry = I->second;

			uint16 Type = pEntry->GetType();
			if (Type == DNS_TYPE_A || Type == DNS_TYPE_AAAA)
				Entries.insert(std::make_pair(-1 - pEntry->GetCreateTimeStamp(), pEntry));
		}
	}

	std::wstring HostName;
	if (!Entries.empty())
	{
		std::shared_lock Lock(m_Mutex);

		std::vector<std::wstring> Domins;
		for(auto I: Entries)
			Domins.push_back(GetHostNamesSmart(I.second->GetHostName()));

		if (Domins.size() == 1)
			HostName = Domins[0];
		else if (Domins.size() > 1)
			HostName = L"(" + JoinStr(Domins, L" | ") + L")";

		if (!RevHostNames.empty())
		{
			std::wstring RevHostName = JoinStr(RevHostNames, L", ");
			if(HostName != RevHostName)
				HostName.append(L" [" + RevHostName + L"]");
		}
	}
	else if(!RevHostNames.empty()) // no A or AAAA entries lets show only the reverse DNS result
		HostName = JoinStr(RevHostNames, L", ");

	return HostName;
}

void CDnsInspector::ProcessJobList()
{
	//int IdleCount = 0;
	while (m_bRunning)
	{
		std::unique_lock Lock(m_JobMutex);
		if (m_JobList.empty()) 
		{
			//if (IdleCount++ > 4 * 10) // if we were idle for 10 seconds end the thread
			//{
			//	m_bRunning = false;
			//	break;
			//}
			Lock.unlock();
			Sleep(250);
			continue;
		}
		//IdleCount = 0;
		CAddress Address = m_JobList.begin()->first;
		Lock.unlock();

		WCHAR AddressReverse[128];
		const BYTE* data = Address.Data();
		if (Address.Type() == CAddress::EAF::INET)
			swprintf(AddressReverse, ARRAYSIZE(AddressReverse), L"%d.%d.%d.%d.in-addr.arpa", data[3], data[2], data[1], data[0]);
		else if(Address.Type() == CAddress::EAF::INET6)
		{
			WCHAR* ptr = AddressReverse;
			WCHAR* end = AddressReverse + ARRAYSIZE(AddressReverse);
			for (int i = 16 - 1; i >= 0; i--) {
				swprintf(ptr, end - ptr, L"%x.%x.", data[i] & 0xF, (data[i] >> 4) & 0xF);
				ptr += 4;
			}
			swprintf(ptr, end-ptr, L"ip6.arpa");
		}

		std::wstring HostName;
		PDNS_RECORD addressResultsRoot = NULL;
		if (DnsQuery((std::wstring(AddressReverse) + L".").c_str(), DNS_TYPE_PTR, DNS_QUERY_NO_HOSTS_FILE /* | DNS_QUERY_BYPASS_CACHE*/, NULL, &addressResultsRoot, NULL) == ERROR_SUCCESS)
		{
			std::vector<std::wstring> RevHostNames;
			for (PDNS_RECORD addressResults = addressResultsRoot; addressResults != NULL; addressResults = addressResults->pNext)
			{
				if (addressResults->wType != DNS_TYPE_PTR)
					continue;

				std::wstring ResolvedString = addressResults->Data.PTR.pNameHost;
				CAddress Address = RevDnsHost2Address(AddressReverse);

				std::unique_lock Lock(m_Mutex);

				CDnsCacheEntryPtr pEntry = NULL;
				std::multimap<std::wstring, CDnsCacheEntryPtr>::iterator I = FindDnsEntry(m_DnsCache, AddressReverse, DNS_TYPE_PTR, ResolvedString);
				if (I == m_DnsCache.end())
				{
					pEntry = CDnsCacheEntryPtr(new CDnsCacheEntry(AddressReverse, DNS_TYPE_PTR, Address, ResolvedString));
					m_DnsCache.insert(std::make_pair(AddressReverse, pEntry));
					m_AddressCache.insert(std::make_pair(Address, pEntry));
				}
				else
					pEntry = I->second;

				pEntry->SetTTL(addressResults->dwTtl * 1000); // we need that in ms

				RevHostNames.push_back(ResolvedString);
			}

			if (addressResultsRoot)
				DnsRecordListFree(addressResultsRoot, DnsFreeRecordList);

			HostName = GetHostNameSmart(Address, RevHostNames);
		}

		Lock.lock();
		for(auto I = m_JobList.find(Address); I != m_JobList.end() && I->first == Address; I = m_JobList.erase(I)) {
			if (HostName.empty()) continue;
			auto ptr = std::dynamic_pointer_cast<CResHostName>(I->second);
			if (ptr) ptr->Resolve(HostName, EDnsSource::eReverseDns, GetCurTimeStamp());
		}

		if(HostName.empty())
			m_NoResultBlackList[Address] = GetCurTick() + 10*60*1000; // wait 10 minute to retry
		Lock.unlock();
	}
}

CVariant CDnsInspector::DumpDnsCache()
{
	std::shared_lock Lock(m_Mutex);

	CVariant DnsCache;
	DnsCache.BeginList();
	for (auto I = m_DnsCache.begin(); I != m_DnsCache.end(); ++I)
		DnsCache.WriteVariant(I->second->ToVariant());
	DnsCache.Finish();
	return DnsCache;
}