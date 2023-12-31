#include "pch.h"
#include "DriverAPI.h"

/*#include <ntstatus.h>
#define WIN32_NO_STATUS
typedef long NTSTATUS;
#include <windows.h>
#include <winternl.h>*/

#include <phnt_windows.h>
#include <phnt.h>

#include "../Helpers/NtIO.h"
#include "../../Driver/DriverApi.h"
#include "../../Library/Helpers/Scoped.h"
#include "../Helpers/AppUtil.h"
#include "../Helpers/Service.h"
#include "../Common/FileIO.h"
#include "FltPortClient.h"
#include "DeviceClient.h"

extern "C" {
#include "../../Driver/KSI/include/kphmsg.h"
}

sint32 CDriverAPI__EmitProcess(CDriverAPI* This, const SProcessEvent* pEvent)
{
	std::unique_lock<std::mutex> Lock(This->m_ProcessHandlersMutex);
	
    NTSTATUS status = STATUS_SUCCESS;
    for (auto Handler : This->m_ProcessHandlers)
		status = Handler(pEvent);
    return status;
}

extern "C" static VOID NTAPI CDriverAPI__Callback(
    _In_ struct _KPH_MESSAGE* Message, 
    _In_ PFLTMGR_CALLBACK_CONTEXT Context, 
    _In_ FLTMGR_REPLYFUNC ReplyFunc
    )
{
    CDriverAPI* This = (CDriverAPI*)Context->Param;

	switch (Message->Header.MessageId)
	{
		case KphMsgProcessCreate:
		{
            CBuffer InBuff(Message->Data, Message->Header.Size - sizeof(Message->Header), true);
            CVariant vMessage;
            vMessage.FromPacket(&InBuff, true);

            SProcessEvent Event;
            Event.Type = SProcessEvent::EType::Started;
            Event.ProcessId = vMessage[(uint32)'pid'];
            Event.CreateTime = vMessage[(uint64)'ct'];
            Event.ParentId = vMessage[(uint32)'ppid'];
            Event.FileName = vMessage.Get((uint32)'img');
            Event.CommandLine = vMessage.Get((uint32)'cmd');

            NTSTATUS status = CDriverAPI__EmitProcess(This, &Event);

            CVariant vReply;
            vReply[(uint32)'cs'] = (sint32)status;

            CBuffer OutBuff;
	        OutBuff.SetData(NULL, sizeof(MSG_HEADER)); // make room for header, pointer points after the header
            PMSG_HEADER repHeader = (PMSG_HEADER)OutBuff.GetBuffer();
            repHeader->MessageId = Message->Header.MessageId;
            repHeader->Size = sizeof(MSG_HEADER);
            vReply.ToPacket(&OutBuff);
            PKPH_MESSAGE msg = (PKPH_MESSAGE)OutBuff.GetBuffer();
            msg->Header.Size = (ULONG)OutBuff.GetSize();

			ReplyFunc(Context, msg);

			break;
		}
        case KphMsgProcessExit:
        {
            CBuffer InBuff(Message->Data, Message->Header.Size - sizeof(Message->Header), true);
            CVariant vMessage;
            vMessage.FromPacket(&InBuff, true);

            SProcessEvent Event;
            Event.Type = SProcessEvent::EType::Stopped;
            Event.ProcessId = vMessage[(uint32)'pid'];
            Event.ParentId = 0;
            Event.ExitCode = vMessage[(uint32)'esc'];

            CDriverAPI__EmitProcess(This, &Event);

            break;
        }
	}
}

CDriverAPI::CDriverAPI()
{
    m_pClient = new CFltPortClient(CDriverAPI__Callback, this);
    //m_pClient = new CDeviceClient;
}

CDriverAPI::~CDriverAPI()
{
    delete m_pClient;
}

