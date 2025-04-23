#include "pch.h"
#include "DnsClient.h"

CDnsClient::CDnsClient(const std::wstring& Address, std::shared_ptr<CDnsSocket> pSocket)
	: m_pSocket(pSocket)
{
	if (!m_pSocket) {
		m_pSocket = std::make_shared<CDnsSocket>();
		m_pSocket->Open(0);
	}

	CDnsSocket::ResolveAddress(Address, m_Address);
}

CDnsClient::~CDnsClient()
{
}

bool CDnsClient::Resolve(const std::string& HostName, DNS::Type Type, DNS::Class Class, DNS::Packet& Response)
{
	DNS::Packet Query;

	Query.ID = 0; // will be set by socket
	Query.QR = 0;
	Query.OPCODE = 0;
	Query.AA = false;
	Query.TC = false;
	Query.RD = true;
	Query.RA = false;
	Query.Z = false;
	Query.RCODE = 0;

	DNS::Name n;
	std::istringstream is(HostName);
	std::string part;
	while (std::getline(is, part, '.'))
		n.push_back(part);

	DNS::Query q;
	q.q_name = n;
	q.q_type = Type;
	q.q_class = Class;

	Query.query.push_back(q);

	if (!m_pSocket->SendQuery(m_Address, Query, Response))
		return false;
	return true;
}

bool CDnsClient::ResolveHost(const std::string& HostName, CDnsSocket::SAddress& Address)
{
	DNS::Packet Response;
	if (!Resolve(HostName, DNS::Type::A, DNS::Class::INet, Response))
		return false;

	memset(Address.buffer, 0, sizeof(Address.buffer));
	for (auto& Answer : Response.answer)
	{
		if (Answer.r_type == DNS::Type::A)
		{
			if (Answer.r_data.size() == 4)
			{
				Address.address.sa_family = AF_INET;
				memcpy(&((sockaddr_in*)&Address.address)->sin_addr, Answer.r_data.data(), 4);
				Address.addrlen = sizeof(sockaddr_in);
				return true;
			}
		}
	}

	return false;
}

bool CDnsClient::ResolveHostV6(const std::string& HostName, CDnsSocket::SAddress& Address)
{
	DNS::Packet Response;
	if (!Resolve(HostName, DNS::Type::AAAA, DNS::Class::INet, Response))
		return false;

	memset(Address.buffer, 0, sizeof(Address.buffer));
	for (auto& Answer : Response.answer)
	{
		if (Answer.r_type == DNS::Type::AAAA)
		{
			if (Answer.r_data.size() == 16)
			{
				Address.address.sa_family = AF_INET6;
				memcpy(&((sockaddr_in6*)&Address.address)->sin6_addr, Answer.r_data.data(), 16);
				Address.addrlen = sizeof(sockaddr_in6);
				return true;
			}
		}
	}

	return false;
}