#pragma once
#include "DnsSocket.h"

class CDnsClient
{
public:
	CDnsClient(const std::wstring& Address, std::shared_ptr<CDnsSocket> pSocket = std::shared_ptr<CDnsSocket>());
	virtual ~CDnsClient();
	virtual bool ResolveHost(const std::string& HostName, CDnsSocket::SAddress &Address);
	virtual bool ResolveHostV6(const std::string& HostName, CDnsSocket::SAddress &Address);

protected:
	virtual bool Resolve(const std::string& HostName, DNS::Type Type, DNS::Class Class, DNS::Packet& Response);

	CDnsSocket::SAddress m_Address;
	std::shared_ptr<CDnsSocket> m_pSocket;
};