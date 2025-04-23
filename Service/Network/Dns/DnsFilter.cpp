#include "pch.h"
#include "DnsFilter.h"
#include "../../ServiceCore.h"
#include "../../../Library/API/PrivacyAPI.h"
#include "../../../Library/Common/Strings.h"
#include "../../../Library/Common/FileIO.h"
#include "../../../Library/Helpers/WebUtils.h"
#include "../../../Library/Helpers/MiscHelpers.h"


#define API_DNS_FILTER_FILE_NAME L"DnsFilter.dat"
#define API_DNS_FILTER_FILE_VERSION 1

static FW::StringW ReversePath(FW::AbstractMemPool* pMem, const char* pPath)
{
	FW::Array<FW::StringW> Path(pMem);
	if (pPath) {
		// reverse path *.example.com -> com.example.* for best matching performance
		std::vector<std::string> PathSegments = SplitStr(pPath, ".", true);
		for (int i = (int)PathSegments.size() - 1; i >= 0; i--) {
			FW::StringW Temp(pMem);
			Temp.Assign(PathSegments[i].c_str());
			Path += Temp;
		}
	}
	return FW::StringW::Join(Path, L".");
}

/////////////////////////////////////////////////////////////////////////////
// CDnsRule
//

StVariant CDnsRule::ToVariant(const SVarWriteOpt& Opts) const
{
	StVariantWriter Rule(Allocator());
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Rule.BeginIndex();
		WriteIVariant(Rule, Opts);
	} else {  
		Rule.BeginMap();
		WriteMVariant(Rule, Opts);
	}
	return Rule.Finish();
}

STATUS CDnsRule::FromVariant(const class StVariant& Rule)
{
	if (Rule.GetType() == VAR_TYPE_MAP)         StVariantReader(Rule).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
	else if (Rule.GetType() == VAR_TYPE_INDEX)  StVariantReader(Rule).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;

	FW::StringW Path = ReversePath(Allocator(), m_HostName.c_str());
	SetPath(Path);
	return STATUS_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////
// CDnsFilter
//

CDnsFilter::CDnsFilter()
	: m_FilterTree(&m_MemPool), m_EntryMap(&m_MemPool)
{
	m_FilterTree.SetSeparator(L'.');
	m_FilterTree.SetSimplePattern(true);
}

CDnsFilter::~CDnsFilter()
{
}

STATUS CDnsFilter::Init()
{
	Load();

	return OK;
}

STATUS CDnsFilter::Load()
{
	CBuffer Buffer;
	if (!ReadFile(theCore->GetDataFolder() + L"\\" API_DNS_FILTER_FILE_NAME, Buffer)) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_DNS_FILTER_FILE_NAME L" not found");
		return ERR(STATUS_NOT_FOUND);
	}

	StVariant Data;
	//try {
	auto ret = Data.FromPacket(&Buffer, true);
	//} catch (const CException&) {
	//	return ERR(STATUS_UNSUCCESSFUL);
	//}
	if (ret != StVariant::eErrNone) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_DNS_FILTER_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	if (Data[API_S_VERSION].To<uint32>() != API_DNS_FILTER_FILE_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_DNS_FILTER_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	LoadEntries(Data[API_S_DNS_RULES]);

	return OK;
}

STATUS CDnsFilter::Store()
{
	SVarWriteOpt Opts;
	Opts.Format = SVarWriteOpt::eIndex;
	Opts.Flags = SVarWriteOpt::eSaveToFile;

	StVariant Data;
	Data[API_S_VERSION] = API_DNS_FILTER_FILE_VERSION;
	Data[API_S_DNS_RULES] = SaveEntries(Opts);

	CBuffer Buffer;
	Data.ToPacket(&Buffer);
	WriteFile(theCore->GetDataFolder() + L"\\" API_DNS_FILTER_FILE_NAME, Buffer);

	return OK;
}

