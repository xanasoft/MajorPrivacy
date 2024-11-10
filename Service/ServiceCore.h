#pragma once
#include "../Library/Status.h"
#include "../Library/Common/Buffer.h"
#include "../Library/Common/Variant.h"
#include "../Library/Helpers/ConfigIni.h"
#include "../Library/Helpers/EvtUtil.h"


class CServiceCore
{
	TRACK_OBJECT(CServiceCore)
public:
	static STATUS Startup(bool bEngineMode = false);
	static void Shutdown();

	std::wstring			GetAppDir() const		{ return m_AppDir; }
	std::wstring			GetDataFolder() const	{ return m_DataFolder; }
	CConfigIni*				Config()				{ return m_pConfig; }
	void					Reconfigure(const std::string& Key);

	class CEventLogger*		Log()					{ return m_pLog; }

	class CPipeServer*		UserPipe()				{ return m_pUserPipe; }
	class CAlpcPortServer*	UserPort()				{ return m_pUserPort; }

	class CProgramManager*	ProgramManager()		{ return m_pProgramManager; }
	class CProcessList*		ProcessList()			{ return m_pProcessList; }

	class CAccessManager*	AccessManager()			{ return m_pAccessManager; }

	class CNetworkManager*	NetworkManager()		{ return m_pNetworkManager; }

	class CVolumeManager*	VolumeManager()			{ return m_pVolumeManager; }

	class CTweakManager*	TweakManager()			{ return m_pTweakManager; }

	class CDriverAPI*		Driver()				{ return m_pDriver; }

	class CEtwEventMonitor*	EtwEventMonitor()		{ return m_pEtwEventMonitor; }

	void					BroadcastMessage(uint32 MessageID, const CVariant& MessageData, const std::shared_ptr<class CProgramFile>& pProgram = NULL);

protected:
	friend DWORD CALLBACK CServiceCore__ThreadProc(LPVOID lpThreadParameter);
	friend VOID CALLBACK CServiceCore__TimerProc(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue);

	CServiceCore();
	~CServiceCore();

	STATUS Init();

	void OnTimer();

	void RegisterUserAPI();
	void OnClient(uint32 uEvent, struct SPipeClientInfo& pClient);
	uint32 OnRequest(uint32 msgId, const CBuffer* req, CBuffer* rpl, const struct SPipeClientInfo& pClient);

	std::wstring			m_AppDir;
	std::wstring			m_DataFolder;
	CConfigIni*				m_pConfig = NULL;

	HANDLE					m_hThread = NULL;
    HANDLE					m_hTimer = NULL;
	bool					m_Terminate = false;
	STATUS					m_InitStatus;
	
	class CEventLogger*		m_pLog = NULL;

	class CPipeServer*		m_pUserPipe = NULL;
	class CAlpcPortServer*	m_pUserPort = NULL;
	
	class CProgramManager*	m_pProgramManager = NULL;
	class CProcessList*		m_pProcessList = NULL;

	class CAccessManager*	m_pAccessManager = NULL;

	class CNetworkManager*	m_pNetworkManager = NULL;

	class CVolumeManager*	m_pVolumeManager = NULL;

	class CTweakManager*	m_pTweakManager = NULL;

	class CDriverAPI*		m_pDriver = NULL;

	class CEtwEventMonitor*	m_pEtwEventMonitor = NULL;

	bool					m_bEngineMode = false;


	struct SClient
	{
		mutable std::shared_mutex Mutex;

		std::set<uint64> WatchedPrograms;
		bool bWatchAllPrograms = false;

		//uint64 LibraryCacheToken = 0;
		//std::map<uint64, uint64> LibraryCache;
	};
	typedef std::shared_ptr<SClient> SClientPtr;

	mutable std::recursive_mutex m_ClientsMutex;
	std::map<uint32, SClientPtr> m_Clients;
};

extern CServiceCore* theCore;