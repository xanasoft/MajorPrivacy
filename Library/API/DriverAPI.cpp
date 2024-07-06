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
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/Helpers/Scoped.h"
#include "../Helpers/AppUtil.h"
#include "../Helpers/NtUtil.h"
#include "../Helpers/Service.h"
#include "../Common/FileIO.h"
#include "../IPC/FltPortClient.h"
#include "../IPC/DeviceClient.h"

extern "C" {
#include "../../Driver/KSI/include/kphmsg.h"
}

uint32 RuleTypeToMsgType(ERuleType Type)
{
	switch (Type)
	{
    case ERuleType::eProgram: return KphMsgProgramRules;
    case ERuleType::eAccess: return KphMsgAccessRules;
	}
	return 0;
}

ERuleType MsgTypeToRuleType(uint32 Type)
{
	switch (Type)
	{
	case KphMsgProgramRules: return ERuleType::eProgram;
	case KphMsgAccessRules: return ERuleType::eAccess;
	}
	return ERuleType::eUnknown;
}

sint32 CDriverAPI__EmitProcess(CDriverAPI* This, const SProcessEvent* pEvent)
{
	std::unique_lock<std::mutex> Lock(This->m_HandlersMutex);
	
    NTSTATUS status = STATUS_SUCCESS;
    for (auto Handler : This->m_ProcessHandlers)
		status = Handler(pEvent);
    return status;
}

void CDriverAPI__EmitRuleEvent(CDriverAPI* This, ERuleType Type, const std::wstring& Guid, ERuleEvent Event)
{
    std::unique_lock<std::mutex> Lock(This->m_HandlersMutex);

    NTSTATUS status = STATUS_SUCCESS;
    This->m_RuleEventHandlers[Type](Guid, Event, Type);
}

