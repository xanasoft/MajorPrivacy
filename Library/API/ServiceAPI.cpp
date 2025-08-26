#include "pch.h"
#include "ServiceAPI.h"

/*#include <ntstatus.h>
#define WIN32_NO_STATUS
typedef long NTSTATUS;
#include <windows.h>
#include <winternl.h>*/

#include <phnt_windows.h>
#include <phnt.h>

#include <ph.h>

#include "../Helpers/Service.h"
#include "../Helpers/NtIO.h"
#include <fltuser.h>

#include "../Helpers/AppUtil.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Common/StVariant.h"

#include "../IPC/PipeClient.h"
#include "../IPC/AlpcPortClient.h"

//#define USE_ALPC

void CServiceAPI__EmitEvent(CServiceAPI* This, const CBuffer* pEvent)
{
	MSG_HEADER* in_msg = (MSG_HEADER*)pEvent->ReadData(sizeof(MSG_HEADER));

	std::unique_lock<std::mutex> Lock(This->m_EventHandlersMutex);

    auto I = This->m_EventHandlers.find(in_msg->MessageId);
    if (I != This->m_EventHandlers.end())
		I->second(in_msg->MessageId, pEvent);
}

static VOID NTAPI CServiceAPI__Callback(const CBuffer& buff, PVOID Param)
{
	CServiceAPI__EmitEvent((CServiceAPI*)Param, &buff);
}

CServiceAPI::CServiceAPI()
{
#ifdef USE_ALPC
	m_pClient = new CAlpcPortClient();
#else
	m_pClient = new CPipeClient(CServiceAPI__Callback, this);
#endif
}

CServiceAPI::~CServiceAPI()
{
	if(m_hEngineProcess)
		CloseHandle(m_hEngineProcess);
	delete m_pClient;
}

STATUS CServiceAPI::InstallSvc(bool bAutoStart)
{
	std::wstring FileName = GetApplicationDirectory() + L"\\" API_SERVICE_BINARY;
	std::wstring DisplayName = L"MajorPrivacy System Service";

	// The Windows Firewall must be started first
	const wchar_t* Dependencies = L"MpsSvc\0\0";

	uint32 uOptions = OPT_OWN_TYPE;
	if(bAutoStart)
		uOptions |= OPT_AUTO_START;
	else
		uOptions |= OPT_DEMAND_START;

	return InstallService(API_SERVICE_NAME, FileName.c_str(), DisplayName.c_str(), NULL, Dependencies, uOptions);
}

STATUS CServiceAPI::ConnectSvc()
{
    if (m_pClient->IsConnected())
		m_pClient->Disconnect();

	STATUS Status;
    SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
	if ((SvcState & SVC_INSTALLED) != SVC_INSTALLED) {
		return ERR(STATUS_DEVICE_NOT_READY);
		//Status = InstallSvc();
		//if (!Status) return Status;
	}
    if ((SvcState & SVC_RUNNING) != SVC_RUNNING)
        Status = RunService(API_SERVICE_NAME);
    if (!Status)
        return Status;

#ifdef USE_ALPC
	Status = m_pClient->Connect(API_SERVICE_PORT);
#else
	Status = m_pClient->Connect(API_SERVICE_PIPE);
#endif

	return Status;
}

