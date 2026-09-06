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
#include "../Library/Helpers/Scoped.h"
#include "../Common/StVariant.h"

#include "../IPC/PipeClient.h"
#include "../IPC/AlpcPortClient.h"
#include "../IPC/ServerReadyEvent.h"

void CServiceAPI__EmitEvent(CServiceAPI* This, const CBuffer* pEvent)
{
	MSG_HEADER* in_msg = (MSG_HEADER*)pEvent->ReadData(This->GetHeaderSize());

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
	m_pClient = new CAlpcPortClient(CServiceAPI__Callback, this);
#else
	m_pClient = new CPipeClient(CServiceAPI__Callback, this);
#endif
}

CServiceAPI::~CServiceAPI()
{
	delete m_pClient;
}

STATUS CServiceAPI::InstallSvc(bool bAutoStart)
{
	std::wstring FileName = GetApplicationDirectory() + L"\\" API_SERVICE_BINARY;
	std::wstring DisplayName = L"MajorPrivacy System Service";

	// The Kernel Isolator driver must be started first
	// The Windows Firewall must be started first
	//const wchar_t* Dependencies = API_DRIVER_NAME L"\0MpsSvc\0\0";
	const wchar_t* Dependencies = L"MpsSvc\0\0";

	uint32 uOptions = OPT_OWN_TYPE | OPT_ENABLE_RECOVERY;
	if(bAutoStart)
		uOptions |= OPT_AUTO_START;
	else
		uOptions |= OPT_DEMAND_START;

	return InstallService(API_SERVICE_NAME, FileName.c_str(), DisplayName.c_str(), NULL, Dependencies, uOptions);
}

STATUS CServiceAPI::ReConnect()
{
	if (m_pClient->IsConnected())
		m_pClient->Disconnect();

	return Connect();
}

STATUS CServiceAPI::ConnectSvc(bool bCanInstall)
{
	STATUS Status = ReConnect();
	if (Status)
		return Status;

    SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
	if ((SvcState & SVC_INSTALLED) != SVC_INSTALLED) {
		if(!bCanInstall)
			return ERR(STATUS_DEVICE_NOT_READY);
		Status = InstallSvc(false);
		if (!Status) return Status;
	}

	if ((SvcState & SVC_RUNNING) != SVC_RUNNING)
	{
		if ((SvcState & SVC_STARTING) != SVC_STARTING) 
		{
			Status = RunService(API_SERVICE_NAME);
			if (!Status)
				return Status;
		}

		// Wait for service to finish starting up
		for (int i = 0; i < 5 * 60; i++)
		{
			Sleep(1000);
			SvcState = GetServiceState(API_SERVICE_NAME);
			if ((SvcState & SVC_RUNNING) == SVC_RUNNING)
				break;
			if (SvcState != SVC_STARTING)
				return ERR(STATUS_UNSUCCESSFUL); // Service stopped or failed to start
		}
	}

	// Try connecting with short retry
	for (int i = 0; i < 5; i++)
	{
		Status = Connect();
		if (Status) break;
		Sleep(250);
	}
	return Status;
}

STATUS CServiceAPI::ConnectEngine(bool bCanStart)
{
	STATUS Status = ReConnect();
	if (Status)
		return Status;

	if(!bCanStart)
		return ERR(STATUS_INVALID_SYSTEM_SERVICE);

	// Create event before starting engine so we can wait for it to be ready
	// Uses Local\ namespace since engine runs in the same session as client
	CServerReadyEvent ReadyEvent;
	ReadyEvent.ClientCreate(API_WORKER_READY_EVENT);

	std::wstring Path = GetApplicationDirectory() + L"\\" API_SERVICE_BINARY;
	CScopedHandle hEngineProcess(INVALID_HANDLE_VALUE, CloseHandle);
	if (IsRunningElevated())
	{
		std::wstring Command = Path + L" -engine";

		STARTUPINFOW si = { sizeof(si) };
		PROCESS_INFORMATION pi = { 0 };
		if (CreateProcessW(NULL, (WCHAR*)Command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			hEngineProcess.Set(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}
	else
		hEngineProcess.Set(RunElevatedEx(Path, L"-engine"));
	if (!hEngineProcess)
		return ERR(PhGetLastWin32ErrorAsNtStatus());

	DWORD Ret = WaitForSingleObject(hEngineProcess, 5 * 60 * 1000);
	if (Ret == WAIT_TIMEOUT) { // user did not approve or deny UAC prompt 
		TerminateProcess(hEngineProcess, -1);
		return ERR(STATUS_TIMEOUT);
	}
	if (Ret == WAIT_OBJECT_0) {
		GetExitCodeProcess(hEngineProcess, &Ret);
		if (Ret) {
			if (Ret == ERROR_CANCELLED)
				return ERR(STATUS_OK_CNCELED);
			if (Ret != STILL_ACTIVE)
				return ERR(STATUS_UNSUCCESSFUL); // failed to start process
		}
	}

	if (!ReadyEvent.ClientWait(5 * 60 * 1000))
		return ERR(STATUS_TIMEOUT);

	// Try connecting with short retry
	for (int i = 0; i < 5; i++)
	{
		Status = Connect();
		if (Status) break;
		Sleep(250);
	}
	return Status;
}

STATUS CServiceAPI::Connect()
{
#ifdef USE_ALPC
	return m_pClient->Connect(API_SERVICE_PORT);
#else
	return m_pClient->Connect(API_SERVICE_PIPE);
#endif
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

STATUS CServiceAPI::NegotiateKey()
{
	CKeyExchange KeyExchange;
	NTSTATUS status = KeyExchange.GenerateKeyPair();
	if (!NT_SUCCESS(status))
		return ERR(status);

	CBuffer pubKey;
	status = KeyExchange.GetPublicKey(pubKey);
	if (!NT_SUCCESS(status))
		return ERR(status);

	StVariant ReqVar;
	ReqVar[API_V_PUB_KEY] = pubKey;

	auto Ret = Call(SVC_API_KEY_EXCHANGE, ReqVar, NULL);
	if (Ret.IsError())
		return Ret.GetStatus();

	StVariant ResVar = Ret.GetValue();
	CBuffer svcPubKey = ResVar[API_V_PUB_KEY].To<CBuffer>();
	if (svcPubKey.GetSize() == 0)
		return ERR(STATUS_INVALID_PARAMETER);

	status = KeyExchange.DeriveSharedSecret(svcPubKey, m_SharedSecret);
	if (!NT_SUCCESS(status))
		return ERR(status);

	return OK;
}

STATUS CServiceAPI::GetSharedSecret(CBuffer& SharedSecret) const
{
	if (m_SharedSecret.GetSize() == 0)
		return ERR(STATUS_DEVICE_NOT_READY);

	SharedSecret = m_SharedSecret;
	return OK;
}

ULONG CServiceAPI::GetHeaderSize() const
{
	return m_pClient->GetHeaderSize();
}