STATUS CDriverAPI::ConnectDrv()
{
    STATUS Status;

    if (m_pClient->IsConnected())
		return OK;

    std::wstring Name = API_DRIVER_NAME;
    std::wstring DisplayName = L"MajorPrivacy Kernel Driver";

    SVC_STATE SvcState = GetServiceState(Name.c_str());
    if ((SvcState & SVC_INSTALLED) != SVC_INSTALLED)
    {
	    std::wstring FileName = GetApplicationDirectory() + L"\\" API_DRIVER_BINARY;

        uint32 TraceLogLevel = 6;

        CVariant Params;
        Params["Altitude"] = API_DRIVER_ALTITUDE;
        Params["TraceLevel"] = TraceLogLevel;
        Params["PortName"] = API_DRIVER_PORT;
        CBuffer PubKey;
        if(ReadFile(GetApplicationDirectory() + L"\\publickey.dat", 0, PubKey))
            Params["UserPublicKey"] = PubKey;

        Status = InstallService(Name.c_str(), FileName.c_str(), DisplayName.c_str(), NULL, OPT_KERNEL_TYPE | OPT_DEMAND_START | OPT_START_NOW, Params);
    }
    else if ((SvcState & SVC_RUNNING) != SVC_RUNNING)
        Status = RunService(Name.c_str());

    // FltMgr API

    //Status = m_pClient->Connect(API_DEVICE_NAME);
    Status = m_pClient->Connect(API_DRIVER_PORT);

    //

    return Status;
}

bool CDriverAPI::IsDrvConnected()
{
    return m_pClient->IsConnected();
}

void CDriverAPI::DisconnectDrv()
{
    m_pClient->Disconnect();
}

RESULT(std::shared_ptr<std::vector<uint64>>) CDriverAPI::EnumProcesses()
{
    CVariant ReqVar;
    ReqVar[(uint32)'all'] = 1;

	auto Ret = m_pClient->Call(API_ENUM_PROCESSES, ReqVar);
    if (Ret.IsError())
        return Ret;

	CVariant ResVar = Ret.GetValue();
    CVariant vList = ResVar.Get('pids');

    auto pids = std::make_shared<std::vector<uint64>>();

    pids->reserve(vList.Count());
    for(uint32 i=0; i < vList.Count(); i++)
        pids->push_back(vList.At(i));

    RETURN(pids);
}

RESULT(SProcessInfoPtr) CDriverAPI::GetProcessInfo(uint64 pid)
{
    CVariant ReqVar;
    ReqVar[(uint32)'pid'] = pid;

	auto Ret = m_pClient->Call(API_GET_PROCESS_INFO, ReqVar);
    if (Ret.IsError())
        return Ret;

	CVariant ResVar = Ret.GetValue();
    
    SProcessInfoPtr Info = SProcessInfoPtr(new SProcessInfo());

    Info->ImageName = ResVar.Get('name').AsStr();
    Info->FileName = ResVar.Get('fn').AsStr();
    ASSERT(!Info->FileName.empty() || pid == 4); // todo: test
    Info->CreateTime = ResVar.Get('ct').To<uint64>();
    Info->ParentPid = ResVar.Get('ppid').To<uint64>();

    Info->FileHash = ResVar.Get('hash').AsBytes();
    Info->EnclaveId = ResVar.Get('eid', 0).To<uint32>();

    RETURN(Info);
}

STATUS CDriverAPI::StartProcessInEnvlave(const std::wstring& path, uint32 EnclaveId)
{
    CVariant ReqVar;
    ReqVar[(uint32)'eid'] = EnclaveId;

	auto Ret = m_pClient->Call(API_PREPARE_ENCLAVE, ReqVar);
    if (Ret.IsError())
        return Ret;

	CVariant ResVar = Ret.GetValue();

    EnclaveId = ResVar[(uint32)'eid'];

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = { 0 };
    CreateProcessW(NULL, (wchar_t*)path.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    
    return OK;
}

STATUS CDriverAPI::RegisterForProcesses(bool bRegister)
{
    CVariant ReqVar;
    if(bRegister)
        ReqVar[(uint32)'snbm'] = KphMsgProcessCreate | KphMsgProcessExit;
    else
        ReqVar[(uint32)'cnbm'] = KphMsgProcessCreate | KphMsgProcessExit;
	return m_pClient->Call(API_REGISTER_FOR_EVENT, ReqVar);
}

void CDriverAPI::RegisterProcessHandler(const std::function<sint32(const SProcessEvent* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m_ProcessHandlersMutex);
	m_ProcessHandlers.push_back(Handler); 
}

void CDriverAPI::TestDrv()
{
    CVariant ReqVar;
    ReqVar[(uint32)'test'] = 1234;
	auto Ret = m_pClient->Call(0x1234, ReqVar);
	CVariant ResVar = Ret.GetValue();
	std::wstring test = ResVar["TEST"];
}