extern "C" static VOID NTAPI CDriverAPI__Callback(
    _In_ struct _KPH_MESSAGE* Message, 
    _In_ PFLTMGR_CALLBACK_CONTEXT Context, 
    _In_ FLTMGR_REPLYFUNC ReplyFunc
    )
{
    CDriverAPI* This = (CDriverAPI*)Context->Param;

    CBuffer InBuff(Message->Data, Message->Header.Size - sizeof(Message->Header), true);
    CVariant vEvent;
    vEvent.FromPacket(&InBuff, true);

	switch (Message->Header.MessageId)
	{
		case KphMsgProcessCreate:
		{
            SProcessStartEvent Event;
            Event.Type = SProcessEvent::EType::ProcessStarted;
            Event.ActorProcessId = vEvent[API_V_EVENT_ACTOR_PID].To<uint64>();
            Event.ActorThreadId = vEvent[API_V_EVENT_ACTOR_TID].To<uint64>();
            uint32 ActorServiceTag = (uint32)vEvent.Get(API_V_EVENT_ACTOR_SVC).To<uint64>();
            if (ActorServiceTag) Event.ActorServiceTag = GetServiceNameFromTag((HANDLE)Event.ActorProcessId, ActorServiceTag);
            Event.ProcessId = vEvent[API_V_EVENT_PID].To<uint64>();
            Event.TimeStamp = vEvent[API_V_EVENT_TIME_STAMP].To<uint64>();
            Event.ParentId = vEvent[API_V_EVENT_PARENT_PID].To<uint64>();
            Event.FileName = vEvent.Get(API_V_EVENT_PATH);
            Event.CommandLine = vEvent.Get(API_V_EVENT_CMD);
            Event.CreationStatus = vEvent.Get(API_V_EVENT_CS);
            Event.Status = (EEventStatus)vEvent[API_V_EVENT_ACCESS_STATUS].To<uint32>();

            NTSTATUS status = CDriverAPI__EmitProcess(This, &Event);

            /*CVariant vReply;
            vReply[API_V_EVENT_CS] = (sint32)status;

            CBuffer OutBuff;
	        OutBuff.SetData(NULL, sizeof(MSG_HEADER)); // make room for header, pointer points after the header
            PMSG_HEADER repHeader = (PMSG_HEADER)OutBuff.GetBuffer();
            repHeader->MessageId = Message->Header.MessageId;
            repHeader->Size = sizeof(MSG_HEADER);
            vReply.ToPacket(&OutBuff);
            PKPH_MESSAGE msg = (PKPH_MESSAGE)OutBuff.GetBuffer();
            msg->Header.Size = (ULONG)OutBuff.GetSize();

			ReplyFunc(Context, msg);*/

			break;
		}
        case KphMsgProcessExit:
        {
            SProcessStopEvent Event;
            Event.Type = SProcessEvent::EType::ProcessStopped;
            Event.ProcessId = vEvent[API_V_EVENT_PID];
            Event.TimeStamp = vEvent[API_V_EVENT_TIME_STAMP].To<uint64>();
            //vEvent[API_V_EVENT_TID];
            Event.ExitCode = vEvent[API_V_EVENT_ECODE];

            CDriverAPI__EmitProcess(This, &Event);
            break;
        }
        case KphMsgImageLoad:
        {
            SProcessImageEvent Event;
            Event.Type = SProcessEvent::EType::ImageLoad;
            Event.ActorProcessId = vEvent[API_V_EVENT_ACTOR_PID].To<uint64>();
            Event.ActorThreadId = vEvent[API_V_EVENT_ACTOR_TID].To<uint64>();
            uint32 ActorServiceTag = (uint32)vEvent.Get(API_V_EVENT_ACTOR_SVC).To<uint64>();
            if (ActorServiceTag) Event.ActorServiceTag = GetServiceNameFromTag((HANDLE)Event.ActorProcessId, ActorServiceTag);
            Event.ProcessId = vEvent[API_V_EVENT_PID].To<uint64>();
            Event.TimeStamp = vEvent[API_V_EVENT_TIME_STAMP].To<uint64>();
            Event.FileName = vEvent[API_V_EVENT_PATH].AsStr();
            Event.ImageProperties = vEvent[API_V_EVENT_IMG_PROPS].To<uint32>();
            Event.ImageBase = vEvent[API_V_EVENT_IMG_BASE].To<uint64>();
            Event.SignAuthority = (KPH_VERIFY_AUTHORITY)vEvent[API_V_EVENT_IMG_SIGN_AUTH].To<uint32>();
            Event.SignLevel = vEvent[API_V_EVENT_IMG_SIGN_LEVEL].To<uint32>();
            Event.SignPolicy = vEvent[API_V_EVENT_IMG_SIGN_POLICY].To<uint32>();
            Event.ImageSelector = vEvent[API_V_EVENT_IMG_SEL].To<uint32>();
            Event.ImageSectionNumber = vEvent[API_V_EVENT_IMG_SECT_NR].To<uint32>();

            CDriverAPI__EmitProcess(This, &Event);
            break;
        }
        case KphMsgUntrustedImage:
        {
            SProcessImageEvent Event;
            Event.Type = SProcessEvent::EType::UntrustedLoad;
            Event.ActorProcessId = vEvent[API_V_EVENT_ACTOR_PID].To<uint64>();
            Event.ActorThreadId = vEvent[API_V_EVENT_ACTOR_TID].To<uint64>();
            uint32 ActorServiceTag = (uint32)vEvent.Get(API_V_EVENT_ACTOR_SVC).To<uint64>();
            if (ActorServiceTag) Event.ActorServiceTag = GetServiceNameFromTag((HANDLE)Event.ActorProcessId, ActorServiceTag);
            Event.ProcessId = vEvent[API_V_EVENT_PID].To<uint64>();
            Event.TimeStamp = vEvent[API_V_EVENT_TIME_STAMP].To<uint64>();
            Event.bLoadPrevented = vEvent[API_V_EVENT_WAS_LP].To<bool>();
            Event.FileName = vEvent[API_V_EVENT_PATH].AsStr();
            Event.ImageBase = vEvent[API_V_EVENT_IMG_BASE].To<uint64>();
            Event.SignAuthority = (KPH_VERIFY_AUTHORITY)vEvent[API_V_EVENT_IMG_SIGN_AUTH].To<uint32>();
            Event.SignLevel = vEvent[API_V_EVENT_IMG_SIGN_LEVEL].To<uint32>();
            Event.SignPolicy = vEvent[API_V_EVENT_IMG_SIGN_POLICY].To<uint32>();

            CDriverAPI__EmitProcess(This, &Event);
            break;
        }
        case KphMsgProcAccess:
        {
            SProcessAccessEvent Event;
            Event.Type = SProcessEvent::EType::ProcessAccess;
            Event.ActorProcessId = vEvent[API_V_EVENT_ACTOR_PID].To<uint64>();
            Event.ActorThreadId = vEvent[API_V_EVENT_ACTOR_TID].To<uint64>();
            uint32 ActorServiceTag = (uint32)vEvent.Get(API_V_EVENT_ACTOR_SVC).To<uint64>();
            if (ActorServiceTag) Event.ActorServiceTag = GetServiceNameFromTag((HANDLE)Event.ActorProcessId, ActorServiceTag);
            Event.ProcessId = vEvent[API_V_EVENT_PID].To<uint64>();
            Event.TimeStamp = vEvent[API_V_EVENT_TIME_STAMP].To<uint64>();
            Event.bThread = !vEvent[API_V_EVENT_IS_P].To<bool>();
            Event.AccessMask = vEvent[API_V_EVENT_ACCESS].To<uint32>();
            Event.Status = (EEventStatus)vEvent[API_V_EVENT_ACCESS_STATUS].To<uint32>();

            CDriverAPI__EmitProcess(This, &Event);
            break;
        }
        case KphMsgAccessFile:
        case KphMsgAccessReg:
        {
            SResourceAccessEvent Event;
            Event.Type = SProcessEvent::EType::ResourceAccess;
            Event.ActorProcessId = vEvent[API_V_EVENT_ACTOR_PID].To<uint64>();
            Event.ActorThreadId = vEvent[API_V_EVENT_ACTOR_TID].To<uint64>();
            uint32 ActorServiceTag = (uint32)vEvent.Get(API_V_EVENT_ACTOR_SVC).To<uint64>();
            if (ActorServiceTag) Event.ActorServiceTag = GetServiceNameFromTag((HANDLE)Event.ActorProcessId, ActorServiceTag);
            Event.TimeStamp = vEvent[API_V_EVENT_TIME_STAMP].To<uint64>();
            Event.Path = vEvent[API_V_EVENT_PATH].AsStr();
            Event.AccessMask = vEvent[API_V_EVENT_ACCESS].To<uint32>();
            Event.Status = (EEventStatus)vEvent[API_V_EVENT_ACCESS_STATUS].To<uint32>();

            CDriverAPI__EmitProcess(This, &Event);
            break;
        }


        case KphMsgProgramRules:
        case KphMsgAccessRules:
		{
			ERuleType Type = MsgTypeToRuleType(Message->Header.MessageId);
			std::wstring Guid = vEvent[API_V_RULE_GUID].AsStr();
            ERuleEvent Event = (ERuleEvent)vEvent[API_V_EVENT].To<uint32>(); // todo move to api header
			CDriverAPI__EmitRuleEvent(This, Type, Guid, Event);
			break;
		}
	}
}

