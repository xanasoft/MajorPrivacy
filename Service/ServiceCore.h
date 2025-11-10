#pragma once
#include "../Library/Status.h"
#include "../Framework/Common/Buffer.h"
#include "../Library/Common/StVariant.h"
#include "../Library/Helpers/ConfigIni.h"
#include "../Library/Helpers/EvtUtil.h"
#include "../Library/Common/ThreadPool.h"
#include "../Framework/Core/Memory.h"
#include "../Library/API/PrivacyDefs.h"
#include "../../Framework/Core/MemoryPool.h"
#include "JSEngine/JSEngine.h"
#include "../Library/Common/FlexGuid.h"

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

	class CPipeServer*		UserPipe()				{ return m_pUserPipe; }
	class CAlpcPortServer*	UserPort()				{ return m_pUserPort; }

	class CEnclaveManager*	EnclaveManager()		{ return m_pEnclaveManager; }

	class CHashDB*			HashDB()				{ return m_pHashDB; }

	class CProgramManager*	ProgramManager()		{ return m_pProgramManager; }

	class CProcessList*		ProcessList()			{ return m_pProcessList; }

	class CAccessManager*	AccessManager()			{ return m_pAccessManager; }

	class CNetworkManager*	NetworkManager()		{ return m_pNetworkManager; }

	class CVolumeManager*	VolumeManager()			{ return m_pVolumeManager; }

	class CTweakManager*	TweakManager()			{ return m_pTweakManager; }

	class CPresetManager*	PresetManager()			{ return m_pPresetManager; }

	class CDriverAPI*		Driver()				{ return m_pDriver; }

	class CEtwEventMonitor*	EtwEventMonitor()		{ return m_pEtwEventMonitor; }

	CJSEnginePtr			GetScript(const CFlexGuid& Guid, EItemType Type);

	int						BroadcastMessage(uint32 MessageID, const StVariant& MessageData, const std::shared_ptr<class CProgramFile>& pProgram = NULL);

	HANDLE					GetThreadHandle() const { return m_hThread; }

	static std::wstring		NormalizePath(std::wstring FilePath, bool bLowerCase = true);

	static void				ImpersonateCaller(uint32 CallerPID);
	static std::wstring		GetCallerSID(uint32 CallerPID);

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

	STATUS InitDriver();
	void CloseDriver();

	STATUS PrepareShutdown();

	void StoreRecords(bool bAsync = false);

	void OnTimer();

	void RegisterUserAPI();
	void OnClient(uint32 uEvent, struct SPipeClientInfo& pClient);
	uint32 OnRequest(uint32 msgId, const CBuffer* req, CBuffer* rpl, const struct SPipeClientInfo& pClient);

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

	class CPipeServer*		m_pUserPipe = NULL;
	class CAlpcPortServer*	m_pUserPort = NULL;
	
	class CEnclaveManager*	m_pEnclaveManager = NULL;

	class CHashDB*			m_pHashDB = NULL;

	class CProgramManager*	m_pProgramManager = NULL;

	class CProcessList*		m_pProcessList = NULL;

	class CAccessManager*	m_pAccessManager = NULL;

	class CNetworkManager*	m_pNetworkManager = NULL;

	class CVolumeManager*	m_pVolumeManager = NULL;

	class CTweakManager*	m_pTweakManager = NULL;

	class CPresetManager*	m_pPresetManager = NULL;

	class CDriverAPI*		m_pDriver = NULL;

	class CEtwEventMonitor*	m_pEtwEventMonitor = NULL;

	bool					m_bEngineMode = false;


	struct SClient
	{
		bool bIsTrusted = false;

		mutable std::shared_mutex Mutex;

		std::set<uint64> WatchedPrograms;
		bool bWatchAllPrograms = false;

		//uint64 LibraryCacheToken = 0;
		//std::map<uint64, uint64> LibraryCache;
	};
	typedef std::shared_ptr<SClient> SClientPtr;

	mutable std::recursive_mutex m_ClientsMutex;
	std::map<uint32, SClientPtr> m_Clients;

	uint64 m_LastStoreTime = 0;
	uint64 m_LastCheckTime = 0;
	uint64 m_NextLogTime = 0;


	CThreadPool				m_Pool;
};

extern CServiceCore* theCore;

extern FW::DefaultMemPool g_DefaultMemPool;