bool CDnsFilter::HandlePacket(DNS::Packet& Packet, const SAddress& Address)
{
	// Filter queries
	if (Packet.QR == 0) // query
	{
//#ifdef _DEBUG
//		 DbgPrint("DNS QUERY FROM %s\n", Address.ntop().c_str());
//		 DbgPrint("%s\n", DNS::output_packet(Packet, "  ").c_str());
//#endif

		 CDnsRule::EAction Action = CDnsRule::eNone;
		 for (auto const& query : Packet.query)
		 {
			 std::string HostName = DNS::build_name(query.q_name, 0);
			 if (query.q_class != DNS::Class::INet)
				 continue;

			 FW::StringW Path = ReversePath(&m_MemPool, HostName.c_str());
			 if (!Path.IsEmpty())
			 {
				 auto Entry = m_FilterTree.GetBestEntry(Path, CPathTree::eMayHaveSeparator);
				 if (Entry) {
					 Action = Entry.Cast<CDnsRule>()->GetAction(); 
					 Entry.Cast<CDnsRule>()->IncHitCount();
					 break;
				 }
			 }

			 if (Action == CDnsRule::eNone)
			 {
				 SKey key(HostName, query.q_type);
				 if (m_BlockList.find(key) != m_BlockList.end()) {
					 Action = CDnsRule::eBlock;
					 break;
				 }
			 }
		 }

		 if (Action == CDnsRule::eBlock)
		 {
//#ifdef _DEBUG
//			 DbgPrint("BLOCKED\n");
//#endif

			 Packet.QR = true;
			 Packet.OPCODE = 0;
			 Packet.AA = false;
			 Packet.TC = false;
			 Packet.RA = false;
			 Packet.Z = false;
			 Packet.RCODE = 3;

			 for (auto const& query : Packet.query)
			 {
				 std::string HostName = DNS::build_name(query.q_name, 0);
				 if (query.q_class != DNS::Class::INet)
					 continue;

				 SDnsFilterEvent Event;
				 Event.HostName = s2w(HostName);
				 Event.Action	= CDnsRule::eBlock;
				 Event.Type		= (uint16)query.q_type;
				 EmitEvent(&Event);
			 }


			 SendResponse(Packet, Address);
			 return true;
		 }

	}
	return CDnsServer::HandlePacket(Packet, Address);
}

static const CAddress LocalHost("127.0.0.1");
static const CAddress LocalHostV6("::1");

std::wstring decodeDnsName(const std::vector<uint8_t>& field) 
{
	std::wstring result;
	size_t pos = 0;

#ifdef _DEBUG
	char* test = (char*)field.data();
#endif

	while (pos < field.size()) {
		uint8_t length = field[pos];
		if (length == 0) {
			break;  // End of domain name
		}

		if (pos + length >= field.size()) {
			break;
		}

		if (!result.empty()) {
			result += L'.';  // Separate labels with dots
		}

		for (size_t i = 1; i <= length; ++i) {
			result += static_cast<wchar_t>(field[pos + i]);
		}

		pos += length + 1;
	}

	return result;
}