CDriverAPI::CDriverAPI(EInterface Interface)
{
    m_Interface = Interface;
    switch (m_Interface) {
    case EInterface::eFltPort: m_pClient = new CFltPortClient(CDriverAPI__Callback, this); break;
    case EInterface::eDevice: m_pClient = new CDeviceClient; break;
    }
}

CDriverAPI::~CDriverAPI()
{
    delete m_pClient;
}

STATUS CDriverAPI::InstallDrv()
{
    std::wstring FileName = GetApplicationDirectory() + L"\\" API_DRIVER_BINARY;
    std::wstring DisplayName = L"MajorPrivacy Kernel Driver";

    uint32 TraceLogLevel = 6;

    CVariant Params;
    Params["Altitude"] = API_DRIVER_ALTITUDE;
    Params["TraceLevel"] = TraceLogLevel;
    Params["PortName"] = API_DRIVER_PORT;

    return InstallService(API_DRIVER_NAME, FileName.c_str(), DisplayName.c_str(), NULL, OPT_KERNEL_TYPE | OPT_DEMAND_START, Params);
}

STATUS CDriverAPI::ConnectDrv()
{
    STATUS Status;

    if (m_pClient->IsConnected())
		return OK;

    SVC_STATE SvcState = GetServiceState(API_DRIVER_NAME);
    if ((SvcState & SVC_INSTALLED) != SVC_INSTALLED) {
        Status = InstallDrv();
        if (!Status) return Status;
    }
    if ((SvcState & SVC_RUNNING) != SVC_RUNNING)
        Status = RunService(API_DRIVER_NAME);
    if (!Status)
        return Status;

    // FltMgr API

    /*while (!IsDebuggerPresent())
        Sleep(500);
    DebugBreak();*/

    switch (m_Interface) {
    case EInterface::eFltPort: Status = m_pClient->Connect(API_DRIVER_PORT); break; // this require admin rights
    case EInterface::eDevice: Status = m_pClient->Connect(API_DEVICE_NAME); break;
    default: Status = ERR(STATUS_NOINTERFACE);
    }

    //

    return Status;
}

