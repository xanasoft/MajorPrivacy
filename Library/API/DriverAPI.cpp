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
#define FIX_EVENT(e) ((uint32)e & 0xBFFFFFFF) // strip 0x40000000
}

uint32 ConfigGroupToMsgType(EConfigGroup Config)
{
	switch (Config)
	{
	case EConfigGroup::eEnclaves:       return KphMsgSecureEnclave;
    case EConfigGroup::eHashDB:         return KphMsgHashDB;
    case EConfigGroup::eProgramRules:   return KphMsgProgramRules;
    case EConfigGroup::eAccessRules:    return KphMsgAccessRules;
	case EConfigGroup::eFirewallRules:  return KphMsgFirewallRules;
	}
	return 0;
}

EConfigGroup MsgTypeToConfigGroup(uint32 Type)
{
	switch (Type)
	{
	case KphMsgSecureEnclave:   return EConfigGroup::eEnclaves;
	case KphMsgHashDB:          return EConfigGroup::eHashDB;
	case KphMsgProgramRules:    return EConfigGroup::eProgramRules;
	case KphMsgAccessRules:     return EConfigGroup::eAccessRules;
	case KphMsgFirewallRules:   return EConfigGroup::eFirewallRules;
	}
	return EConfigGroup::eUndefined;
}

sint32 CDriverAPI__EmitProcess(CDriverAPI* This, const SProcessEvent* pEvent)
{
	std::unique_lock<std::mutex> Lock(This->m_HandlersMutex);
	
    NTSTATUS status = STATUS_SUCCESS;
    for (auto Handler : This->m_ProcessHandlers)
		status = Handler(pEvent);
    return status;
}

void SVerifierInfo::ReadFromEvent(const StVariant& Event)
{
    StatusFlags = Event[API_V_SIGN_FLAGS].To<uint32>();

    PrivateAuthority = (KPH_VERIFY_AUTHORITY)Event[API_V_IMG_SIGN_AUTH].To<uint32>();
    SignLevel = Event[API_V_IMG_SIGN_LEVEL].To<uint32>();
    SignPolicyBits = Event[API_V_IMG_SIGN_POLICY].To<uint32>();

    FileHashAlgorithm = Event[API_V_FILE_HASH_ALG].To<uint32>(0);
    if (FileHashAlgorithm)
        FileHash = Event[API_V_FILE_HASH].AsBytes();

    ThumbprintAlgorithm = Event[API_V_FILE_HASH_ALG].To<uint32>(0);
    if (ThumbprintAlgorithm)
        Thumbprint = Event[API_V_FILE_HASH].AsBytes();

    SignerHashAlgorithm = Event[API_V_IMG_CERT_ALG].To<uint32>(0);
    if (SignerHashAlgorithm) {
        SignerHash = Event[API_V_IMG_CERT_HASH].AsBytes();
        SignerName = Event[API_V_IMG_SIGN_NAME].ToString();
    }

    IssuerHashAlgorithm = Event[API_V_IMG_CA_ALG].To<uint32>(0);
    if (IssuerHashAlgorithm) {
        IssuerHash = Event[API_V_IMG_CA_HASH].AsBytes();
        IssuerName = Event[API_V_IMG_CA_NAME].ToString();
    }

    FoundSignatures.Value = Event[API_V_IMG_SIGN_BITS].To<uint32>();
	AllowedSignatures.Value = Event[API_V_EXEC_ALLOWED_SIGNERS].To<uint32>(0);
}

void CDriverAPI__EmitConfigEvent(CDriverAPI* This, EConfigGroup Config, const std::wstring& Guid, EConfigEvent Event, uint64 PID)
{
    std::unique_lock<std::mutex> Lock(This->m_HandlersMutex);

    NTSTATUS status = STATUS_SUCCESS;
    auto cb = This->m_ConfigEventHandlers[Config];
    if(cb)
        cb(Guid, Event, Config, PID);
}

