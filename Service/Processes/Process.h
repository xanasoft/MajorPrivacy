#pragma once
#include "../Common/AbstractInfo.h"
#include "../Library/Common/StVariant.h"
#include "../Network/Socket.h"
#include "../Network/Dns/DnsProcLog.h"
#include "../Access/Handle.h"
#include "../Library/API/PrivacyDefs.h"
#include "../Programs/ImageSignInfo.h"
#include "../Library/Common/FlexGuid.h"

struct SProcessUID
{
	SProcessUID() {}
	SProcessUID(uint64 uPid, uint64 msTime);
	__inline uint64 Get() const { return PUID; }
	__inline void Set(uint64 UID) { PUID = UID; }
	uint64 PUID = 0;

	bool operator==(const SProcessUID& other) const { return PUID == other.PUID; }
	bool operator!=(const SProcessUID& other) const { return PUID != other.PUID; }
	bool operator<(const SProcessUID& other) const { return PUID < other.PUID; }
	bool operator<=(const SProcessUID& other) const { return PUID <= other.PUID; }
	bool operator>(const SProcessUID& other) const { return PUID > other.PUID; }
	bool operator>=(const SProcessUID& other) const { return PUID >= other.PUID; }
};

class CProcess: public CAbstractInfoEx
{
	TRACK_OBJECT(CProcess)
public:
	CProcess(uint64 Pid);
	~CProcess();

	uint64 GetProcessId() const { std::shared_lock Lock(m_Mutex); return m_Pid; }
	uint64 GetParentId() const { std::shared_lock Lock(m_Mutex); return m_ParentPid; }
	uint64 GetCreatorId() const { std::shared_lock Lock(m_Mutex); return m_CreatorPid; }
	SProcessUID GetProcessUID() const { std::shared_lock Lock(m_Mutex); return SProcessUID(m_Pid, m_CreateTimeStamp); }
	std::wstring GetName() const { std::shared_lock Lock(m_Mutex); return m_Name; }
	std::wstring GetNtFilePath() const { std::shared_lock Lock(m_Mutex); return m_NtFilePath; }
	std::wstring GetWorkDir() const;

	CFlexGuid GetEnclave() const { std::shared_lock Lock(m_Mutex); return m_EnclaveGuid; }
	uint32 GetSecState() const { std::shared_lock Lock(m_Mutex); return m_SecState; }
	uint32 GetFlags() const { std::shared_lock Lock(m_Mutex); return m_Flags; }
	uint32 GetSecFlags() const { std::shared_lock Lock(m_Mutex); return m_SecFlags; }

	bool IsInitDone() const { std::shared_lock Lock(m_Mutex); return m_ParentPid != -1; }

	void UpdateSignInfo(const struct SVerifierInfo* pVerifyInfo);
	CImageSignInfo GetSignInfo() const { std::shared_lock Lock(m_Mutex); return m_SignInfo; }
	void SetSignInfo(const CImageSignInfo& SignInfo) { std::unique_lock Lock(m_Mutex); m_SignInfo = SignInfo; }
	bool HashInfoUnknown() const;
	static bool FillVerifyInfo(const std::wstring& ModulePath, SVerifierInfo& VerifyInfo);

	std::set<std::wstring> GetServices() const { std::shared_lock Lock(m_Mutex);  return m_ServiceList; }

	std::wstring GetAppContainerSid() const { std::shared_lock Lock(m_Mutex); return m_AppContainerSid; }
	std::wstring GetAppContainerName() const { std::shared_lock Lock(m_Mutex); return m_AppContainerName; }
	std::wstring GetAppPackageName() const { std::shared_lock Lock(m_Mutex); return m_AppPackageName; }

	void AddLibrary(const std::shared_ptr<class CProgramLibrary>& pLibrary) { std::unique_lock Lock(m_Mutex); m_Libraries.insert(pLibrary); }
	void RemoveLibrary(const std::shared_ptr<class CProgramLibrary>& pLibrary) { std::unique_lock Lock(m_Mutex); m_Libraries.erase(pLibrary); }
	std::set<std::shared_ptr<class CProgramLibrary>> GetLibraries() const { std::shared_lock Lock(m_Mutex); return m_Libraries; }

	std::shared_ptr<class CProgramFile> GetProgram() const { std::shared_lock Lock(m_Mutex); return m_pFileRef.lock(); }