bool CDriverAPI::IsConnected()
{
    return m_pClient->IsConnected();
}

STATUS CDriverAPI::Reconnect()
{
    m_pClient->Disconnect();

    switch (m_Interface) {
    case EInterface::eFltPort: return m_pClient->Connect(API_DRIVER_PORT); // this require admin rights
    case EInterface::eDevice: return m_pClient->Connect(API_DEVICE_NAME);
    default: return ERR(STATUS_NOINTERFACE);
    }
}

void CDriverAPI::Disconnect()
{
    m_pClient->Disconnect();
}

RESULT(CVariant) CDriverAPI::Call(uint32 MessageId, const CVariant& Message)
{
    auto Ret = m_pClient->Call(MessageId, Message);
    if (!Ret.IsError() && (Ret.GetValue().Get(API_V_ERR_CODE).To<uint32>() != 0 || Ret.GetValue().Has(API_V_ERR_MSG)))
        return ERR(Ret.GetValue()[API_V_ERR_CODE], Ret.GetValue()[API_V_ERR_MSG].AsStr());
    return Ret;
}

RESULT(std::shared_ptr<std::vector<uint64>>) CDriverAPI::EnumProcesses()
{
    CVariant ReqVar;
    ReqVar[API_V_ENUM_ALL] = 1;

	auto Ret = m_pClient->Call(API_ENUM_PROCESSES, ReqVar);
    if (Ret.IsError())
        return Ret;
    
	CVariant ResVar = Ret.GetValue();
    CVariant vList = ResVar.Get(API_V_PROG_PROC_PIDS);

    auto pids = std::make_shared<std::vector<uint64>>();

    pids->reserve(vList.Count());
    for(uint32 i=0; i < vList.Count(); i++)
        pids->push_back(vList.At(i));

    RETURN(pids);
}

