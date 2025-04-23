#include "pch.h"
#include "SocketList.h"
#include "../Framework/Common/Buffer.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/Scoped.h"
#include "../Etw/EtwEventMonitor.h"
#include "./Firewall/WindowsFwLog.h"
#include "ServiceCore.h"
#include "../Processes/ProcessList.h"

#include <WS2tcpip.h>
#include <Iphlpapi.h>


CSocketList::CSocketList()
{
}

CSocketList::~CSocketList()
{
}

STATUS CSocketList::Init()
{
    theCore->EtwEventMonitor()->RegisterNetworkHandler(&CSocketList::OnEtwNetEvent, this);

	EnumSockets();

	return OK;
}

void CSocketList::Update()
{
	EnumSockets();
}

std::unordered_multimap<uint64, CSocketPtr>::iterator CSocketList::FindSocketEntry(std::unordered_multimap<uint64, CSocketPtr>& Sockets, uint64 ProcessId, 
    uint32 ProtocolType, const CAddress& LocalAddress, uint16 LocalPort, const CAddress& RemoteAddress, uint16 RemotePort, CSocket::EMatchMode Mode)
{
	uint64 HashID = CSocket::MkHash(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort);

	for (std::unordered_multimap<uint64, CSocketPtr>::iterator I = Sockets.find(HashID); I != Sockets.end() && I->first == HashID; I++)
	{
		if (I->second->Match(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, Mode))
			return I;
	}

	// no matching entry
	return Sockets.end();
}

CSocketPtr CSocketList::FindSocket(uint64 ProcessId, 
    uint32 ProtocolType, const CAddress& LocalAddress, uint16 LocalPort, const CAddress& RemoteAddress, uint16 RemotePort, CSocket::EMatchMode Mode)
{
	std::shared_lock Lock(m_Mutex);

	auto I = FindSocketEntry(m_List, ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, Mode);
	if (I == m_List.end())
		return NULL;
	return I->second;
}

