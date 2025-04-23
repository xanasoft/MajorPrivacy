#include "pch.h"
#include "DnsSocket.h"

#define BUFSIZE 65536

CDnsSocket::CDnsSocket()
{
}

CDnsSocket::~CDnsSocket()
{
	Close();
}

DWORD CALLBACK CDnsSocket__Run(LPVOID lpThreadParameter)
{
	CDnsSocket* This = (CDnsSocket*)lpThreadParameter;
	This->Run();
	return 0;
}

bool CDnsSocket::Open(uint16 Port)
{
	if (m_Socket)
		return false;

	m_Socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (m_Socket == INVALID_SOCKET)
		return false;

	int optval = 1;
	setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(int));

	sockaddr_in serverAddr = { 0 };
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(Port);

	if (bind(m_Socket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
		closesocket(m_Socket);
		return false;
	}

	m_bRunning = true;
	m_hThread = CreateThread(NULL, 0, CDnsSocket__Run, (void*)this, 0, NULL);

	return true;
}

uint16 CDnsSocket::MakeId()
{
	if (m_IdGenerator == 0)
		m_IdGenerator = (uint16)time(nullptr);

	// Galois LFSR
	// https://en.wikipedia.org/wiki/Linear-feedback_shift_register

	unsigned lsb = m_IdGenerator & 1u;  /* Get LSB (i.e., the output bit). */
	m_IdGenerator >>= 1;                /* Shift register */
	if (lsb)                            /* If the output bit is 1, */
		m_IdGenerator ^= 0xB400u;       /* apply toggle mask. */

	return m_IdGenerator;
}

void CDnsSocket::Run()
{
	char Buffer[BUFSIZE];
	uint32 ReqCount = 0;

	while (m_bRunning)
	{
		SAddress Address;
		DNS::Packet Packet;

		int iReceived = recvfrom(m_Socket, Buffer, sizeof(Buffer), 0, &Address.address, &Address.addrlen);
		if (iReceived == SOCKET_ERROR)
		{
#ifdef _DEBUG
			int Err = WSAGetLastError();
			if(m_bRunning && Err != WSAEWOULDBLOCK)
				DbgPrint("recvfrom failed with error: %d\n", Err);
#endif
			continue;
		}
		
		if (!DNS::parse((uint8_t*)Buffer, iReceived, Packet))
			continue;

		HandlePacket(Packet, Address);

		// cleanup timeouts periodically
		if (ReqCount++ % 100)
		{
			std::unique_lock lock(m_Mutex);
			uint64 TimeOutSec = 30;			
			uint64 Now = time(nullptr);
			for (auto it = m_Queries.begin(); it != m_Queries.end();)
			{
				if (Now - it->second->TimeStamp > TimeOutSec)
					it = m_Queries.erase(it);
				else
					++it;
			}
		}
	}
}

bool CDnsSocket::HandlePacket(DNS::Packet& Packet, const SAddress& Address)
{
	if (Packet.QR == 1) // Response
	{
		std::unique_lock lock(m_Mutex);
		auto it = m_Queries.find(Packet.ID);
		if (it != m_Queries.end()) {
			it->second->HandleResponse(Packet, this);
			m_Queries.erase(it);
			return true;
		}
	}
	return false;
}

void CDnsSocket::SClientQuery::HandleResponse(DNS::Packet& Response, CDnsSocket* pSocket)
{
	*ResponsePtr = Response;

	{
		std::unique_lock lock(mtx);
		ready = true;
	}
	cv.notify_one();
}

bool CDnsSocket::SendQuery(const SAddress& Address, DNS::Packet& Query, DNS::Packet& Response)
{
	bool Ok = false;

	auto pQuery = std::make_shared<SClientQuery>();
	pQuery->TimeStamp = time(nullptr);
	pQuery->ResponsePtr = &Response;
	do {
		std::unique_lock lock(m_Mutex);
		
		pQuery->RequestId = MakeId();

		if(!m_Queries.insert(std::pair(pQuery->RequestId, pQuery)).second)
			pQuery->RequestId = 0;

	} while (!pQuery->RequestId);
	Query.ID = pQuery->RequestId;

	std::vector<uint8_t> Buffer = DNS::build_packet(Query);
	int iSent = sendto(m_Socket, (const char*)Buffer.data(), (int)Buffer.size(), 0, &Address.address, Address.addrlen);
	if (iSent == SOCKET_ERROR)
	{
		int Err = WSAGetLastError();
		DbgPrint("sendto failed with error: %d\n", Err);
	}
	else
	{
		std::unique_lock lock(pQuery->mtx);
		Ok = pQuery->cv.wait_for(lock, std::chrono::seconds(5), [pQuery](){ return pQuery->ready; });
	}

	if (!Ok)
	{
		std::unique_lock lock(m_Mutex);
		m_Queries.erase(pQuery->RequestId);
	}

	return Ok;
}

void CDnsSocket::Close()
{
	m_bRunning = false;

	if (m_Socket) {
		closesocket(m_Socket);
		m_Socket = NULL;
	}

	if (m_hThread) {
		if (WaitForSingleObject(m_hThread, 60 * 1000) == WAIT_TIMEOUT)
			TerminateThread(m_hThread, -1);
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}
}

bool CDnsSocket::ResolveAddress(const std::wstring& sAddress, SAddress& Address)
{
	struct addrinfoW hints = {0};
	//hints.ai_family = AF_UNSPEC; // any
	hints.ai_family = AF_INET;
	//hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	struct addrinfoW * result;
	if (GetAddrInfoW(sAddress.c_str(), L"53", &hints, &result) != 0) {
		return false;
	}

	if (result == NULL)
		return false;

	for (struct addrinfoW *rp = result; rp != NULL; rp = rp->ai_next) 
	{
		Address.address = *rp->ai_addr;
		Address.addrlen = (socklen_t)rp->ai_addrlen;
		break; // handle only one for now
	}

	FreeAddrInfoW(result);
	return true;
}