RESULT(SProcessInfoPtr) CDriverAPI::GetProcessInfo(uint64 pid)
{
    CVariant ReqVar;
    ReqVar[API_V_PID] = pid;

	auto Ret = m_pClient->Call(API_GET_PROCESS_INFO, ReqVar);
    if (Ret.IsError())
        return Ret;

	CVariant ResVar = Ret.GetValue();
    
    SProcessInfoPtr Info = SProcessInfoPtr(new SProcessInfo());

    Info->ImageName = ResVar.Get(API_V_NAME).AsStr();
    Info->FileName = ResVar.Get(API_V_FILE_PATH).AsStr();
    ASSERT(!Info->FileName.empty() || pid == 4); // todo: test
    Info->CreateTime = ResVar[API_V_CREATE_TIME].To<uint64>();
    Info->ParentPid = ResVar[API_V_PARENT_PID].To<uint64>();

    //Info->FileHash = ResVar.Get('hash').AsBytes();
    Info->EnclaveId = ResVar.Get(API_V_EID).To<uint64>(0);
    Info->SecState = ResVar[API_V_KPP_STATE].To<uint32>(0);

    Info->Flags = ResVar[API_V_FLAGS].To<uint32>(0);
    Info->SecFlags = ResVar[API_V_SFLAGS].To<uint32>(0);

    Info->NumberOfImageLoads = ResVar[API_V_N_IMG].To<uint32>(0);
    Info->NumberOfMicrosoftImageLoads = ResVar[API_V_N_MS_IMG].To<uint32>(0);
    Info->NumberOfAntimalwareImageLoads = ResVar[API_V_N_AV_IMG].To<uint32>(0);
    Info->NumberOfVerifiedImageLoads = ResVar[API_V_N_V_IMG].To<uint32>(0);
    Info->NumberOfSignedImageLoads = ResVar[API_V_N_S_IMG].To<uint32>(0);
    Info->NumberOfUntrustedImageLoads = ResVar[API_V_N_U_IMG].To<uint32>(0);


    RETURN(Info);
}

RESULT(SHandleInfoPtr) CDriverAPI::GetHandleInfo(ULONG_PTR UniqueProcessId, ULONG_PTR HandleValue)
{
    CVariant ReqVar;
    ReqVar[(uint32)API_V_PID] = (uint64)UniqueProcessId;
    ReqVar[(uint32)API_V_HANDLE] = (uint64)HandleValue;

    auto Ret = m_pClient->Call(API_GET_HANDLE_INFO, ReqVar);
    if (Ret.IsError())
        return Ret;

    CVariant ResVar = Ret.GetValue();

    SHandleInfoPtr Info = SHandleInfoPtr(new SHandleInfo());
    Info->Path = ResVar.Get((uint32)API_V_FILE_PATH).AsStr();
    Info->Type = ResVar.Get((uint32)API_V_HANDLE_TYPE).AsStr();
    RETURN(Info);
}

RESULT(std::shared_ptr<std::vector<uint64>>) CDriverAPI::EnumEnclaves()
{
    CVariant ReqVar;
    //ReqVar[API_V_ENUM_ALL] = 1;

    auto Ret = m_pClient->Call(API_ENUM_ENCLAVES, ReqVar);
    if (Ret.IsError())
        return Ret;

    CVariant ResVar = Ret.GetValue();
    CVariant vList = ResVar.Get(API_V_EIDS);

    auto pids = std::make_shared<std::vector<uint64>>();

    pids->reserve(vList.Count());
    for(uint32 i=0; i < vList.Count(); i++)
        pids->push_back(vList.At(i));

    RETURN(pids);
}