STATUS CServiceAPI::ConnectEngine()
{
	if(m_pClient->IsConnected())
		m_pClient->Disconnect();

#ifdef USE_ALPC
	STATUS Status = m_pClient->Connect(API_SERVICE_PORT);
#else
	STATUS Status = m_pClient->Connect(API_SERVICE_PIPE);
#endif
	if (Status)
		return Status;
	
	if (m_hEngineProcess) {
		DWORD exitCode;
		GetExitCodeProcess(m_hEngineProcess, &exitCode);
		if (exitCode != STILL_ACTIVE) {
			CloseHandle(m_hEngineProcess);
			m_hEngineProcess = NULL;
		}
	}

	if (!m_hEngineProcess) 
	{
		/*if (IsRunningElevated())
		{
			std::wstring Command = L"\"" + GetApplicationDirectory() + L"\\" API_SERVICE_BINARY L"\" -engine";

			STARTUPINFOW si = { sizeof(si) };
			PROCESS_INFORMATION pi = { 0 };
			if (CreateProcessW(NULL, (WCHAR*)Command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
				m_hEngineProcess = pi.hProcess;
				CloseHandle(pi.hThread);
			} else
				return ERR(PhGetLastWin32ErrorAsNtStatus());
		}
		else*/
		{
			std::wstring Path = GetApplicationDirectory() + L"\\" API_SERVICE_BINARY;

			m_hEngineProcess = RunElevated(Path, L"-engine");
			if (!m_hEngineProcess)
				return ERR(PhGetLastWin32ErrorAsNtStatus());
		}
	}


#ifdef _DEBUG
	for(int i = 1;;)
#else
	for (int i = 1; i < 10; i++)
#endif
	{
		DWORD exitCode;
		GetExitCodeProcess(m_hEngineProcess, &exitCode);
		if (exitCode != STILL_ACTIVE) {
			Status = ERR(PhDosErrorToNtStatus(exitCode));
			break; // engine failed to start
		}

#ifdef USE_ALPC
		Status = m_pClient->Connect(API_SERVICE_PORT);
#else
		Status = m_pClient->Connect(API_SERVICE_PIPE);
#endif
		if (Status)
			return OK;
		Sleep(1000 * i);
	}

	TerminateProcess(m_hEngineProcess, -1);
	CloseHandle(m_hEngineProcess);
	m_hEngineProcess = NULL;
	return ERR(STATUS_INVALID_SYSTEM_SERVICE);
}

void CServiceAPI::Disconnect()
{
	m_pClient->Disconnect();
}

bool CServiceAPI::IsConnected()
{
	return m_pClient->IsConnected();
}

uint32 CServiceAPI::GetProcessId() const
{
	return m_pClient->GetServerPID();
}

RESULT(StVariant) CServiceAPI::Call(uint32 MessageId, const StVariant& Message, SCallParams* pParams)
{
	std::unique_lock Lock(m_CallMutex);
	auto Ret = m_pClient->Call(MessageId, Message, pParams);
	auto& Val = Ret.GetValue();
	if (!Ret.IsError() && (Val.Get(API_V_ERR_CODE).To<uint32>() != 0 || Val.Has(API_V_ERR_MSG)))
		return ERR(Val[API_V_ERR_CODE], Val[API_V_ERR_MSG].AsStr());
	return Ret;
}

bool CServiceAPI::RegisterEventHandler(uint32 MessageId, const std::function<void(uint32 msgId, const CBuffer* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m_EventHandlersMutex);
	return m_EventHandlers.insert(std::make_pair(MessageId, Handler)).second; 
}

uint32 CServiceAPI::GetABIVersion()
{
	StVariant Request;
	auto Ret = Call(SVC_API_GET_VERSION, Request);
	StVariant Response = Ret.GetValue();
	uint32 version = Response.Get(API_V_VERSION).To<uint32>();
	return version;
}

uint32 CServiceAPI::GetConfigStatus()
{
	StVariant ReqVar;

    auto Ret = Call(SVC_API_GET_CONFIG_STATUS, ReqVar, NULL);
    if (Ret.IsError())
        return false;

	StVariant ResVar = Ret.GetValue();

    return ResVar.To<uint32>();
}

STATUS CServiceAPI::StoreConfigChanges()
{
	StVariant ReqVar;
    return Call(SVC_API_COMMIT_CONFIG, ReqVar, NULL);
}

STATUS CServiceAPI::DiscardConfigChanges()
{
	StVariant ReqVar;

	return Call(SVC_API_DISCARD_CHANGES, ReqVar, NULL);
}