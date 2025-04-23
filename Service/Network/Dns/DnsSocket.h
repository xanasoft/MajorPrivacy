#pragma once
#include "../../Framework/Common/Buffer.h"
#include "DnsPacket.h"

class CDnsSocket
{
public:
	struct SAddress
	{
		SAddress() { addrlen = sizeof(buffer); }

		union {
			struct sockaddr address;
			char buffer[28];
		};
		socklen_t addrlen;

		std::string ntop() const
		{
			char host[NI_MAXHOST], service[NI_MAXSERV];
			int rc = getnameinfo(&address, addrlen, host, sizeof host, service, sizeof service, NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
			if (rc != 0)
				return "INVALID ADDRESS";
			return std::string(host) + ":" + std::string(service);
		}
	};

	CDnsSocket();
	virtual ~CDnsSocket();
	virtual bool Open(uint16 Port);
	virtual bool SendQuery(const SAddress& Address, DNS::Packet& Query, DNS::Packet& Response);
	virtual bool IsRunning() const { return m_bRunning; }
	virtual void Close();

	static bool ResolveAddress(const std::wstring& sAddress, SAddress& Address);

protected:
	friend DWORD CALLBACK CDnsSocket__Run(LPVOID lpThreadParameter);
	virtual void Run();

	virtual bool HandlePacket(DNS::Packet& Packet, const SAddress& Address);

	uint16 MakeId();

	volatile uint16 m_IdGenerator = 0;

	HANDLE m_hThread = NULL;
	SOCKET m_Socket = NULL;
	volatile bool m_bRunning = false;

	mutable std::mutex m_Mutex;

	struct SQuery
	{
		virtual ~SQuery() {}
		virtual void HandleResponse(DNS::Packet& Response, CDnsSocket* pSocket) = 0;

		uint16 RequestId = 0;
		uint64 TimeStamp = 0;
	};

	struct SClientQuery : SQuery
	{
		void HandleResponse(DNS::Packet& Response, CDnsSocket* pSocket) override;

		std::mutex mtx;
		std::condition_variable cv;
		bool ready = false;
		DNS::Packet* ResponsePtr = NULL;
	};


	std::map<uint16, std::shared_ptr<SQuery>> m_Queries;
};