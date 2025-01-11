#include "pch.h"
#include "Socket.h"
#include "SocketList.h"
#include "../ServiceCore.h"
#include "../Library/Helpers/MiscHelpers.h"
#include "../Library/Helpers/Service.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Processes/Process.h"
#include "../../Library/API/PrivacyAPI.h"
#include "NetworkManager.h"
#include "Dns/DnsInspector.h"
#include "../Library/Common/Strings.h"

#include <WS2tcpip.h>
#include <Iphlpapi.h>

CSocket::CSocket()
{
}

CSocket::~CSocket()
{
}

bool CSocket::Match(uint64 ProcessId, uint32 ProtocolType, const CAddress& LocalAddress, uint16 LocalPort, const CAddress& RemoteAddress, uint16 RemotePort, EMatchMode Mode)
{
	std::shared_lock Lock(m_Mutex);

	if (m_ProcessId != ProcessId)
		return false;

	if (m_ProtocolType != ProtocolType)
		return false;

	if (m_ProtocolType == IPPROTO_TCP || m_ProtocolType == IPPROTO_UDP)
	{
		if (m_LocalPort != LocalPort)
			return false;
	}

	// a socket may be bount to all adapters than it has a local null address
	if (Mode == eStrict || m_LocalAddress.IsNull()) // IsAny
	{
		if(m_LocalAddress != LocalAddress)
			return false;
	}

	// don't test the remote endpoint if this is a udp socket
	if (Mode == eStrict || m_ProtocolType == IPPROTO_TCP)
	{
		if (m_ProtocolType == IPPROTO_TCP || m_ProtocolType == IPPROTO_UDP)
		{
			if (m_RemotePort != RemotePort)
				return false;
		}

		if (m_RemoteAddress != RemoteAddress)
			return false;
	}

	return true;
}

uint64 CSocket::MkHash(uint64 ProcessId, uint32 ProtocolType, const CAddress& LocalAddress, uint16 LocalPort, const CAddress& RemoteAddress, uint16 RemotePort)
{
	if (ProtocolType == IPPROTO_UDP)
		RemotePort = 0;

	uint64 HashID = ((uint64)LocalPort << 0) | ((uint64)RemotePort << 16) | (uint64)(((uint32*)&ProcessId)[0] ^ ((uint32*)&ProcessId)[1]) << 32;

	return HashID;
}

void CSocket::InitStaticData(uint64 ProcessId, uint32 ProtocolType,
	const CAddress& LocalAddress, uint16 LocalPort, const CAddress& RemoteAddress, uint16 RemotePort)
{
	m_ProtocolType = ProtocolType;
	m_LocalAddress = LocalAddress;
	m_LocalPort = LocalPort;
	m_RemoteAddress = RemoteAddress;
	m_RemotePort = RemotePort;
	m_ProcessId = ProcessId;

	/*if(m_ProcessId == 0)
		m_ProcessName = tr("Waiting connections");
	else
		m_ProcessName = tr("Unknown process PID: %1").arg(m_ProcessId);*/

	// generate a somewhat unique id to optimize std::map search
	m_HashID = MkHash(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort);
}

void CSocket::SetRawCreationTime(uint64 TimeStamp)
{
	std::unique_lock Lock(m_Mutex);

	m_CreationTime = TimeStamp;
	m_CreateTimeStamp = FILETIME2ms(TimeStamp);
}

void CSocket::LinkProcess(const CProcessPtr& pProcess)
{
	//m_ProcessName = pProcess->GetName();
	m_pProcess = pProcess; // relember m_pProcess is a week pointer

	//ProcessSetNetworkFlag();
}

void CSocket::ResolveHost()
{
	if (!m_RemoteAddress.IsNull()) {
		if(!m_pRemoteHostName) m_pRemoteHostName = std::make_shared<CResHostName>(m_CreationTime);
		CProcessPtr pProcess = m_pProcess.lock();
		if(!pProcess || !pProcess->DnsLog()->ResolveHost(m_RemoteAddress, m_pRemoteHostName))
			theCore->NetworkManager()->DnsInspector()->ResolveHost(m_RemoteAddress, m_pRemoteHostName);
	}
}

void CSocket::InitStaticDataEx(struct SNetworkSocket* connection)
{
	SetRawCreationTime(connection->CreateTime);

	std::unique_lock Lock(m_Mutex);

	ULONG serviceTag = *(PULONG)connection->OwnerInfo;
	m_OwnerService = GetServiceNameFromTag((HANDLE)connection->ProcessId, serviceTag);
#ifdef _DEBUG
	if (!m_OwnerService.empty())
	{
		if (CProcessPtr pProcess = m_pProcess.lock())
		{
			auto Services = pProcess->GetServices();
			if (Services.find(MkLower(m_OwnerService)) == Services.end())
			{
				DebugBreak();
				// todo;
			}
		}
	}
#endif

	m_LocalScopeId = connection->LocalScopeId;
	m_RemoteScopeId = connection->RemoteScopeId;

	m_HasStaticDataEx = true;
}