	void AddHandle(const CHandlePtr& pHandle);
	void RemoveHandle(const CHandlePtr& pHandle);
	std::set<CHandlePtr> GetHandleList() const { std::shared_lock Lock(m_HandleMutex); return m_HandleList;  }
	int GetHandleCount() const { std::shared_lock Lock(m_HandleMutex); return (int)m_HandleList.size(); }

	void AddSocket(const CSocketPtr& pSocket);
	void RemoveSocket(const CSocketPtr& pSocket, bool bNoCommit = false);
	std::set<CSocketPtr> GetSocketList() const { std::shared_lock Lock(m_SocketMutex); return m_SocketList;  }
	int GetSocketCount() const { std::shared_lock Lock(m_SocketMutex); return (int)m_SocketList.size(); }

	CDnsProcLog* DnsLog()					{ return &m_DnsLog; }

	void UpdateLastNetActivity(uint64 TimeStamp) { std::unique_lock Lock(m_Mutex); if(TimeStamp > m_LastNetActivity) m_LastNetActivity = TimeStamp; }
	uint64	GetLastNetActivity() const		{ std::shared_lock Lock(m_Mutex); return m_LastNetActivity;}

	uint64	GetUpload() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.SendRate.Get();}
	uint64	GetDownload() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.ReceiveRate.Get();}
	uint64	GetUploaded() const				{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.SendRaw;}
	uint64	GetDownloaded() const			{ std::shared_lock StatsLock(m_StatsMutex); return m_Stats.Net.ReceiveRaw;}

	void	AddNetworkIO(int Type, uint32 TransferSize);

	StVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;

	static const wchar_t* NtOsKernel_exe; 

protected:
	friend class CProcessList;
	friend class CServiceList;
	friend class CProgramManager;

	void AddService(const std::wstring& Key) { std::unique_lock Lock(m_Mutex); m_ServiceList.insert(Key); }
	void RemoveService(const std::wstring& Key) { std::unique_lock Lock(m_Mutex); m_ServiceList.erase(Key); }

	bool Init(const struct SProcessInfo* Data);
	bool Init(PSYSTEM_PROCESS_INFORMATION process, bool bFullProcessInfo);
	bool Update(PSYSTEM_PROCESS_INFORMATION process, bool bFullProcessInfo);
	void Update();
	void Update(const struct SProcessInfo* Data);

	void SetRawCreationTime(uint64 TimeStamp);
	bool InitOther();
	bool InitLibs();

	uint64 m_Pid = -1;
	uint64 m_ParentPid = -1;
	uint64 m_CreatorPid = -1;
	uint64 m_CreationTime = 0;
	//uint64 m_SeqNr = -1;
	//uint64 m_ParentSeqNr = -1;
	std::wstring m_Name;
	std::wstring m_NtFilePath;
	std::wstring m_CommandLine;

	CFlexGuid m_EnclaveGuid;
	uint32 m_SecState = 0;
	uint32 m_Flags = 0;
	uint32 m_SecFlags = 0;
	CImageSignInfo m_SignInfo;
	uint32 m_NumberOfImageLoads = 0;
	uint32 m_NumberOfMicrosoftImageLoads = 0;
	uint32 m_NumberOfAntimalwareImageLoads = 0;
	uint32 m_NumberOfVerifiedImageLoads = 0;
	uint32 m_NumberOfSignedImageLoads = 0;
	uint32 m_NumberOfUntrustedImageLoads = 0;
	std::set<std::shared_ptr<class CProgramLibrary>> m_Libraries;

	std::set<std::wstring>		m_ServiceList;

	std::wstring m_AppContainerSid;
	std::wstring m_AppContainerName;
	std::wstring m_AppPackageName;

	std::wstring m_UserSid;

	mutable std::shared_mutex	m_HandleMutex;
	std::set<CHandlePtr>		m_HandleList;

	mutable std::shared_mutex	m_SocketMutex;
	std::set<CSocketPtr>		m_SocketList;

	CDnsProcLog					m_DnsLog;

	//
	// Note: For windows services the m_pFileRef is NULL as one process svchost.exe 
	// may contain multiple services, when handling services always use thair SvcId
	// and reffer to the service man in the process manager
	//

	std::weak_ptr<class CProgramFile> m_pFileRef; 

	uint64						m_LastNetActivity = 0;
	// I/O stats
	mutable std::shared_mutex	m_StatsMutex;
	SProcStats					m_Stats;
};

typedef std::shared_ptr<CProcess> CProcessPtr;