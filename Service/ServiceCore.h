#pragma once
#include "../Library/Status.h"
#include "../Framework/Common/Buffer.h"
#include "../Library/Common/StVariant.h"
#include "../Library/Helpers/ConfigIni.h"
#include "../Library/Helpers/EvtUtil.h"
#include "../Library/Common/ThreadPool.h"
#include "../Framework/Core/Memory.h"
#include "../Library/API/PrivacyDefs.h"
#include "Common/ListReplies.h"
#include "../../Framework/Core/MemoryPool.h"
#include "JSEngine/JSEngine.h"
#include "JSEngine/JSStateManager.h"
#include "../Library/Common/FlexGuid.h"
#include "../Library/IPC/PipeServer.h"
#include "../Library/IPC/AlpcPortServer.h"
#include "../Library/Crypto/KeyExchange.h"

#define DEF_CORE_TIMER_INTERVAL		250

class CServiceCore
{
	TRACK_OBJECT(CServiceCore)
public:
	static STATUS Startup(bool bEngineMode = false);
	enum EShutdownMode
	{
		eShutdown_None = 0,
		eShutdown_Wait = 1,
		eShutdown_NoWait = 2,
		eShutdown_System = 3
	};
	static void Shutdown(EShutdownMode eMode);

	FW::AbstractMemPool*	Allocator() const		{ return m_pMemPool; }

	std::wstring			GetAppDir() const		{ return m_AppDir; }
	std::wstring			GetDataFolder() const	{ return m_DataFolder; }
	CConfigIni*				Config()				{ return m_pConfig; }
	void					Reconfigure(const std::string& Key);
	void					RefreshConfig(const std::string& Key);

	bool					IsConfigDirty() const	{ return m_bConfigDirty; }
	void					SetConfigDirty(bool bDirty = true) { m_bConfigDirty = bDirty; }
	STATUS					CommitConfig();
	STATUS					DiscardConfig();

	class CEventLogger*		Log()					{ return m_pSysLog; }
	void EmitEvent(ELogLevels Level, int Type, const StVariant& Data);

#ifdef USE_ALPC
	class CAlpcPortServer*	UserPort()				{ return m_pUserPort; }
#else
	class CPipeServer*		UserPipe()				{ return m_pUserPipe; }
#endif

	class CEnclaveManager*	EnclaveManager()		{ return m_pEnclaveManager; }

	class CHashDB*			HashDB()				{ return m_pHashDB; }

	class CProgramManager*	ProgramManager()		{ return m_pProgramManager; }

	class CProcessList*		ProcessList()			{ return m_pProcessList; }

	class CAccessManager*	AccessManager()			{ return m_pAccessManager; }

	class CNetworkManager*	NetworkManager()		{ return m_pNetworkManager; }

	class CVolumeManager*	VolumeManager()			{ return m_pVolumeManager; }

	class CTweakManager*	TweakManager()			{ return m_pTweakManager; }

	class CPresetManager*	PresetManager()			{ return m_pPresetManager; }

	class CJSStateManager*	JSStateManager()		{ return m_pJSStateManager; }

	class CDriverAPI*		Driver()				{ return m_pDriver; }

	class CEtwEventMonitor*	EtwEventMonitor()		{ return m_pEtwEventMonitor; }

	CJSEnginePtr			GetScript(const CFlexGuid& Guid, EItemType Type);

	int						BroadcastMessage(uint32 MessageID, const StVariant& MessageData, const std::shared_ptr<class CProgramFile>& pProgram = NULL);

	//
	// The same, but collected and sent together on the next turn of the timer.
	//
	// For the activity events, which arrive in the hundreds per second and which
	// nobody is waiting on an answer to. Anything the service needs a reply to must
	// still go through BroadcastMessage above.
	//
	void					QueueBroadcast(uint32 MessageID, const StVariant& MessageData, const std::shared_ptr<class CProgramFile>& pProgram = NULL);
	void					FlushBroadcasts();

	HANDLE					GetThreadHandle() const { return m_hThread; }

	static std::wstring		NormalizePath(std::wstring FilePath, bool bLowerCase = true);

	static void				ImpersonateCaller(uint32 CallerPID);
	static std::wstring		GetCallerSID(uint32 CallerPID);

	//
	// Whether the caller could have done it itself. The service runs as SYSTEM and
	// will open anything it is pointed at, so before it touches a file on someone
	// else's behalf it checks that the someone was entitled to it.
	//
	static STATUS			CheckCallerFileAccess(uint32 CallerPID, const std::wstring& Path, ULONG DesiredAccess);

	CThreadPool*			ThreadPool()			{ return &m_Pool; }

	enum EExecFlags
	{
		eExec_None = 0,
		eExec_Hidden = 0x0001,			// Run without showing window
		eExec_DropAdmin = 0x0002,		// Drop admin rights from token
		eExec_LowPrivilege = 0x0004,	// Run with restricted low privilege token
		eExec_AsCallerUser = 0x0008,	// Run as the calling user's token (default)
		eExec_Elevate = 0x0010,			// Elevate to admin rights (get linked token if caller is in admin group)
		eExec_AsSystem = 0x0020,		// Run as SYSTEM (requires service mode with appropriate privileges)
	};

	STATUS					CreateUserProcess(const std::wstring& CommandLine, uint32 CallerPID, uint32 Flags = eExec_AsCallerUser, const std::wstring& WorkingDir = L"", uint32* pProcessId = nullptr, HANDLE* pProcessHandle = nullptr);

