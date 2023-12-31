#pragma once
#include "../Library/Status.h"
#include "../Library/Common/Buffer.h"
#include "../Library/Helpers/ConfigIni.h"

class CServiceCore
{
public:
	static STATUS Startup(bool bEngineMode = false);
	static void Shutdown();

	std::wstring			GetDataFolder() const	{ return m_DataFolder + L"\\"; }
	CConfigIni*				Config()				{ return m_pConfig; }

	class CEventLogger*		Log()					{ return m_pLog; }

	class CPipeServer*		UserPipe()				{ return m_pUserPipe; }
	class CAlpcPortServer*	UserPort()				{ return m_pUserPort; }

	class CProgramManager*	ProgramManager()		{ return m_pProgramManager; }
	class CProcessList*		ProcessList()			{ return m_pProcessList; }

	class CAppIsolator*		AppIsolator()			{ return m_pAppIsolator; }
	class CNetIsolator*		NetIsolator()			{ return m_pNetIsolator; }
	class CFSIsolator*		FSIsolator()			{ return m_pFSIsolator; }
	class CRegIsolator*		RegIsolator()			{ return m_pRegIsolator; }

	class CTweakManager*	TweakManager()			{ return m_pTweakManager; }

	class CDriverAPI*		Driver()				{ return m_Driver; }

	class CEtwEventMonitor*	EtwEventMonitor()		{ return m_pEtwEventMonitor; }

protected:
	friend DWORD CALLBACK CServiceCore__ThreadProc(LPVOID lpThreadParameter);
	friend VOID CALLBACK CServiceCore__TimerProc(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue);

	CServiceCore();
	~CServiceCore();

	STATUS Init();

	void OnTimer();

	void RegisterUserAPI();
	uint32 OnRequest(uint32 msgId, const CBuffer* req, CBuffer* rpl, uint32 PID, uint32 TID);

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

	class CAppIsolator*		m_pAppIsolator = NULL;
	class CNetIsolator*		m_pNetIsolator = NULL;
	class CFSIsolator*		m_pFSIsolator = NULL;
	class CRegIsolator*		m_pRegIsolator = NULL;

	class CTweakManager*	m_pTweakManager = NULL;

	class CDriverAPI*		m_Driver = NULL;

	class CEtwEventMonitor*	m_pEtwEventMonitor = NULL;

	bool					m_bEngineMode = false;
};

extern CServiceCore* svcCore;