void CDnsFilter::SendResponse(DNS::Packet& Response, const SAddress& Address)
{
//#ifdef _DEBUG
//	DbgPrint("DNS RESPONSE TO %s\n", Address.ntop().c_str());
//	DbgPrint("%s\n", DNS::output_packet(Response, "  ").c_str());
//#endif

	for (auto const& answer : Response.answer)
	{
		// Build the queried hostname from the answer's name.
		std::string HostName = DNS::build_name(answer.r_name, 0);
		if (answer.r_class != DNS::Class::INet)
			continue;

		SDnsFilterEvent Event;
		Event.HostName = s2w(HostName);
		Event.Action   = CDnsRule::eAllow;
		Event.Type     = (uint16)answer.r_type;
		Event.TTL      = answer.r_ttl;

		switch (answer.r_type)
		{
		case DNS::Type::A:
			if (answer.r_data.size() >= 4)
			{
				// IPv4 address (4 bytes)
				Event.Address = CAddress(_ntohl(*((uint32*)answer.r_data.data())));
				if (Event.Address == LocalHost)
					continue;
			}
		break;

		case DNS::Type::AAAA:
			if (answer.r_data.size() >= 16)
			{
				// IPv6 address (16 bytes)
				Event.Address = CAddress(answer.r_data.data());
				if (Event.Address == LocalHostV6)
					continue;
			}
		break;

		case DNS::Type::CNAME:
			// CNAME: contains a domain name.
			Event.ResolvedString = decodeDnsName(answer.r_data);
		break;

		case DNS::Type::NA:
			// NA (often used as NS): contains a domain name for the nameserver.
			Event.ResolvedString = decodeDnsName(answer.r_data);
		break;

		case DNS::Type::PTR:
			// PTR: contains a domain name.
			Event.ResolvedString = decodeDnsName(answer.r_data);
		break;

		case DNS::Type::MX:
			// MX: contains 2-byte preference followed by a domain name.
			if (answer.r_data.size() >= 3)
			{
				uint16_t preference = (answer.r_data[0] << 8) | answer.r_data[1];
				std::vector<uint8_t> mxName(answer.r_data.begin() + 2, answer.r_data.end());
				std::wstring exchange = decodeDnsName(mxName);
				Event.ResolvedString = L"Pref: " + std::to_wstring(preference) + L", Exchange: " + exchange;
			}
		break;

		case DNS::Type::TXT:
			// TXT: contains one or more text strings.
			{
			std::wstring txt;
			size_t pos = 0;
			while (pos < answer.r_data.size())
			{
				uint8_t len = answer.r_data[pos];
				pos++;
				if (pos + len > answer.r_data.size())
					break;
				std::wstring part;
				for (size_t i = 0; i < len; ++i)
					part.push_back(static_cast<wchar_t>(answer.r_data[pos + i]));
				if (!txt.empty())
					txt += L" ";
				txt += part;
				pos += len;
			}
			Event.ResolvedString = txt;
		}
		break;

		case DNS::Type::SOA:
			// SOA: contains two domain names (MNAME and RNAME) followed by 5 32-bit integers.
		{
			// We'll decode the MNAME and RNAME.
			// The r_data is uncompressed so we can use decodeDnsName.
			// Note: decodeDnsName only decodes until the first zero byte.
			std::wstring mname = decodeDnsName(answer.r_data);
			// To decode RNAME, we need to skip over the first name.
			size_t pos = 0;
			while (pos < answer.r_data.size() && answer.r_data[pos] != 0)
			{
				pos += answer.r_data[pos] + 1;
			}
			if (pos < answer.r_data.size())
				pos++; // skip the terminating zero
			std::vector<uint8_t> remaining(answer.r_data.begin() + pos, answer.r_data.end());
			std::wstring rname = decodeDnsName(remaining);
			Event.ResolvedString = L"SOA MNAME: " + mname + L", RNAME: " + rname;
			// Optionally, you could parse and append the subsequent numeric fields.
		}
		break;

		case DNS::Type::WKS:
			// WKS: contains an IPv4 address (4 bytes), a protocol (1 byte), and a service bitmap.
			if (answer.r_data.size() >= 5)
			{
				CAddress addr = CAddress(_ntohl(*((uint32_t*)answer.r_data.data())));
				Event.Address = addr;
				uint8_t protocol = answer.r_data[4];
				std::wstring bitmap;
				for (size_t i = 5; i < answer.r_data.size(); ++i)
				{
					wchar_t buf[3] = {0};
					swprintf(buf, 3, L"%02X", answer.r_data[i]);
					bitmap += buf;
				}
				Event.ResolvedString = L"Protocol: " + std::to_wstring(protocol) + L", Bitmap: " + bitmap;
			}
		break;

		case DNS::Type::SRV:
			// SRV: contains 2 bytes priority, 2 bytes weight, 2 bytes port, then a domain name.
			if (answer.r_data.size() >= 7)
			{
				uint16_t priority = (answer.r_data[0] << 8) | answer.r_data[1];
				uint16_t weight   = (answer.r_data[2] << 8) | answer.r_data[3];
				uint16_t port     = (answer.r_data[4] << 8) | answer.r_data[5];
				std::vector<uint8_t> srvName(answer.r_data.begin() + 6, answer.r_data.end());
				std::wstring target = decodeDnsName(srvName);
				Event.ResolvedString = L"Priority: " + std::to_wstring(priority) +
					L", Weight: " + std::to_wstring(weight) +
					L", Port: " + std::to_wstring(port) +
					L", Target: " + target;
			}
		break;

			// Optionally, handle ANY and unknown types by dumping raw data in hex.
		case DNS::Type::ANY:
		default:
		{
			std::string hex = DNS::output_data(answer.r_data);
			Event.ResolvedString = s2w(hex);
		}
		break;
		}

		EmitEvent(&Event);
	}

	// Filter responses - not used for now
	CDnsServer::SendResponse(Response, Address);
}

