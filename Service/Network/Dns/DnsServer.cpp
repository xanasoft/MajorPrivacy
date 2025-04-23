#include "pch.h"
#include "DnsServer.h"

CDnsServer::CDnsServer()
{
	m_ForwardAddress.addrlen = 0;
}

CDnsServer::~CDnsServer()
{
}

bool CDnsServer::Start()
{
	return Open(53);
}

bool CDnsServer::SetForwardAddress(const std::wstring& sAddress)
{
	std::unique_lock lock(m_Mutex);
	if (sAddress.empty()) {
		m_ForwardAddress.addrlen = 0;
		return true;
	}
	m_sForwardAddress = sAddress;
	return ResolveAddress(sAddress, m_ForwardAddress);
}

bool CDnsServer::HandlePacket(DNS::Packet& Packet, const SAddress& Address)
{
	if (Packet.QR == 0) // query
	{
		if(IsForwarder())
			return ForwardQuery(Packet, Address);
		return ResolveLocally(Packet, Address);
	}
	return CDnsSocket::HandlePacket(Packet, Address);
}

bool CDnsServer::ForwardQuery(DNS::Packet& Query, const SAddress& Address)
{
	bool Ok = false;

	auto pQuery = std::make_shared<SForwardedQuery>();
	pQuery->TimeStamp = time(nullptr);
	pQuery->Address = Address;
	pQuery->QueryId = Query.ID; // save original id
	do {
		std::unique_lock lock(m_Mutex);

		pQuery->RequestId = MakeId();

		if(!m_Queries.insert(std::pair(pQuery->RequestId, pQuery)).second)
			pQuery->RequestId = 0;

	} while (!pQuery->RequestId);
	Query.ID = pQuery->RequestId;

	std::vector<uint8_t> Buffer = DNS::build_packet(Query);
	int iSent = sendto(m_Socket, (const char*)Buffer.data(), (int)Buffer.size(), 0, &m_ForwardAddress.address, m_ForwardAddress.addrlen);
	if (iSent == SOCKET_ERROR)
	{
		std::unique_lock lock(m_Mutex);
		m_Queries.erase(pQuery->RequestId);
	}

	return Ok;
}

void CDnsServer::SForwardedQuery::HandleResponse(DNS::Packet& Response, CDnsSocket* pSocket)
{
	Response.ID = QueryId; // restore original id

	((CDnsServer*)pSocket)->AddLocalAnswers(Response);
	((CDnsServer*)pSocket)->SendResponse(Response, Address);
}

void CDnsServer::SendResponse(DNS::Packet& Response, const SAddress& Address)
{
	std::vector<uint8_t> Buffer = DNS::build_packet(Response);

	int iSent = sendto(m_Socket, (const char*)Buffer.data(), (int)Buffer.size(), 0, &Address.address, Address.addrlen);
	if (iSent == SOCKET_ERROR)
	{
		int Err = WSAGetLastError();
		DbgPrint("sendto failed with error: %d\n", Err);
	}
}

void CDnsServer::Stop()
{
	Close();
}

bool CDnsServer::ResolveLocally(DNS::Packet& Query, const SAddress& Address) 
{ 
	DNS::Packet Response;

	Response.ID = Query.ID;
	Response.QR = true;
	Response.OPCODE = 0;
	Response.AA = false;
	Response.TC = false;
	Response.RD = Query.RD;
	Response.RA = false;
	Response.Z = false;
	Response.RCODE = 0;

	Response.query = Query.query;

	AddLocalAnswers(Response);
	SendResponse(Response, Address);
	return true;
}

void CDnsServer::AddLocalAnswers(DNS::Packet& Response)
{
	for (auto const &query : Response.query) 
	{
		std::string HostName = DNS::build_name(query.q_name, 0);
		if (query.q_class != DNS::Class::INet)
			continue;

		SKey key(HostName, query.q_type);
		auto range = m_EntryMap.equal_range(key);
		if (range.first == range.second)
			continue;

		Response.RCODE = 0; // clear error, we found something

		DNS::Answer Answer;
		Answer.r_name = query.q_name;
		Answer.r_type = query.q_type;
		Answer.r_class = query.q_class;
		Answer.r_ttl = std::chrono::minutes(10).count();
		do {
			Answer.r_data = range.first->second;
			Response.answer.push_back(Answer);
		} while (++range.first != range.second);
	}
}

bool CDnsServer::LoadHosts(const std::wstring& sHostsFile) 
{ 
	std::unique_lock lock(m_Mutex); 

	return LoadHosts(sHostsFile, m_EntryMap); 
}

bool CDnsServer::LoadHosts(const std::wstring& sHostsFile, std::multimap<SKey, DNS::Binary>& EntryMap)
{
	std::string host;
	std::string addr;
	std::string line;

	std::ifstream in(sHostsFile);

	while(std::getline(in, line)) 
	{
		std::istringstream is(line);
		DNS::Binary net_address;
		DNS::Type type;

		is >> addr;
		is >> host;

		if (addr.empty() || is.bad())
			continue;

		if (addr[0] == '#')
			continue;

		if (addr.find(':') != std::string::npos) { // ipv6
			struct in6_addr a6;
			if (inet_pton(AF_INET6, addr.c_str(), &a6) <= 0)
				continue;
			type = DNS::Type::AAAA;
			net_address = DNS::Binary( reinterpret_cast<DNS::Binary::const_pointer>(&a6), reinterpret_cast<DNS::Binary::const_pointer>(&a6 + 1));
		}
		else { // assume ipv4
			struct in_addr a;
			if (inet_pton(AF_INET, addr.c_str(), &a) <= 0)
				continue;
			type = DNS::Type::A;
			net_address = DNS::Binary( reinterpret_cast<DNS::Binary::const_pointer>(&a), reinterpret_cast<DNS::Binary::const_pointer>(&a + 1));
		}

		while(!is.bad() && !host.empty() && host[0] != '#') {
			
			EntryMap.emplace(SKey(host, type), net_address);

			host.clear();
			is >> host; // load aliasses
		}
	}

	return true;
}