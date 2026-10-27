#pragma once
#include "../Library/Common/Address.h"
#include "../Common/AbstractInfo.h"
#include "../Library/Common/StVariant.h"
#include "../Library/Common/MiscStats.h"
#include "Dns/DnsHostName.h"
#include "Firewall/FirewallDefs.h"

//typedef enum _ET_FIREWALL_STATUS
//{
//    FirewallUnknownStatus,
//    FirewallAllowedNotRestricted,
//    FirewallAllowedRestricted,
//    FirewallNotAllowedNotRestricted,
//    FirewallNotAllowedRestricted,
//    FirewallMaximumStatus
//} ET_FIREWALL_STATUS;

class CSocket: public CAbstractInfoEx
{
	TRACK_OBJECT(CSocket)
public:
	CSocket();
	~CSocket();

	uint64				GetHashID() const			{ std::shared_lock Lock(m_Mutex); return m_HashID; }

	uint32				GetProtocolType() const		{ std::shared_lock Lock(m_Mutex); return m_ProtocolType; }
	CAddress			GetLocalAddress() const		{ std::shared_lock Lock(m_Mutex); return m_LocalAddress; }
	uint16				GetLocalPort() const		{ std::shared_lock Lock(m_Mutex); return m_LocalPort; }
	CAddress			GetRemoteAddress() const	{ std::shared_lock Lock(m_Mutex); return m_RemoteAddress; }
	uint16				GetRemotePort() const		{ std::shared_lock Lock(m_Mutex); return m_RemotePort; }
	uint32				GetState() const			{ std::shared_lock Lock(m_Mutex); return m_State; }
	uint32				GetLocalScopeId() const		{ std::shared_lock Lock(m_Mutex); return m_LocalScopeId; }
	uint32				GetRemoteScopeId() const	{ std::shared_lock Lock(m_Mutex); return m_RemoteScopeId; }
	void				SetClosed();
	void				SetBlocked()				{ std::unique_lock Lock(m_Mutex); m_State = -1; }
	bool				WasBlocked() const			{ std::shared_lock Lock(m_Mutex); return m_State == -1; }

	// The direction is only known from the firewall log event (one per flow, see CSocketList::OnFwLogEvent),
	// the first one wins; reauthorizations repeat it and ETW does not carry it.
	void				SetDirection(EFwDirections Direction) { std::unique_lock Lock(m_Mutex); if (m_Direction == EFwDirections::Unknown) m_Direction = Direction; }
	EFwDirections		GetDirection() const		{ std::shared_lock Lock(m_Mutex); return m_Direction; }
	bool				IsConnection() const;		// open connection for the usage graph: has a peer, no listener, not closed or blocked
	uint64				GetProcessId() const		{ std::shared_lock Lock(m_Mutex); return m_ProcessId; }

	CHostNamePtr		GetRemoteHostName() const	{ std::shared_lock Lock(m_Mutex); return m_pRemoteHostName; }

	//std::wstring		GetProcessName() const		{ std::shared_lock Lock(m_Mutex); return m_ProcessName; }
	std::shared_ptr<class CProcess> GetProcess() const { std::shared_lock Lock(m_Mutex); return m_pProcess.lock(); }
	
	//SSockStats			GetStats() const			{ QReadLocker Locker(&m_StatsMutex); return m_Stats; }

	std::wstring		GetOwnerServiceName() const	{ std::shared_lock Lock(m_Mutex); return m_OwnerService; }

	//int					GetFirewallStatus();
	//std::wstring		GetFirewallStatusString();

	uint64				GetIdleTime() const;

	bool				HasStaticDataEx() const		{ std::shared_lock Lock(m_Mutex); return m_HasStaticDataEx; }

	void				UpdateStats();
	void				AddNetworkIO(int Type, uint32 TransferSize);

	// books the bytes to the usage log of the owning service or else the program file, see UsageStats.h
	void				AddUsageIO(int Type, uint32 TransferSize, bool bLan);
	// counts the open connections among the sockets and books them as connection seconds, once per usage sample
	static void			BookUsageConnections(const std::set<std::shared_ptr<CSocket>>& Sockets, uint32 Seconds);
	void				AddUsageConnections(bool bInbound, uint32 Seconds);

	uint64	GetLastActivity() const			{ std::shared_lock Lock(m_Mutex); return m_LastActivity;}

	uint64	GetUpload() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.SendRate.Get();}
	uint64	GetDownload() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.ReceiveRate.Get();}
	uint64	GetUploaded() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.SendRaw;}
	uint64	GetDownloaded() const			{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.ReceiveRaw;}

	enum EMatchMode
	{
		eFuzzy = 0,
		eStrict,
	};

	bool Match(uint64 ProcessId, uint32 ProtocolType, const CAddress& LocalAddress, uint16 LocalPort, const CAddress& RemoteAddress, uint16 RemotePort, EMatchMode Mode);

	static uint64 MkHash(uint64 ProcessId, uint32 ProtocolType, const CAddress& LocalAddress, uint16 LocalPort, const CAddress& RemoteAddress, uint16 RemotePort);

	StVariant ToVariant(FW::AbstractMemPool* pMemPool = nullptr) const;

protected:
	friend class CSocketList;

	void InitStaticData(uint64 ProcessId,
		uint32 ProtocolType, const CAddress& LocalAddress, uint16 LocalPort, const CAddress& RemoteAddress, uint16 RemotePort);
	void SetRawCreationTime(uint64 TimeStamp);
	void LinkProcess(const std::shared_ptr<class CProcess>& pProcess);
	void ResolveHost();

	void InitStaticDataEx(struct SNetworkSocket* connection);
	bool GetUsageTarget(std::shared_ptr<class CProgramFile>& pProgram, std::shared_ptr<class CWindowsService>& pService) const;
	void CopyStaticDataEx(CSocket* pOther);
	bool UpdateDynamicData(struct SNetworkSocket* connection);

	uint64						m_HashID = -1;

	uint32						m_ProtocolType = 0;
	CAddress					m_LocalAddress;
	uint16						m_LocalPort = 0;
	CAddress					m_RemoteAddress;
	uint16						m_RemotePort = 0;
	uint64						m_CreationTime = 0;
	uint32						m_State = 0;
	EFwDirections				m_Direction = EFwDirections::Unknown;
	uint64						m_ProcessId = 0;

	std::wstring				m_OwnerService;
	bool						m_HasStaticDataEx = false;

	uint32						m_LocalScopeId = 0; // Ipv6
    uint32						m_RemoteScopeId = 0; // Ipv6

	//ET_FIREWALL_STATUS			m_FwStatus = FirewallUnknownStatus;

	//std::wstring				m_ProcessName;
	std::weak_ptr<class CProcess> m_pProcess;

	CHostNamePtr				m_pRemoteHostName;
		
	uint64						m_LastActivity = 0;
	// I/O stats
	mutable std::shared_mutex	m_StatsMutex;
	SSockStats					m_Stats;

};

typedef std::shared_ptr<CSocket> CSocketPtr;