STATUS CDnsFilter::LoadEntries(const StVariant& Entries)
{
	std::unique_lock lock(m_Mutex);

	for (uint32 i = 0; i < Entries.Count(); i++)
	{
		StVariant Entry = Entries[i];

		auto pEntry = m_MemPool.New<CDnsRule>();
		
		STATUS Status = pEntry->FromVariant(Entry);
		if (Status.IsError())
			; //  todo log error
		else
			AddEntry_unlocked(pEntry);
	}

	return OK;
}

bool CDnsFilter::AddEntry_unlocked(const FW::SharedPtr<CDnsRule>& pEntry)
{
	CFlexGuid Guid = pEntry->GetGuid();
	if (Guid.IsNull()) {
		if(!pEntry->IsTemporary())
			return false;
		Guid.FromWString(MkGuid());
	}
	if(m_EntryMap.Insert(Guid, pEntry, FW::EInsertMode::eNoReplace) != FW::EInsertResult::eOK)
		return false;
	if(pEntry->IsEnabled())
		return m_FilterTree.AddEntry(pEntry);
	return true;
}

StVariant CDnsFilter::SaveEntries(const SVarWriteOpt& Opts)
{
	std::unique_lock lock(m_Mutex);

	StVariant Entries(&m_MemPool);

	for (auto& pEntry : m_EntryMap) {

		if (pEntry->GetGuid().IsNull())
			continue;
		if(!(Opts.Flags & SVarWriteOpt::eSaveAll) && pEntry->IsTemporary())
			continue;

		Entries.Append(pEntry->ToVariant(Opts));
	}
	
	return Entries;
}

RESULT(StVariant) CDnsFilter::SetEntry(const StVariant& Entry)
{
	std::unique_lock lock(m_Mutex);

	CFlexGuid Guid(Entry[API_V_GUID].AsStr());

	FW::SharedPtr<CDnsRule> pEntry;
	if (Guid.IsNull()) // new entry
		Guid.FromWString(MkGuid());
	else
		pEntry = m_EntryMap.FindValue(Guid);

	bool bAdded = false;
	if (!pEntry) // not found or new entry
	{
		pEntry = m_MemPool.New<CDnsRule>();
		pEntry->SetGuid(Guid);
		if(m_EntryMap.Insert(Guid, pEntry, FW::EInsertMode::eNoReplace) != FW::EInsertResult::eOK)
			return ERR(STATUS_OBJECT_NAME_COLLISION);
		bAdded = true;
	}
	else // clear old entry
		m_FilterTree.RemoveEntry(pEntry);

	// update entry
	STATUS Status = pEntry->FromVariant(Entry);
	if (Status.IsError())
		return Status;

	// add new entry if enabled
	if (pEntry->IsEnabled()) {
		if (m_FilterTree.AddEntry(pEntry) != 0)
			return ERR(STATUS_INTERNAL_ERROR);
	}

	EmitChangeEvent(Guid, bAdded ? EConfigEvent::eAdded : EConfigEvent::eModified);
	RETURN(Guid.ToVariant(false));
}

RESULT(StVariant) CDnsFilter::GetEntry(const std::wstring& EntryId, const SVarWriteOpt& Opts)
{
	std::unique_lock lock(m_Mutex);

	CFlexGuid Guid(EntryId);
	FW::SharedPtr<CDnsRule> pEntry = m_EntryMap.FindValue(Guid);
	if (!pEntry)
		return ERR(STATUS_NOT_FOUND);
	RETURN(pEntry->ToVariant(Opts));
}

STATUS CDnsFilter::DelEntry(const std::wstring& EntryId)
{
	std::unique_lock lock(m_Mutex);

	CFlexGuid Guid(EntryId);
	auto pEntry = m_EntryMap.Take(Guid);
	if (!pEntry)
		return ERR(STATUS_NOT_FOUND);
	m_FilterTree.RemoveEntry(pEntry);
	EmitChangeEvent(Guid, EConfigEvent::eRemoved);
	return OK;
}

void CDnsFilter::EmitChangeEvent(const CFlexGuid& Guid, enum class EConfigEvent Event)
{
	StVariant vEvent;
	vEvent[API_V_GUID] = Guid.ToVariant(false);
	//vEvent[API_V_NAME] = ;
	vEvent[API_V_EVENT_TYPE] = (uint32)Event;

	theCore->BroadcastMessage(SVC_API_EVENT_DNS_RULE_CHANGED, vEvent);
}