extern "C" static VOID NTAPI CDriverAPI__Callback(
    _In_ struct _KPH_MESSAGE* Message, 
    _In_ PFLTMGR_CALLBACK_CONTEXT Context, 
    _In_ FLTMGR_REPLYFUNC ReplyFunc
    )
{
    CDriverAPI* This = (CDriverAPI*)Context->Param;

    CBuffer InBuff(Message->Data, Message->Header.Size - sizeof(Message->Header), true);
    StVariant vEvent;
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
            Event.ProcessId = vEvent[API_V_EVENT_TARGET_PID].To<uint64>();
            Event.EnclaveId = vEvent.Get(API_V_EVENT_TARGET_EID).AsStr();
            Event.TimeStamp = vEvent[API_V_EVENT_TIME_STAMP].To<uint64>();
            Event.ParentId = vEvent[API_V_PARENT_PID].To<uint64>();
            Event.FileName = vEvent.Get(API_V_FILE_NT_PATH);
            Event.CommandLine = vEvent.Get(API_V_CMD_LINE);
            
            Event.VerifierInfo.ReadFromEvent(vEvent);

            Event.CreationStatus = vEvent.Get(API_V_NT_STATUS);
            Event.Status = (EEventStatus)vEvent[API_V_EVENT_STATUS].To<uint32>();

            NTSTATUS status = CDriverAPI__EmitProcess(This, &Event);

            /*StVariant vReply;
            vReply[API_V_NT_STATUS] = (sint32)status;

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
            Event.ProcessId = vEvent[API_V_EVENT_TARGET_PID];
            Event.EnclaveId = vEvent.Get(API_V_EVENT_TARGET_EID).AsStr();
            Event.TimeStamp = vEvent[API_V_EVENT_TIME_STAMP].To<uint64>();
            Event.ExitCode = vEvent[API_V_EVENT_ECODE];

            CDriverAPI__EmitProcess(This, &Event);
            break;
        }
        
        case KphMsgImageLoad:
        case KphMsgUntrustedImage:
        {
            SProcessImageEvent Event;
			Event.Type = (Message->Header.MessageId == KphMsgImageLoad) ? SProcessEvent::EType::ImageLoad : SProcessEvent::EType::UntrustedLoad;
            Event.ActorProcessId = vEvent[API_V_EVENT_ACTOR_PID].To<uint64>();
            Event.ActorThreadId = vEvent[API_V_EVENT_ACTOR_TID].To<uint64>();
            uint32 ActorServiceTag = (uint32)vEvent.Get(API_V_EVENT_ACTOR_SVC).To<uint64>();
            if (ActorServiceTag) Event.ActorServiceTag = GetServiceNameFromTag((HANDLE)Event.ActorProcessId, ActorServiceTag);
            Event.ProcessId = vEvent[API_V_EVENT_TARGET_PID].To<uint64>();
            Event.EnclaveId = vEvent.Get(API_V_EVENT_TARGET_EID).AsStr();
            Event.TimeStamp = vEvent[API_V_EVENT_TIME_STAMP].To<uint64>();
            if(Message->Header.MessageId == KphMsgUntrustedImage)
                Event.bLoadPrevented = vEvent[API_V_WAS_BLOCKED].To<bool>();
            Event.FileName = vEvent[API_V_FILE_NT_PATH].AsStr();
            //Event.ImageProperties = vEvent[API_V_EVENT_IMG_PROPS].To<uint32>();
            Event.ImageBase = vEvent[API_V_EVENT_IMG_BASE].To<uint64>();
            //Event.ImageSelector = vEvent[API_V_EVENT_IMG_SEL].To<uint32>();
            //Event.ImageSectionNumber = vEvent[API_V_EVENT_IMG_SECT_NR].To<uint32>();

			Event.VerifierInfo.ReadFromEvent(vEvent);

            CDriverAPI__EmitProcess(This, &Event);
            break;
        }
        case KphMsgProcAccess:
        case KphMsgThreadAccess:
        {
            SProcessAccessEvent Event;
            Event.Type = (Message->Header.MessageId == KphMsgProcAccess) ? SProcessEvent::EType::ProcessAccess : SProcessEvent::EType::ThreadAccess;
            Event.ActorProcessId = vEvent[API_V_EVENT_ACTOR_PID].To<uint64>();
            Event.ActorThreadId = vEvent[API_V_EVENT_ACTOR_TID].To<uint64>();
            uint32 ActorServiceTag = (uint32)vEvent.Get(API_V_EVENT_ACTOR_SVC).To<uint64>();
            if (ActorServiceTag) Event.ActorServiceTag = GetServiceNameFromTag((HANDLE)Event.ActorProcessId, ActorServiceTag);
            Event.ProcessId = vEvent[API_V_EVENT_TARGET_PID].To<uint64>();
            Event.EnclaveId = vEvent.Get(API_V_EVENT_TARGET_EID).AsStr();
            Event.TimeStamp = vEvent[API_V_EVENT_TIME_STAMP].To<uint64>();
            //Event.bThread = Message->Header.MessageId == KphMsgThreadAccess;
            Event.AccessMask = vEvent[API_V_ACCESS_MASK].To<uint32>();
            Event.Status = (EEventStatus)vEvent[API_V_EVENT_STATUS].To<uint32>();
            Event.bSupressed = vEvent[API_V_EVENT_NO_PROTECT].To<bool>();
            Event.bCreating = vEvent[API_V_EVENT_CREATING].To<bool>();

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
            Event.Path = vEvent[API_V_FILE_NT_PATH].AsStr();
            Event.AccessMask = vEvent[API_V_ACCESS_MASK].To<uint32>();
            Event.Status = (EEventStatus)vEvent[API_V_EVENT_STATUS].To<uint32>();
            Event.RuleGuid = vEvent.Get(API_V_RULE_REF_GUID).AsStr();
            Event.NtStatus = vEvent.Get(API_V_NT_STATUS).To<uint32>();            
            Event.IsDirectory = vEvent.Get(API_V_IS_DIRECTORY).To<bool>();            

            Event.TimeOut = vEvent.Get(API_V_TIMEOUT).To<uint32>(0);

            CDriverAPI__EmitProcess(This, &Event);

            if (Event.TimeOut != 0)
            {
				if (Event.Action == EAccessRuleType::eNone) // we have no verdict / timeout
					Event.Action = EAccessRuleType::eProtect;

                StVariant vReply;
                vReply[API_V_EVENT_ACTION] = (sint32)Event.Action;

                CBuffer OutBuff;
                OutBuff.WriteData(NULL, sizeof(MSG_HEADER)); // make room for header, pointer points after the header
                PMSG_HEADER repHeader = (PMSG_HEADER)OutBuff.GetBuffer();
                repHeader->MessageId = Message->Header.MessageId;
                repHeader->Size = sizeof(MSG_HEADER);
                vReply.ToPacket(&OutBuff);
                PKPH_MESSAGE msg = (PKPH_MESSAGE)OutBuff.GetBuffer();
                msg->Header.Size = (ULONG)OutBuff.GetSize();

                ReplyFunc(Context, msg);
            }

            break;
        }

        case KphMsgInjectDll:
        {
            SInjectionRequest Event;
            Event.Type = SProcessEvent::EType::InjectionRequest;
            Event.ProcessId = vEvent[API_V_PID].To<uint64>();
            
            NTSTATUS status = CDriverAPI__EmitProcess(This, &Event);

            StVariant vReply;
            vReply[API_V_STATUS] = (sint32)status;

            CBuffer OutBuff;
            OutBuff.WriteData(NULL, sizeof(MSG_HEADER)); // make room for header, pointer points after the header
            PMSG_HEADER repHeader = (PMSG_HEADER)OutBuff.GetBuffer();
            repHeader->MessageId = Message->Header.MessageId;
            repHeader->Size = sizeof(MSG_HEADER);
            vReply.ToPacket(&OutBuff);
            PKPH_MESSAGE msg = (PKPH_MESSAGE)OutBuff.GetBuffer();
            msg->Header.Size = (ULONG)OutBuff.GetSize();

            ReplyFunc(Context, msg);
            break;
        }


        case KphMsgSecureEnclave:
		case KphMsgHashDB:
        case KphMsgProgramRules:
        case KphMsgAccessRules:
		{
            EConfigGroup Config = MsgTypeToConfigGroup(Message->Header.MessageId);
			std::wstring Guid = vEvent[API_V_GUID].AsStr();
            EConfigEvent Event = (EConfigEvent)vEvent[API_V_EVENT_TYPE].To<uint32>(); // todo move to api header
            uint64 PID = vEvent[API_V_EVENT_ACTOR_PID].To<uint64>();
			CDriverAPI__EmitConfigEvent(This, Config, Guid, Event, PID);
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

STATUS CDriverAPI::InstallDrv(bool bAutoStart, uint32 TraceLogLevel)
{
    std::wstring FileName = GetApplicationDirectory() + L"\\" API_DRIVER_BINARY;
    std::wstring DisplayName = L"MajorPrivacy Kernel Driver";

    StVariant Params;
    Params["Altitude"] = API_DRIVER_ALTITUDE;
    Params["PortName"] = API_DRIVER_PORT;
    Params["TraceLevel"] = TraceLogLevel;

    uint32 uOptions = OPT_KERNEL_TYPE;
    if(bAutoStart)
        uOptions |= OPT_AUTO_START;
    else
        uOptions |= OPT_DEMAND_START;

    return InstallService(API_DRIVER_NAME, FileName.c_str(), DisplayName.c_str(), NULL, NULL, uOptions, Params);
}

STATUS CDriverAPI::ConnectDrv()
{
    if (m_pClient->IsConnected())
		m_pClient->Disconnect();

    STATUS Status;

    SVC_STATE SvcState = GetServiceState(API_DRIVER_NAME);
    if ((SvcState & SVC_INSTALLED) != SVC_INSTALLED) {
        return ERR(STATUS_DEVICE_NOT_READY);
        //Status = InstallDrv();
        //if (!Status) return Status;
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

void CDriverAPI::Disconnect()
{
    m_pClient->Disconnect();
}

RESULT(StVariant) CDriverAPI::Call(uint32 MessageId, const StVariant& Message, SCallParams* pParams)
{
    std::unique_lock Lock(m_CallMutex);
    auto Ret = m_pClient->Call(MessageId, Message, pParams);
	auto& Val = Ret.GetValue();
    if (!Ret.IsError() && Val.GetType() == VAR_TYPE_INDEX && (Val.Get(API_V_ERR_CODE).To<uint32>() != 0 || Val.Has(API_V_ERR_MSG)))
        return ERR(Val[API_V_ERR_CODE], Val[API_V_ERR_MSG].AsStr());
    return Ret;
}

RESULT(std::shared_ptr<std::vector<uint64>>) CDriverAPI::EnumProcesses()
{
    StVariant ReqVar;
    ReqVar[API_V_ENUM_ALL] = 1;

	auto Ret = Call(API_ENUM_PROCESSES, ReqVar, NULL);
    if (Ret.IsError())
        return Ret;
    
    StVariant ResVar = Ret.GetValue();
    StVariant vList = ResVar.Get(API_V_PIDS);

    auto pids = std::make_shared<std::vector<uint64>>();

    pids->reserve(vList.Count());
    for(uint32 i=0; i < vList.Count(); i++)
        pids->push_back(vList.At(i));

    RETURN(pids);
}

RESULT(SProcessInfoPtr) CDriverAPI::GetProcessInfo(uint64 pid)
{
    StVariant ReqVar;
    ReqVar[API_V_PID] = pid;

    ASSERT(pid != 0xffffffff); // bad cast from uint32 -1 !!!

	auto Ret = Call(API_GET_PROCESS_INFO, ReqVar, NULL);
    if (Ret.IsError())
        return Ret;

    StVariant ResVar = Ret.GetValue();
    
    SProcessInfoPtr Info = SProcessInfoPtr(new SProcessInfo());

    Info->ImageName = ResVar.Get(API_V_NAME).AsStr();
    Info->FileName = ResVar.Get(API_V_FILE_PATH).AsStr();
    Info->CreateTime = ResVar[API_V_CREATE_TIME].To<uint64>();
    Info->ParentPid = ResVar[API_V_PARENT_PID].To<uint64>();
    Info->CreatorPid = ResVar[API_V_CREATOR_PID].To<uint64>();
    Info->CreatorTid = ResVar[API_V_CREATOR_TID].To<uint64>();

    //Info->FileHash = ResVar.Get('hash').AsBytes();
    Info->Enclave = ResVar.Get(API_V_ENCLAVE).AsStr();
    Info->SecState = ResVar[API_V_KPP_STATE].To<uint32>(0);

    Info->Flags = ResVar[API_V_FLAGS].To<uint32>(0);
    Info->SecFlags = ResVar[API_V_SFLAGS].To<uint32>(0);

    Info->NumberOfImageLoads = ResVar[API_V_NUM_IMG].To<uint32>(0);
    Info->NumberOfMicrosoftImageLoads = ResVar[API_V_NUM_MS_IMG].To<uint32>(0);
    Info->NumberOfAntimalwareImageLoads = ResVar[API_V_NUM_AV_IMG].To<uint32>(0);
    Info->NumberOfVerifiedImageLoads = ResVar[API_V_NUM_V_IMG].To<uint32>(0);
    Info->NumberOfSignedImageLoads = ResVar[API_V_NUM_S_IMG].To<uint32>(0);
    Info->NumberOfUntrustedImageLoads = ResVar[API_V_NUM_U_IMG].To<uint32>(0);


    RETURN(Info);
}

RESULT(SHandleInfoPtr) CDriverAPI::GetHandleInfo(ULONG_PTR UniqueProcessId, ULONG_PTR HandleValue)
{
    StVariant ReqVar;
    ReqVar[(uint32)API_V_PID] = (uint64)UniqueProcessId;
    ReqVar[(uint32)API_V_HANDLE] = (uint64)HandleValue;

    auto Ret = Call(API_GET_HANDLE_INFO, ReqVar, NULL);
    if (Ret.IsError())
        return Ret;

    StVariant ResVar = Ret.GetValue();

    SHandleInfoPtr Info = SHandleInfoPtr(new SHandleInfo());
    Info->Path = ResVar.Get((uint32)API_V_FILE_PATH).AsStr();
    Info->Type = ResVar.Get((uint32)API_V_HANDLE_TYPE).AsStr();
    RETURN(Info);
}

STATUS CDriverAPI::PrepareEnclave(const CFlexGuid& EnclaveGuid)
{
    StVariant ReqVar;
    ReqVar[API_V_GUID] = EnclaveGuid.ToVariant(true);
	return Call(API_PREP_ENCLAVE, ReqVar, NULL);
}

RESULT(StVariant) CDriverAPI::GetConfig(const char* Name)
{
    StVariant ReqVar;
    ReqVar[API_V_KEY] = Name;
    auto Ret = Call(API_GET_CONFIG_VALUE, ReqVar);
    if (Ret.IsError())
        return Ret.GetStatus();
    return Ret.GetValue().Get(API_V_VALUE);
}

STATUS CDriverAPI::SetConfig(const char* Name, const StVariant& Value)
{
    StVariant ReqVar;
    ReqVar[API_V_KEY] = Name;
    ReqVar[API_V_VALUE] = Value;
    return Call(API_SET_CONFIG_VALUE, ReqVar);
}

STATUS CDriverAPI::SetUserKey(const CBuffer& PubKey, const CBuffer& EncryptedBlob, bool bLock)
{
    StVariant ReqVar;
	ReqVar[API_V_PUB_KEY] = PubKey;
	ReqVar[API_V_KEY_BLOB] = EncryptedBlob;
    ReqVar[API_V_LOCK] = bLock;

	return Call(API_SET_USER_KEY, ReqVar, NULL);
}

RESULT(SUserKeyInfoPtr) CDriverAPI::GetUserKey()
{
    StVariant ReqVar;
    
    auto Ret = Call(API_GET_USER_KEY, ReqVar, NULL);
    if (Ret.IsError())
        return Ret;

    StVariant ResVar = Ret.GetValue();

    SUserKeyInfoPtr Info = SUserKeyInfoPtr(new SUserKeyInfo());
    Info->PubKey = ResVar[API_V_PUB_KEY];
    Info->EncryptedBlob = ResVar[API_V_KEY_BLOB];
    Info->bLocked = ResVar.Get(API_V_LOCK).To<bool>(false);

    RETURN(Info);
}

STATUS CDriverAPI::ClearUserKey(const CBuffer& ChallengeResponse)
{
    StVariant ReqVar;
    ReqVar[API_V_SIGNATURE] = ChallengeResponse;

    return Call(API_CLEAR_USER_KEY, ReqVar, NULL);
}

STATUS CDriverAPI::ProtectConfig(const CBuffer& ConfigSignature, bool bHardLock)
{
    StVariant ReqVar;
    ReqVar[API_V_SIGNATURE] = ConfigSignature;
	ReqVar[API_V_LOCK] = bHardLock;

	return Call(API_PROTECT_CONFIG, ReqVar, NULL);
}

STATUS CDriverAPI::UnprotectConfig(const CBuffer& ChallengeResponse)
{
    StVariant ReqVar;
	ReqVar[API_V_SIGNATURE] = ChallengeResponse;

	return Call(API_UNPROTECT_CONFIG, ReqVar, NULL);
}

uint32 CDriverAPI::GetConfigStatus()
{
    StVariant ReqVar;

    auto Ret = Call(API_GET_CONFIG_STATUS, ReqVar, NULL);
    if (Ret.IsError())
        return false;

    StVariant ResVar = Ret.GetValue();

    return ResVar.To<uint32>();
}

STATUS CDriverAPI::UnlockConfig(const CBuffer& ChallengeResponse)
{
    StVariant ReqVar;
    ReqVar[API_V_SIGNATURE] = ChallengeResponse;

    return Call(API_UNLOCK_CONFIG, ReqVar, NULL);
}

STATUS CDriverAPI::CommitConfigChanges(const CBuffer& ConfigSignature)
{
    StVariant ReqVar;
    if(ConfigSignature.GetSize() > 0)
	    ReqVar[API_V_SIGNATURE] = ConfigSignature;

	return Call(API_COMMIT_CONFIG, ReqVar, NULL);
}

STATUS CDriverAPI::StoreConfigChanges(bool bPreShutdown)
{
    StVariant ReqVar;
    ReqVar[API_V_SHUTDOWN] = bPreShutdown;

    return Call(API_STORE_CONFIG, ReqVar, NULL);
}

STATUS CDriverAPI::DiscardConfigChanges()
{
    StVariant ReqVar;

	return Call(API_DISCARD_CHANGES, ReqVar, NULL);
}

STATUS CDriverAPI::GetChallenge(CBuffer& Challenge)
{
    StVariant ReqVar;

    auto Ret = Call(API_GET_CHALLENGE, ReqVar, NULL);
    if (Ret.IsError())
        return Ret;

    StVariant ResVar = Ret.GetValue();

    Challenge = ResVar[API_V_RAND];

    return OK;
}

STATUS CDriverAPI::GetConfigHash(CBuffer& ConfigHash, const StVariant& Data)
{
    StVariant ReqVar;
	//ReqVar[API_V_GET_DATA] = true; // request the config blob to be returned
    if(Data.IsValid())
        ReqVar[API_V_DATA] = Data;

    auto Ret = Call(API_GET_CONFIG_HASH, ReqVar, NULL);
    if (Ret.IsError())
        return Ret;

    StVariant ResVar = Ret.GetValue();

    ConfigHash = ResVar[API_V_HASH];

    return OK;
}

//STATUS CDriverAPI::SetupRuleAlias(const std::wstring& PrefixPath, const std::wstring& DevicePath)
//{
//    StVariant ReqVar;
//    ReqVar[API_V_PATH_PREFIX] = PrefixPath;
//    ReqVar[API_V_DEVICE_PATH] = DevicePath;
//
//    return Call(API_SETUP_ACCESS_RULE_ALIAS, ReqVar);
//}
//
//STATUS CDriverAPI::ClearRuleAlias(const std::wstring& DevicePath)
//{
//    StVariant ReqVar;
//    ReqVar[API_V_DEVICE_PATH] = DevicePath;
//
//    return Call(API_CLEAR_ACCESS_RULE_ALIAS, ReqVar);
//}

STATUS CDriverAPI::AcquireUnloadProtection()
{
	StVariant ReqVar;
	return Call(API_ACQUIRE_NO_UNLOAD, ReqVar, NULL);
}

LONG CDriverAPI::ReleaseUnloadProtection()
{
	StVariant ReqVar;
	auto Ret = Call(API_RELEASE_NO_UNLOAD, ReqVar, NULL);
	if (Ret.IsError())
		return 0;
	return Ret.GetValue().To<sint32>();
}

STATUS CDriverAPI::AcquireHibernationPrevention()
{
    StVariant ReqVar;
	return Call(API_ACQUIRE_NO_HIBERNATION, ReqVar, NULL);
}

LONG CDriverAPI::ReleaseHibernationPrevention()
{
    StVariant ReqVar;
    auto Ret = Call(API_RELEASE_NO_HIBERNATION, ReqVar, NULL);
    if (Ret.IsError())
        return 0;
	return Ret.GetValue().To<sint32>();
}

STATUS CDriverAPI::RegisterForProcesses(uint32 uEvents, bool bRegister)
{
    StVariant ReqVar;
    if(bRegister)
        ReqVar[API_V_SET_NOTIFY_BITMASK] = FIX_EVENT(uEvents);
    else
        ReqVar[API_V_CLEAR_NOTIFY_BITMASK] = FIX_EVENT(uEvents);
	return Call(API_REGISTER_FOR_EVENT, ReqVar, NULL);
}

void CDriverAPI::RegisterProcessHandler(const std::function<sint32(const SProcessEvent* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m_HandlersMutex);
	m_ProcessHandlers.push_back(Handler); 
}

STATUS CDriverAPI::RegisterForConfigEvents(EConfigGroup Config, bool bRegister)
{
    StVariant ReqVar;
    if(bRegister)
        ReqVar[API_V_SET_NOTIFY_BITMASK] = FIX_EVENT(ConfigGroupToMsgType(Config));
    else
        ReqVar[API_V_CLEAR_NOTIFY_BITMASK] = FIX_EVENT(ConfigGroupToMsgType(Config));
    return Call(API_REGISTER_FOR_EVENT, ReqVar, NULL);
}

void CDriverAPI::RegisterConfigEventHandler(EConfigGroup Config, const std::function<void(const std::wstring& Guid, EConfigEvent Event, EConfigGroup Config, uint64 PID)>& Handler)
{
    std::unique_lock<std::mutex> Lock(m_HandlersMutex);
    m_ConfigEventHandlers[Config] = Handler;
}

uint32 CDriverAPI::GetABIVersion()
{
    StVariant Request;
    auto Ret = Call(API_GET_VERSION, Request);
    StVariant Response = Ret.GetValue();
    uint32 version = Response.Get(API_V_VERSION).To<uint32>();
    return version;
}