	static STATUS InstallDriver(bool bAutoStart);
	static STATUS StopDriver();
	static STATUS RemoveDriver();

protected:
	friend DWORD CALLBACK CServiceCore__ThreadProc(LPVOID lpThreadParameter);
	friend VOID CALLBACK CServiceCore__TimerProc(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue);

	CServiceCore();
	~CServiceCore();

	STATUS Init();

	void ConfigInit();

	STATUS InitHooks();
	void RemoveHooks();

	STATUS InitDriver();
	void CloseDriver();

	STATUS PrepareShutdown();

	void StoreRecords(bool bAsync = false);

	void OnTimer();

	void OnDriverChanged(const std::wstring& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID);

	void RegisterUserAPI();
	void OnClient(uint32 uEvent, struct SClientInfo& pClient);
	uint32 OnRequest(uint32 msgId, const CBuffer* req, CBuffer* rpl, const struct SClientInfo& pClient);

	static void	DeviceChangedCallback(void* param);

	std::wstring			m_AppDir;
	std::wstring			m_DataFolder;
	CConfigIni*				m_pConfig = NULL;

	HANDLE					m_hThread = NULL;
    HANDLE					m_hTimer = NULL;
	EShutdownMode			m_Shutdown = eShutdown_None;
	STATUS					m_InitStatus;
	bool					m_bConfigDirty = false;	
	
	FW::MemoryPool*			m_pMemPool = NULL;

	class CEventLogger*		m_pSysLog = NULL;
	class CEventLog*		m_pEventLog = NULL;

#ifdef USE_ALPC
	class CAlpcPortServer*	m_pUserPort = NULL;
#else
	class CPipeServer*		m_pUserPipe = NULL;
#endif
	
	class CEnclaveManager*	m_pEnclaveManager = NULL;

	class CHashDB*			m_pHashDB = NULL;

	class CProgramManager*	m_pProgramManager = NULL;

	class CProcessList*		m_pProcessList = NULL;

	class CAccessManager*	m_pAccessManager = NULL;

	class CNetworkManager*	m_pNetworkManager = NULL;

	class CVolumeManager*	m_pVolumeManager = NULL;

	class CTweakManager*	m_pTweakManager = NULL;

	class CPresetManager*	m_pPresetManager = NULL;

	class CJSStateManager*	m_pJSStateManager = NULL;

	class CDriverAPI*		m_pDriver = NULL;

	class CEtwEventMonitor*	m_pEtwEventMonitor = NULL;

	bool					m_bEngineMode = false;


	struct SClient
	{
		//
		// A pool of this client's own, rather than the one the whole service shares.
		//
		// Two reasons. The shared pool takes a lock on every allocation, so building
		// a delta on the connection thread contends with every collector in the
		// service for as long as it takes. And what the caches hold is exactly as
		// long lived as the connection, so when the pipe goes it all goes at once,
		// without walking anything to free it.
		//
		SClient()
		{
			m_pMemPool = FW::MemoryPool::Create();
			Lists = new SListCaches(m_pMemPool);
		}

		~SClient()
		{
			//
			// In this order: the caches hold variants out of the pool.
			//
			delete Lists;
			FW::MemoryPool::Destroy(m_pMemPool);
		}
		bool bIsTrusted = false;

		//
		// Guards the watched program set below. Taken by BroadcastMessage for every
		// event that goes out, so it must never be held for long.
		//
		mutable std::shared_mutex Mutex;

		//
		// And a separate one for the caches, which a list request holds for as long
		// as it takes to build a delta - milliseconds. Sharing Mutex for that put
		// every driver event broadcast behind the refresh: it was more than half of
		// all the time the event threads spent waiting.
		//
		mutable std::mutex ListsMutex;

		std::set<uint64> WatchedPrograms;
		bool bWatchAllPrograms = false;

		//uint64 LibraryCacheToken = 0;
		//std::map<uint64, uint64> LibraryCache;

		//
		// What this client has already been sent. One per list, living and dying
		// with the connection - the connection is the subscription, so there is
		// nothing to expire and no token on the wire. Guarded by Mutex above.
		//
		FW::MemoryPool*	m_pMemPool = nullptr;
		SListCaches*	Lists = nullptr;

		CBuffer SharedSecret;
	};
	typedef std::shared_ptr<SClient> SClientPtr;

	mutable std::recursive_mutex m_ClientsMutex;
	std::map<uint64, SClientPtr> m_Clients;

	//
	// What QueueBroadcast has collected since the last flush. The program is kept
	// with each one because the filtering is per client, and which clients there
	// are is only known when it goes out.
	//
	struct SQueuedEvent
	{
		uint32						MessageID = 0;
		StVariant					Data;
		std::shared_ptr<class CProgramFile>	pProgram;
	};

	std::mutex				m_EventQueueMutex;
	std::vector<SQueuedEvent> m_EventQueue;

	//
	// Held for the whole of a flush so that two of them cannot interleave and
	// deliver the events out of order.
	//
	std::mutex				m_FlushMutex;

	uint64 m_LastStoreTime = 0;
	uint64 m_LastCheckTime = 0;
	uint64 m_NextLogTime = 0;


	CThreadPool				m_Pool;
};

extern CServiceCore* theCore;

extern FW::DefaultMemPool g_DefaultMemPool;