void CDnsFilter::UpdateBlockLists()
{
	std::unique_lock lock(m_Mutex);

	if (!CreateDirectoryW((theCore->GetDataFolder() + L"\\dns").c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_DNS_INIT_FAILED, L"Failed to create directory: %s", (theCore->GetDataFolder() + L"\\dns").c_str());
		return;
	}

	//
	// Schedule BlockList Download
	//

	std::map<std::wstring, SBLockList> OldList = m_BlockLists;

	auto BlockLists = SplitStr(theCore->Config()->GetValue("Service", "DnsBlockLists"), L";");
	for (auto& sBlockListUrl : BlockLists)
	{
		std::wstring sBlockListFile = GetFileNameFromUrl(sBlockListUrl);
		auto I = OldList.find(sBlockListFile);
		if (I != OldList.end())
			OldList.erase(I);
		else
			m_BlockLists.emplace(sBlockListFile, SBLockList{ sBlockListUrl });

		DownloadBlockList(sBlockListUrl);
	}

	for (auto I = OldList.begin(); I != OldList.end(); I++)
		m_BlockLists.erase(I->first);

	//
	// Load all BlockLists, this will load old versions of the already loaded lists
	//

	LoadBlockLists_unlocked();
}

void CDnsFilter::LoadBlockLists_unlocked()
{
	//
	// empty the blocklist
	// 
	// and clear all entries that spiled over from the simple blocklist to the the filtertree
	// thay will be readded by LoadBlockLists_unlocked
	//

	m_BlockList.clear();

	for (auto I = m_EntryMap.begin(); I != m_EntryMap.end(); ++I)
	{
		if ((*I)->GetGuid().IsNull())
			m_FilterTree.RemoveEntry(I.Value());
	}

	//
	// load all current blocklists
	//

	for (auto I = m_BlockLists.begin(); I != m_BlockLists.end(); I++)
	{
		std::wstring sBlockListFile = I->first;
		if (!sBlockListFile.empty() && FileExists(sBlockListFile)) 
			I->second.Count = LoadFilters_unlocked(sBlockListFile, CDnsRule::eBlock, &m_BlockList);
	}
}

StVariant CDnsFilter::GetBlockListInfo() const
{
	std::unique_lock lock(m_Mutex);

	StVariantWriter BlockListInfo;
	BlockListInfo.BeginList();

	for (auto& I : m_BlockLists)
	{
		StVariant BlockList;
		BlockList[API_V_URL] = I.second.Url;
		BlockList[API_V_FILE_PATH] = I.first;
		BlockList[API_V_COUNT] = I.second.Count;
		BlockListInfo.WriteVariant(BlockList);
	}

	return BlockListInfo.Finish();
}

std::wstring CDnsFilter::GetFileNameFromUrl(const std::wstring& sUrl)
{
	std::wstring Host, Path, Query;
	if (!ParseUrl(sUrl.c_str(), NULL, &Host, NULL, &Path, &Query) || Host.empty()) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_DNS_INIT_FAILED, L"Blocklist url invalid: %s", sUrl.c_str());
		return L"";
	}

	std::wstring Name;
	auto PathName = Split2(Path, L"/", true);
	if (!PathName.second.empty()) Name = PathName.second;
	else if (!PathName.first.empty()) Name = PathName.first;
	else Name = L"hosts";

	return theCore->GetDataFolder() + L"\\dns\\" + StrReplaceAll(Host, L".", L"-") + L"~" + Name;
}


DWORD CALLBACK CDnsFilter__DlThreadProc(LPVOID lpThreadParameter)
{
#ifdef _DEBUG
	SetThreadDescription(GetCurrentThread(), L"CServiceCore__ThreadProc");
#endif

	CDnsFilter* This = (CDnsFilter*)lpThreadParameter;

	std::unique_lock lock(This->m_DlMutex);
	while(!This->m_DlList.empty())
	{
		auto it = This->m_DlList.begin();
		std::wstring sBlockListUrl = it->first;
		std::wstring sBlockListFile = it->second;
		lock.unlock();

		bool bDownload = true;
		time_t TimeStamp = GetFileModificationUnixTime(sBlockListFile);
		time_t WebTimeStamp = -1;
		if(!WebGetLastModifiedDate(sBlockListUrl.c_str(), WebTimeStamp))
			bDownload = false;
		else if (TimeStamp != -1 && WebTimeStamp <= TimeStamp)
			bDownload = false;

		if (bDownload)
		{
			char* pData = NULL;
			ULONG uDataLen = 0;
			if (WebDownload(sBlockListUrl.c_str(), &pData, &uDataLen))
			{
				CBuffer Buffer(pData, uDataLen); // Buffer adopts pData and will free it
				if (WriteFile(sBlockListFile, Buffer))
				{
					SetFileModificationUnixTime(sBlockListFile, WebTimeStamp);

					//
					// once we have downloaded a list reload all lists
					//

					std::unique_lock lock(This->m_Mutex);

					This->LoadBlockLists_unlocked();
				}
			}
		}

		lock.lock();
		This->m_DlList.erase(sBlockListUrl);
	}

	CloseHandle(This->m_hDlThread);
	This->m_hDlThread = NULL;

	return 0;
}

