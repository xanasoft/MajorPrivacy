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
#include "../Helpers/Service.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Common/Variant.h"

#include "../IPC/PipeClient.h"
#include "../IPC/AlpcPortClient.h"

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
	//m_pClient = new CAlpcPortClient();
	m_pClient = new CPipeClient(CServiceAPI__Callback, this);
}

CServiceAPI::~CServiceAPI()
{
	if(hEngineProcess)
		CloseHandle(hEngineProcess);
	delete m_pClient;
}

STATUS CServiceAPI::InstallSvc()
{
	std::wstring FileName = GetApplicationDirectory() + L"\\" API_SERVICE_BINARY;
	std::wstring DisplayName = L"MajorPrivacy System Service";

	return InstallService(API_SERVICE_NAME, FileName.c_str(), DisplayName.c_str(), NULL, OPT_OWN_TYPE | OPT_DEMAND_START);
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

	//Status = m_pClient->Connect(API_SERVICE_PORT);
	Status = m_pClient->Connect(API_SERVICE_PIPE);

	return Status;
}

STATUS CServiceAPI::ConnectEngine()
{
	if(m_pClient->IsConnected())
		m_pClient->Disconnect();

	//STATUS Status = m_pClient->Connect(API_SERVICE_PORT);
	STATUS Status = m_pClient->Connect(API_SERVICE_PIPE);
	if (Status)
		return Status;
	
	if (hEngineProcess) {
		DWORD exitCode;
		GetExitCodeProcess(hEngineProcess, &exitCode);
		if (exitCode != STILL_ACTIVE) {
			CloseHandle(hEngineProcess);
			hEngineProcess = NULL;
		}
	}

	if (!hEngineProcess) 
	{
		/*if (IsRunningElevated())
		{
			std::wstring Command = L"\"" + GetApplicationDirectory() + L"\\" API_SERVICE_BINARY L"\" -engine";

			STARTUPINFOW si = { sizeof(si) };
			PROCESS_INFORMATION pi = { 0 };
			if (CreateProcessW(NULL, (WCHAR*)Command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
				hEngineProcess = pi.hProcess;
				CloseHandle(pi.hThread);
			} else
				return ERR(PhGetLastWin32ErrorAsNtStatus());
		}
		else*/
		{
			std::wstring Path = GetApplicationDirectory() + L"\\" API_SERVICE_BINARY;

			hEngineProcess = RunElevated(Path, L"-engine");
			if (!hEngineProcess)
				return ERR(PhGetLastWin32ErrorAsNtStatus());
		}
	}


#ifdef _DEBUG
	for(;;)
#else
	for (int i = 0; i < 10; i++)
#endif
	{
		DWORD exitCode;
		GetExitCodeProcess(hEngineProcess, &exitCode);
		if (exitCode != STILL_ACTIVE) {
			Status = ERR(PhDosErrorToNtStatus(exitCode));
			break; // engine failed to start
		}

		//Status = m_pClient->Connect(API_SERVICE_PORT);
		Status = m_pClient->Connect(API_SERVICE_PIPE);
		if (Status)
			return OK;
		Sleep(1000);
	}

	TerminateProcess(hEngineProcess, -1);
	CloseHandle(hEngineProcess);
	hEngineProcess = NULL;
	return ERR(STATUS_INVALID_SYSTEM_SERVICE);
}

//STATUS CServiceAPI::Reconnect()
//{
//	m_pClient->Disconnect();
//
//	return m_pClient->Connect(API_SERVICE_PIPE);
//}

void CServiceAPI::Disconnect()
{
	m_pClient->Disconnect();
}

RESULT(CVariant) CServiceAPI::Call(uint32 MessageId, const CVariant& Message)
{
	auto Ret = m_pClient->Call(MessageId, Message);
	if (!Ret.IsError() && (Ret.GetValue().Get(API_V_ERR_CODE).To<uint32>() != 0 || Ret.GetValue().Has(API_V_ERR_MSG)))
		return ERR(Ret.GetValue()[API_V_ERR_CODE], Ret.GetValue()[API_V_ERR_MSG].AsStr());
	return Ret;
}

bool CServiceAPI::RegisterEventHandler(uint32 MessageId, const std::function<void(uint32 msgId, const CBuffer* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m_EventHandlersMutex);
	return m_EventHandlers.insert(std::make_pair(MessageId, Handler)).second; 
}

uint32 CServiceAPI::GetABIVersion()
{
	CVariant Request;
	auto Ret = Call(SVC_API_GET_VERSION, Request);
	CVariant Response = Ret.GetValue();
	uint32 version = Response.Get(API_V_VERSION).To<uint32>();
	return version;
}

void CServiceAPI::TestSvc()
{

}