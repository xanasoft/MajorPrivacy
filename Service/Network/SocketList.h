#pragma once
#include "../Library/Status.h"
#include "Socket.h"
#include "Programs/ProgramID.h"

#define NETWORK_OWNER_INFO_SIZE 16

struct SNetworkSocket
{
	uint32 ProtocolType = 0;
	CAddress LocalAddress;
	uint16 LocalPort = 0;
	CAddress RemoteAddress;
	uint16 RemotePort = 0;
	uint32 State = 0;
	uint64 ProcessId = 0;
	uint64 CreateTime = 0;
	uint64 OwnerInfo[NETWORK_OWNER_INFO_SIZE];
	uint32 LocalScopeId = 0; // Ipv6
	uint32 RemoteScopeId = 0; // Ipv6
};

class CSocketList
{
	TRACK_OBJECT(CSocketList)
public:
	CSocketList();
	~CSocketList();

	STATUS Init();

	void Update();

	STATUS EnumSockets();

	std::unordered_multimap<uint64, CSocketPtr> List() { 
		std::shared_lock Lock(m_Mutex);
		return m_List; 
	}

	std::set<CSocketPtr> FindSockets(const CProgramID& ID);

	std::set<CSocketPtr> GetAllSockets();

	static std::unordered_multimap<uint64, CSocketPtr>::iterator FindSocketEntry(std::unordered_multimap<uint64, CSocketPtr>& Sockets, uint64 ProcessId, 
		uint32 ProtocolType, const CAddress& LocalAddress, uint16 LocalPort, const CAddress& RemoteAddress, uint16 RemotePort, CSocket::EMatchMode Mode);

	CSocketPtr FindSocket(uint64 ProcessId,
		uint32 ProtocolType, const CAddress& LocalAddress, uint16 LocalPort, const CAddress& RemoteAddress, uint16 RemotePort, CSocket::EMatchMode Mode);

	uint64	GetUpload() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.SendRate.Get();}
	uint64	GetDownload() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.ReceiveRate.Get();}
	uint64	GetUploaded() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.SendRaw;}
	uint64	GetDownloaded() const			{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.ReceiveRaw;}

	enum class EEventType
    {
        Unknown = 0,

        Allow = 1,
        Block = 2,

		Receive = 3,
		Send = 4
    };

protected:
	friend class CFirewall;
	
	CSocketPtr OnNetworkEvent(EEventType Type, uint64 ProcessId, uint32 ProtocolType, uint32 TransferSize,
		CAddress LocalAddress, uint16 LocalPort, CAddress RemoteAddress, uint16 RemotePort, uint64 TimeStamp);

	CSocketPtr OnFwLogEvent(const struct SWinFwLogEvent* pEvent);
	void OnEtwNetEvent(const struct SEtwNetworkEvent* pEvent);

	std::shared_mutex m_Mutex;
	std::unordered_multimap<uint64, CSocketPtr> m_List;

	// I/O stats
	mutable std::shared_mutex	m_StatsMutex;
	SSockStats					m_Stats;
};