void CSocket::CopyStaticDataEx(CSocket* pOther)
{
	std::unique_lock Lock(m_Mutex);

	std::unique_lock OtherLock(pOther->m_Mutex);

	m_HasStaticDataEx = true;

	m_OwnerService = pOther->m_OwnerService;

	m_LocalScopeId = pOther->m_LocalScopeId;
	m_RemoteScopeId = pOther->m_RemoteScopeId;
}

bool CSocket::UpdateDynamicData(struct SNetworkSocket* connection)
{
	std::unique_lock Lock(m_Mutex);

    BOOLEAN modified = FALSE;

	/*if (m_pProcess.isNull())
	{
		CProcessPtr pProcess = theAPI->GetProcessByID(m_ProcessId);
		if (!pProcess.isNull())
		{
			m_pProcess = pProcess; // relember m_pProcess is a week pointer

			ProcessSetNetworkFlag();

			if (m_ProcessName.isEmpty())
			{
				m_ProcessName = pProcess->GetName();
				//m_SubsystemProcess = pWinProc->IsSubsystemProcess();
				//PhpUpdateNetworkItemOwner(networkItem, networkItem->ProcessItem);
				modified = TRUE;
			}
		}
	}*/

	if (m_State != connection->State)
    {
        m_State = connection->State;
        modified = TRUE;

		//if (connection->State == MIB_TCP_STATE_LISTEN)
		//	ProcessSetNetworkFlag();
    }

	//UpdateStats();

	return modified;
}


void CSocket::SetClosed()
{ 
	std::shared_lock Lock(m_Mutex);
	if(m_State != -1)
		m_State = MIB_TCP_STATE_CLOSED; 
	Lock.unlock();

	std::shared_lock StatsLock(m_StatsMutex);
	m_Stats.Net.ReceiveDelta.Delta = 0;
	m_Stats.Net.SendDelta.Delta = 0;
	m_Stats.Net.ReceiveRawDelta.Delta = 0;
	m_Stats.Net.SendRawDelta.Delta = 0;
	m_Stats.Net.ReceiveRate.Clear();
	m_Stats.Net.SendRate.Clear();
}

void CSocket::UpdateStats()
{
	std::shared_lock StatsLock(m_StatsMutex);

	m_Stats.UpdateStats();
}

uint64 CSocket::GetIdleTime() const
{
	std::shared_lock StatsLock(m_StatsMutex);

	return GetCurTime() * 1000ULL - m_LastActivity;
}

void CSocket::AddNetworkIO(int Type, uint32 TransferSize)
{
	std::shared_lock StatsLock(m_StatsMutex);

	m_LastActivity = GetCurTime() * 1000ULL;
	m_RemoveTimeStamp = 0;

	switch ((CSocketList::EEventType)Type)
	{
	case CSocketList::EEventType::Receive:	m_Stats.Net.AddReceive(TransferSize); break;
	case CSocketList::EEventType::Send:		m_Stats.Net.AddSend(TransferSize); break;
	}
}

CVariant CSocket::ToVariant() const
{
	std::shared_lock Lock(m_Mutex);

	CVariant Socket;

	Socket.BeginIMap();

	Socket.Write(API_V_SOCK_REF, (uint64)this);
	//m_HashID; // not guaranteed unique

	Socket.Write(API_V_SOCK_TYPE, m_ProtocolType);
	Socket.Write(API_V_SOCK_LADDR, m_LocalAddress.ToString());
	Socket.Write(API_V_SOCK_LPORT, m_LocalPort);
	Socket.Write(API_V_SOCK_RADDR, m_RemoteAddress.ToString());
	Socket.Write(API_V_SOCK_RPORT, m_RemotePort);
	Socket.Write(API_V_SOCK_STATE, m_State);
	Socket.Write(API_V_SOCK_LSCOPE, m_LocalScopeId); // Ipv6
	Socket.Write(API_V_SOCK_RSCOPE, m_RemoteScopeId); // Ipv6

	//Socket.Write(API_V_SOCK_ACCESS, );
	//m_FwStatus

	Socket.Write(API_V_PID, m_ProcessId);
	Socket.Write(API_V_SERVICE_TAG, m_OwnerService);
	//m_ProcessName;
	//m_pProcess;

	Socket.Write(API_V_SOCK_RHOST, m_pRemoteHostName ? m_pRemoteHostName->ToString() : L"");

	Socket.Write(API_V_CREATE_TIME, m_CreateTimeStamp); // in ms

	Socket.Write(API_V_SOCK_LAST_NET_ACT, m_LastActivity);

	Lock.unlock();

	std::shared_lock StatsLock(m_StatsMutex);

	Socket.Write(API_V_SOCK_UPLOAD, m_Stats.Net.SendRate.Get());
	Socket.Write(API_V_SOCK_DOWNLOAD, m_Stats.Net.ReceiveRate.Get());
	Socket.Write(API_V_SOCK_UPLOADED, m_Stats.Net.SendRaw);
	Socket.Write(API_V_SOCK_DOWNLOADED, m_Stats.Net.ReceiveRaw);

	Socket.Finish();

	return Socket;
}