bool CDnsFilter::DownloadBlockList(const std::wstring& sBlockListUrl)
{
	std::unique_lock lock(m_DlMutex);

	if (!m_hDlThread)
		m_hDlThread = CreateThread(NULL, 0, CDnsFilter__DlThreadProc, (void*)this, 0, NULL);
	
	if (m_DlList.find(sBlockListUrl) != m_DlList.end())
		return true;

	std::wstring sBlockListFile = GetFileNameFromUrl(sBlockListUrl);
	if (sBlockListFile.empty())
		return false;
	m_DlList.emplace(sBlockListUrl, sBlockListFile);
	return true;
}

int CDnsFilter::LoadFilters_unlocked(const std::wstring& sHostsFile, CDnsRule::EAction Action, std::multimap<SKey, DNS::Binary>* pEntryMap)
{
	int Count = 0;

	std::ifstream in(sHostsFile);

	std::string sLine;
	while(std::getline(in, sLine)) 
	{
		std::istringstream is(sLine);

		DNS::Binary IpAddress;
		DNS::Type EntryType = DNS::Type::None;

		std::string sAddress;
		is >> sAddress;

		if (sAddress.empty() || is.bad())
			continue;

		if (sAddress[0] == '#')
			continue;

		if (sAddress.find(':') != std::string::npos) // ipv6
		{
			struct in6_addr a6;
			if (inet_pton(AF_INET6, sAddress.c_str(), &a6) > 0) {
				EntryType = DNS::Type::AAAA;
				IpAddress = DNS::Binary(reinterpret_cast<DNS::Binary::const_pointer>(&a6), reinterpret_cast<DNS::Binary::const_pointer>(&a6 + 1));
			}
		}
		else  // assume ipv4
		{
			struct in_addr a;
			if (inet_pton(AF_INET, sAddress.c_str(), &a) > 0) {
				EntryType = DNS::Type::A;
				IpAddress = DNS::Binary(reinterpret_cast<DNS::Binary::const_pointer>(&a), reinterpret_cast<DNS::Binary::const_pointer>(&a + 1));
			}
		}

		std::string HostName;
		if (EntryType == DNS::Type::None) // no IP found so its a domain list only
			HostName = sAddress;
		else // else read host after IP
			is >> HostName;

		while(!is.bad() && !HostName.empty()) 
		{
			if (HostName[0] == '#')
				break;

			if (pEntryMap && EntryType != DNS::Type::None && HostName.find_first_of("*?<>|") == std::wstring::npos) // when the host contains a wildcard we do not add it to the map
				pEntryMap->emplace(SKey(HostName, EntryType), IpAddress);
			else // add to filter list
			{
				FW::StringW Path = ReversePath(&m_MemPool, HostName.c_str());
				auto pEntry = m_MemPool.New<CDnsRule>(Path, Action, pEntryMap ? true : false); // use m_FilterTree for complex blocklist entries, but mask as temporary
				AddEntry_unlocked(pEntry);
			}
			Count++;

			HostName.clear();
			is >> HostName; // load aliasses
		}
	}

	return Count;
}

void CDnsFilter::RegisterEventHandler(const std::function<void(const SDnsFilterEvent* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m_Mutex);
	m_EventHandlers.push_back(Handler); 
}

void CDnsFilter::EmitEvent(const SDnsFilterEvent* pEvent)
{
	std::unique_lock<std::mutex> Lock(m_Mutex);
	for (auto Handler : m_EventHandlers)
		Handler(pEvent);
}