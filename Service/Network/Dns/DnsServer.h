#pragma once

#include "DnsSocket.h"

class CDnsServer : public CDnsSocket
{
public:
	CDnsServer();
	virtual ~CDnsServer();
	virtual bool SetForwardAddress(const std::wstring& sAddress);
	virtual std::wstring GetForwardAddress() const { std::unique_lock lock(m_Mutex); return m_sForwardAddress; }
	virtual bool IsForwarder() const { return m_ForwardAddress.addrlen > 0; }
	virtual bool Start();
	virtual void Stop();
	virtual bool LoadHosts(const std::wstring& sHostsFile);
	virtual void Clear() { m_EntryMap.clear(); }

protected:
	friend class SForwardedQuery;

	std::wstring m_sForwardAddress;
	SAddress m_ForwardAddress;

	bool HandlePacket(DNS::Packet& Packet, const SAddress& Address) override;

	virtual bool ForwardQuery(DNS::Packet& Packet, const SAddress& Address);

	struct SForwardedQuery : SQuery
	{
		void HandleResponse(DNS::Packet& Response, CDnsSocket* pSocket) override;

		SAddress Address;
		uint16 QueryId = 0;
	};

	virtual void SendResponse(DNS::Packet& Response, const SAddress& Address);

	virtual bool ResolveLocally(DNS::Packet& Query, const SAddress& Address);

	virtual void AddLocalAnswers(DNS::Packet& Response);

	struct SKey
	{
		SKey(const std::string& name, DNS::Type type) : name(name) , type(type) {}

		bool operator<(const SKey& key) const
		{
			int rc = name.compare(key.name);
			if (rc != 0)
				return rc < 0;

			return type < key.type ? true : false;
		}


		std::string name;
		DNS::Type type;
	};

	virtual bool LoadHosts(const std::wstring& sHostsFile, std::multimap<SKey, DNS::Binary>& EntryMap);

	std::multimap<SKey, DNS::Binary> m_EntryMap;
};