STATUS CSocketList::EnumSockets()
{
    std::vector<SNetworkSocket> Sockets;
    size_t Index = 0;

    ULONG uTableSize;
    CBuffer Buffer;

    uTableSize = 0;
    GetExtendedTcpTable(NULL, &uTableSize, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0);
    Buffer.SetSize(0, true, uTableSize);
    if (GetExtendedTcpTable(Buffer.GetBuffer(), &uTableSize, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
    {
        PMIB_TCPTABLE_OWNER_MODULE tcp4Table = (PMIB_TCPTABLE_OWNER_MODULE)Buffer.GetBuffer();

        Sockets.resize(Index + tcp4Table->dwNumEntries);

        for (DWORD i = 0; i < tcp4Table->dwNumEntries; i++)
        {
            SNetworkSocket* pSocket = &Sockets[Index++];

            pSocket->ProtocolType = IPPROTO_TCP;

            pSocket->LocalAddress = CAddress(ntohl(tcp4Table->table[i].dwLocalAddr));
            pSocket->LocalPort = _byteswap_ushort((USHORT)tcp4Table->table[i].dwLocalPort);

            pSocket->RemoteAddress = CAddress(ntohl(tcp4Table->table[i].dwRemoteAddr));
            pSocket->RemotePort = _byteswap_ushort((USHORT)tcp4Table->table[i].dwRemotePort);

            pSocket->State = tcp4Table->table[i].dwState;
            pSocket->ProcessId = tcp4Table->table[i].dwOwningPid;
            pSocket->CreateTime = tcp4Table->table[i].liCreateTimestamp.QuadPart;
            memcpy(pSocket->OwnerInfo, tcp4Table->table[i].OwningModuleInfo, sizeof(ULONGLONG) * min(NETWORK_OWNER_INFO_SIZE, TCPIP_OWNING_MODULE_SIZE));
        }
    }

    uTableSize = 0;
    GetExtendedTcpTable(NULL, &uTableSize, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0);
    Buffer.SetSize(0, true, uTableSize);
    if (GetExtendedTcpTable(Buffer.GetBuffer(), &uTableSize, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
    {
        PMIB_TCP6TABLE_OWNER_MODULE tcp6Table = (PMIB_TCP6TABLE_OWNER_MODULE)Buffer.GetBuffer();

        Sockets.resize(Index + tcp6Table->dwNumEntries);

        for (DWORD i = 0; i < tcp6Table->dwNumEntries; i++)
        {
            SNetworkSocket* pSocket = &Sockets[Index++];

            pSocket->ProtocolType = IPPROTO_TCP;

            pSocket->LocalAddress = CAddress(tcp6Table->table[i].ucLocalAddr);
            pSocket->LocalPort = _byteswap_ushort((USHORT)tcp6Table->table[i].dwLocalPort);

            pSocket->RemoteAddress = CAddress(tcp6Table->table[i].ucRemoteAddr);
            pSocket->RemotePort = _byteswap_ushort((USHORT)tcp6Table->table[i].dwRemotePort);

            pSocket->State = tcp6Table->table[i].dwState;
            pSocket->ProcessId = tcp6Table->table[i].dwOwningPid;
            pSocket->CreateTime = tcp6Table->table[i].liCreateTimestamp.QuadPart;
            memcpy(pSocket->OwnerInfo, tcp6Table->table[i].OwningModuleInfo, sizeof(ULONGLONG) * min(NETWORK_OWNER_INFO_SIZE, TCPIP_OWNING_MODULE_SIZE));
        }
    }

    uTableSize = 0;
    GetExtendedUdpTable(NULL, &uTableSize, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0);
    Buffer.SetSize(0, true, uTableSize);
    if (GetExtendedUdpTable(Buffer.GetBuffer(), &uTableSize, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
    {
        PMIB_UDPTABLE_OWNER_MODULE udp4Table = (PMIB_UDPTABLE_OWNER_MODULE)Buffer.GetBuffer();

        Sockets.resize(Index + udp4Table->dwNumEntries);

        for (DWORD i = 0; i < udp4Table->dwNumEntries; i++)
        {
            SNetworkSocket* pSocket = &Sockets[Index++];

            pSocket->ProtocolType = IPPROTO_UDP;

            pSocket->LocalAddress = CAddress(ntohl(udp4Table->table[i].dwLocalAddr));
            pSocket->LocalPort = _byteswap_ushort((USHORT)udp4Table->table[i].dwLocalPort);

            //pSocket->RemoteAddress = 
            //pSocket->RemotePort = 

            pSocket->State = 0;
            pSocket->ProcessId = udp4Table->table[i].dwOwningPid;
            pSocket->CreateTime = udp4Table->table[i].liCreateTimestamp.QuadPart;
            memcpy(pSocket->OwnerInfo, udp4Table->table[i].OwningModuleInfo, sizeof(ULONGLONG) * min(NETWORK_OWNER_INFO_SIZE, TCPIP_OWNING_MODULE_SIZE));
        }
    }

    uTableSize = 0;
    GetExtendedUdpTable(NULL, &uTableSize, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0);
    Buffer.SetSize(0, true, uTableSize);
    if (GetExtendedUdpTable(Buffer.GetBuffer(), &uTableSize, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
    {
        PMIB_UDP6TABLE_OWNER_MODULE udp6Table = (PMIB_UDP6TABLE_OWNER_MODULE)Buffer.GetBuffer();

        Sockets.resize(Index + udp6Table->dwNumEntries);

        for (DWORD i = 0; i < udp6Table->dwNumEntries; i++)
        {
            SNetworkSocket* pSocket = &Sockets[Index++];

            pSocket->ProtocolType = IPPROTO_UDP;

            pSocket->LocalAddress = CAddress(udp6Table->table[i].ucLocalAddr);
            pSocket->LocalPort = _byteswap_ushort((USHORT)udp6Table->table[i].dwLocalPort);

            //pSocket->RemoteAddress = 
            //pSocket->RemotePort = 

            pSocket->State = 0;
            pSocket->ProcessId = udp6Table->table[i].dwOwningPid;
            pSocket->CreateTime = udp6Table->table[i].liCreateTimestamp.QuadPart;
            memcpy(pSocket->OwnerInfo, udp6Table->table[i].OwningModuleInfo, sizeof(ULONGLONG) * min(NETWORK_OWNER_INFO_SIZE, TCPIP_OWNING_MODULE_SIZE));
        }
    }

	std::unique_lock Lock(m_Mutex);
	
    std::unordered_multimap<uint64, CSocketPtr> OldSockets = m_List;
    
    for (size_t i = 0; i < Sockets.size(); i++)
	{
		CSocketPtr pSocket;

		auto I = FindSocketEntry(OldSockets, Sockets[i].ProcessId, 
            Sockets[i].ProtocolType, Sockets[i].LocalAddress, Sockets[i].LocalPort, Sockets[i].RemoteAddress, Sockets[i].RemotePort, CSocket::eStrict);
        if (I != OldSockets.end()) {
            pSocket = I->second;
			OldSockets.erase(I);
		} 
		else
        {
            pSocket = std::make_shared<CSocket>();
			pSocket->InitStaticData(Sockets[i].ProcessId, 
                Sockets[i].ProtocolType, Sockets[i].LocalAddress, Sockets[i].LocalPort, Sockets[i].RemoteAddress, Sockets[i].RemotePort);
			pSocket->InitStaticDataEx(&(Sockets[i]));
			if (pSocket->m_ProcessId) {
				CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pSocket->m_ProcessId, true); // Note: this will add the process and load some basic data if it does not already exist
				pSocket->LinkProcess(pProcess);
				pProcess->AddSocket(pSocket);
			}
			pSocket->ResolveHost();

			m_List.insert(std::make_pair(pSocket->GetHashID(), pSocket));
		}

		if (!pSocket->HasStaticDataEx()) // on network events we may add new, yet un enumerated, sockets 
		{
			bool bNoOwnerSvc = pSocket->m_OwnerService.empty();
			
			pSocket->InitStaticDataEx(&(Sockets[i]));

			// if we now detect the socket belonging to a service we re add it
			if (bNoOwnerSvc != pSocket->m_OwnerService.empty()) {
				CProcessPtr pProcess = pSocket->GetProcess();
				if (pProcess) {
					pProcess->RemoveSocket(pSocket, true);
					pProcess->AddSocket(pSocket);
				}
			}
		}

		pSocket->UpdateDynamicData(&(Sockets[i]));
	}

	uint64 UdpPersistenceTime = theCore->Config()->GetUInt64("Service", "UdpPersistenceTime", 300);
	// For UDP Traffic keep the pseudo connections listed for a while
	for(auto I = OldSockets.begin(); I != OldSockets.end(); )
	{
        CSocketPtr pSocket = I->second;
		bool bIsUDPPseudoCon = pSocket->GetProtocolType() == IPPROTO_UDP && pSocket->GetRemotePort() != 0;

		if (bIsUDPPseudoCon && pSocket->GetIdleTime() < UdpPersistenceTime) // Active UDP pseudo connection
		{
			pSocket->UpdateStats();
			I = OldSockets.erase(I);
		}
		else // closed connection
		{
			uint32 State = pSocket->GetState();
			if (State != -1 && State != MIB_TCP_STATE_CLOSED)
				pSocket->SetClosed();
			I++;
		}
	}

	// purge all sockets left as thay are not longer running
	for(auto I = OldSockets.begin(); I != OldSockets.end(); ++I)
	{
        CSocketPtr pSocket = I->second;
		if (pSocket->CanBeRemoved())
		{
			if(CProcessPtr pProcess = pSocket->GetProcess())
				pProcess->RemoveSocket(pSocket);

            mmap_erase(m_List, pSocket->GetHashID(), pSocket);
		}
		else if (!pSocket->IsMarkedForRemoval()) 
			pSocket->MarkForRemoval();
	}

	Lock.unlock();

	std::shared_lock StatsLock(m_StatsMutex);

	m_Stats.UpdateStats();

    return OK;
}

CSocketPtr CSocketList::OnNetworkEvent(EEventType Type, uint64 ProcessId, uint32 ProtocolType, uint32 TransferSize,
    CAddress LocalAddress, uint16 LocalPort, CAddress RemoteAddress, uint16 RemotePort, uint64 TimeStamp)
{
	std::unique_lock Lock(m_Mutex);

    CSocket::EMatchMode Mode = CSocket::eFuzzy;
	if (ProtocolType == IPPROTO_UDP && theCore->Config()->GetBool("Service", "UseUDPPseudoConnectins", true))
		Mode = CSocket::eStrict;

	CSocketPtr pSocket;
	auto I = FindSocketEntry(m_List, ProcessId, 
		ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, Mode);
	if (I != m_List.end())
		pSocket = I->second;
	else
	{
		pSocket = std::make_shared<CSocket>();
		pSocket->InitStaticData(ProcessId, 
            ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort);
		pSocket->SetRawCreationTime(TimeStamp); // this may be later then the actual creation time, but it is the best we can do at the moment, alternatively we coudl re list all sockets

		//
		// if this is a UDP pseudo connection try to copy the data from the actual socket,
		// Note: this often fails for short lived udp sockets :/
		//

		bool bIsUDPPseudoCon = ProtocolType == IPPROTO_UDP && RemotePort != 0;
		if (bIsUDPPseudoCon)
		{
			auto J = FindSocketEntry(m_List, ProcessId, 
				ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, CSocket::eFuzzy);
			if (J != m_List.end())
				pSocket->CopyStaticDataEx(J->second.get());
		}

		if (pSocket->m_ProcessId) {
			CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pSocket->m_ProcessId, true); // Note: this will add the process and load some basic data if it does not already exist
			pSocket->LinkProcess(pProcess);
			pProcess->AddSocket(pSocket);
		}
		pSocket->ResolveHost();

		if (Type == EEventType::Block)
			pSocket->SetBlocked();
		
		m_List.insert(std::make_pair(pSocket->GetHashID(), pSocket));
	}

	CProcessPtr pProcess = pSocket->GetProcess();
    if (pProcess)
		pProcess->AddNetworkIO((int)Type, TransferSize);
	pSocket->AddNetworkIO((int)Type, TransferSize);

	std::shared_lock StatsLock(m_StatsMutex);
	switch ((CSocketList::EEventType)Type)
	{
	case CSocketList::EEventType::Receive:	m_Stats.Net.AddReceive(TransferSize); break;
	case CSocketList::EEventType::Send:		m_Stats.Net.AddSend(TransferSize); break;
	}

	return pSocket;
}

CSocketPtr CSocketList::OnFwLogEvent(const struct SWinFwLogEvent* pEvent)
{
    EEventType EventType = EEventType::Unknown;
    switch (pEvent->Type)
    {
    case EFwActions::Allow: EventType = EEventType::Allow; break;
    case EFwActions::Block: EventType = EEventType::Block; break;
    }
    return OnNetworkEvent(EventType, pEvent->ProcessId, (uint32)pEvent->ProtocolType, 0, pEvent->LocalAddress, pEvent->LocalPort, pEvent->RemoteAddress, pEvent->RemotePort, pEvent->TimeStamp);
}

void CSocketList::OnEtwNetEvent(const struct SEtwNetworkEvent* pEvent)
{
    EEventType EventType = EEventType::Unknown;
    switch (pEvent->Type)
    {
	case SEtwNetworkEvent::EType::Send: EventType = EEventType::Send; break;
    case SEtwNetworkEvent::EType::Receive: EventType = EEventType::Receive; break;
    }
    OnNetworkEvent(EventType, pEvent->ProcessId, pEvent->ProtocolType, pEvent->TransferSize, pEvent->LocalAddress, pEvent->LocalPort, pEvent->RemoteAddress, pEvent->RemotePort, pEvent->TimeStamp);
}

std::set<CSocketPtr> CSocketList::FindSockets(const CProgramID& ID)
{
	if (ID.GetType() == EProgramType::eAllPrograms)
		return GetAllSockets();

	auto Processes = theCore->ProcessList()->FindProcesses(ID);

	std::shared_lock<std::shared_mutex> Lock(m_Mutex);

	std::set<CSocketPtr> Found;

	for (auto P : Processes)
	{
		auto pProcess = P.second;
		auto Sockets = pProcess->GetSocketList();
		for (auto pSocket : Sockets)
		{
			if (ID.GetType() == EProgramType::eWindowsService)
			{
				//
				// Note: Multiple services may be hosted in one svchost.exe instance
				// hence if we encounter that case we check the service tag
				//

				if (pProcess->GetServices().size() > 1)
				{
					if (pSocket->GetOwnerServiceName() != ID.GetServiceTag()) // todo to lower?
						continue;
				}
			}

			Found.insert(pSocket);
		}
	}

	return Found;
}

std::set<CSocketPtr> CSocketList::GetAllSockets()
{
	std::shared_lock<std::shared_mutex> Lock(m_Mutex);

	std::set<CSocketPtr> All;

	for (auto I : m_List)
		All.insert(I.second);
	//std::transform(m_List->begin(), m_List->end(), std::inserter(*All), [](auto I) { return I.second; });
	return All;
}