STATUS CDriverAPI::StartProcessInEnvlave(const std::wstring& path, uint64 EnclaveId)
{
    CVariant ReqVar;
    ReqVar[API_V_EID] = EnclaveId;

	auto Ret = m_pClient->Call(API_PREPARE_ENCLAVE, ReqVar);
    if (Ret.IsError())
        return Ret;

	CVariant ResVar = Ret.GetValue();

    EnclaveId = ResVar[API_V_EID];

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = { 0 };
    CreateProcessW(NULL, (wchar_t*)path.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    
    return OK;
}

STATUS CDriverAPI::SetUserKey(const CBuffer& PubKey, const CBuffer& EncryptedBlob, bool bLock)
{
    CVariant ReqVar;
	ReqVar[API_V_PUB_KEY] = PubKey;
	ReqVar[API_V_PRIV_KEY_BLOB] = EncryptedBlob;
    ReqVar[API_V_LOCK] = bLock;

	return m_pClient->Call(API_SET_USER_KEY, ReqVar);
}

RESULT(SUserKeyInfoPtr) CDriverAPI::GetUserKey()
{
    CVariant ReqVar;
    
    auto Ret = m_pClient->Call(API_GET_USER_KEY, ReqVar);
    if (Ret.IsError())
        return Ret;

    CVariant ResVar = Ret.GetValue();

    SUserKeyInfoPtr Info = SUserKeyInfoPtr(new SUserKeyInfo());
    Info->PubKey = ResVar[API_V_PUB_KEY];
    Info->EncryptedBlob = ResVar[API_V_PRIV_KEY_BLOB];
    Info->bLocked = ResVar.Get(API_V_LOCK).To<bool>(false);
    RETURN(Info);
}

STATUS CDriverAPI::ClearUserKey(const CBuffer& Signature)
{
    CVariant ReqVar;
    ReqVar[API_V_SIGNATURE] = Signature;

    return m_pClient->Call(API_CLEAR_USER_KEY, ReqVar);
}

STATUS CDriverAPI::GetChallenge(CBuffer& Challenge)
{
    CVariant ReqVar;

    auto Ret = m_pClient->Call(API_GET_CHALLENGE, ReqVar);
    if (Ret.IsError())
        return Ret;

    CVariant ResVar = Ret.GetValue();

    Challenge = ResVar[API_V_RAND];

    return OK;
}

//STATUS CDriverAPI::SetupRuleAlias(const std::wstring& PrefixPath, const std::wstring& DevicePath)
//{
//    CVariant ReqVar;
//    ReqVar[API_V_PATH_PREFIX] = PrefixPath;
//    ReqVar[API_V_DEVICE_PATH] = DevicePath;
//
//    return m_pClient->Call(API_SETUP_ACCESS_RULE_ALIAS, ReqVar);
//}
//
//STATUS CDriverAPI::ClearRuleAlias(const std::wstring& DevicePath)
//{
//    CVariant ReqVar;
//    ReqVar[API_V_DEVICE_PATH] = DevicePath;
//
//    return m_pClient->Call(API_CLEAR_ACCESS_RULE_ALIAS, ReqVar);
//}

STATUS CDriverAPI::RegisterForProcesses(bool bRegister)
{
    CVariant ReqVar;
    if(bRegister)
        ReqVar[API_V_SET_NOTIFY_BITMASK] = KphMsgProcessCreate | KphMsgProcessExit | KphMsgUntrustedImage | KphMsgImageLoad | KphMsgProcAccess | KphMsgAccessFile | KphMsgAccessReg;
    else
        ReqVar[API_V_CLEAR_NOTIFY_BITMASK] = KphMsgProcessCreate | KphMsgProcessExit | KphMsgUntrustedImage | KphMsgImageLoad | KphMsgProcAccess | KphMsgAccessFile | KphMsgAccessReg;
	return m_pClient->Call(API_REGISTER_FOR_EVENT, ReqVar);
}

void CDriverAPI::RegisterProcessHandler(const std::function<sint32(const SProcessEvent* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m_HandlersMutex);
	m_ProcessHandlers.push_back(Handler); 
}

STATUS CDriverAPI::RegisterForRuleEvents(ERuleType Type, bool bRegister)
{
    CVariant ReqVar;
    if(bRegister)
        ReqVar[API_V_SET_NOTIFY_BITMASK] = RuleTypeToMsgType(Type);
    else
        ReqVar[API_V_CLEAR_NOTIFY_BITMASK] = RuleTypeToMsgType(Type);
    return m_pClient->Call(API_REGISTER_FOR_EVENT, ReqVar);
}

void CDriverAPI::RegisterRuleEventHandler(ERuleType Type, const std::function<void(const std::wstring& Guid, ERuleEvent Event, ERuleType Type)>& Handler)
{
    std::unique_lock<std::mutex> Lock(m_HandlersMutex);
    m_RuleEventHandlers[Type] = Handler;
}

void CDriverAPI::TestDrv()
{

}