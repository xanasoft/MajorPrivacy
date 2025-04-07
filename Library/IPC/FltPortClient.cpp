#include "pch.h"

/*#include <ntstatus.h>
#define WIN32_NO_STATUS
typedef long NTSTATUS;
#include <windows.h>
#include <winternl.h>*/

#include <phnt_windows.h>
#include <phnt.h>

#include <ph.h>


#include "FltPortClient.h"

#include "../Helpers/Service.h"
#include "../Helpers/NtIO.h"
#include <fltuser.h>
#include "../Helpers/AppUtil.h"
//#include "../Helpers/WinUtil.h"

#include "../../Driver/KSI/include/kphmsg.h"

typedef struct _KPH_UMESSAGE
{
    FILTER_MESSAGE_HEADER MessageHeader;
    KPH_MESSAGE Message;
    OVERLAPPED Overlapped;
} KPH_UMESSAGE, *PKPH_UMESSAGE;

typedef struct _KPH_UREPLY
{
    FILTER_REPLY_HEADER ReplyHeader;
    KPH_MESSAGE Message;
} KPH_UREPLY, *PKPH_UREPLY;


#define KPH_COMMS_MIN_THREADS   2
#define KPH_COMMS_MESSAGE_SCALE 2
#define KPH_COMMS_THREAD_SCALE  2
#define KPH_COMMS_MAX_MESSAGES  1024


typedef struct _FILTER_PORT_EA
{
    PUNICODE_STRING PortName;
    PVOID Unknown;
    USHORT SizeOfContext;
    BYTE Padding[6];
    BYTE ConnectionContext[ANYSIZE_ARRAY];
} FILTER_PORT_EA, *PFILTER_PORT_EA;

#define FLT_PORT_EA_NAME "FLTPORT"
#define FLT_CTL_CREATE CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 3, METHOD_NEITHER, FILE_READ_ACCESS)
#define FLT_CTL_ATTACH CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 4, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define FLT_CTL_DETATCH CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 5, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define FLT_CTL_SEND_MESSAGE CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 6, METHOD_NEITHER, FILE_WRITE_ACCESS)
#define FLT_CTL_GET_MESSAGE CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 7, METHOD_NEITHER, FILE_READ_ACCESS)
#define FLT_CTL_REPLY_MESSAGE CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 8, METHOD_NEITHER, FILE_WRITE_ACCESS)
#define FLT_CTL_FIND_FIRST CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 9, METHOD_BUFFERED, FILE_READ_ACCESS)
#define FLT_CTL_FIND_NEXT CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 10, METHOD_BUFFERED, FILE_READ_ACCESS)
#define FLT_CTL_QUERY_INFORMATION CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 11, METHOD_BUFFERED, FILE_READ_ACCESS)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SFltPortClient
//

struct SFltPortClient
{
    SFltPortClient()
    {

    }

    NTSTATUS ConnectCommunicationPort(
        _In_ const std::wstring& PortName,
        _In_ ULONG Options,
        _In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
        _In_ USHORT SizeOfContext,
        _In_opt_ PSECURITY_ATTRIBUTES SecurityAttributes,
        _Outptr_ HANDLE *Port
        )
    {
        NTSTATUS status;
        OBJECT_ATTRIBUTES objectAttributes;
        UNICODE_STRING objectName;
        UNICODE_STRING portName;
        ULONG eaLength;
        PFILE_FULL_EA_INFORMATION ea;
        PFILTER_PORT_EA eaValue;
        IO_STATUS_BLOCK isb;

        *Port = NULL;

        if (((SizeOfContext > 0) && !ConnectionContext) || ((SizeOfContext == 0) && ConnectionContext))
            return STATUS_INVALID_PARAMETER;

        RtlInitUnicodeString(&portName, PortName.c_str());

        //
        // Build the filter EA, this contains the port name and the context.
        //

        eaLength = (sizeof(FILE_FULL_EA_INFORMATION)
                 + sizeof(FILTER_PORT_EA)
                 + ARRAYSIZE(FLT_PORT_EA_NAME)
                 + SizeOfContext);

        ea = (PFILE_FULL_EA_INFORMATION)malloc(eaLength);
        if (!ea)
            return STATUS_INSUFFICIENT_RESOURCES;
        memset(ea, 0, eaLength);

        ea->Flags = 0;
        ea->EaNameLength = ARRAYSIZE(FLT_PORT_EA_NAME) - sizeof(ANSI_NULL);
        RtlCopyMemory(ea->EaName, FLT_PORT_EA_NAME, ARRAYSIZE(FLT_PORT_EA_NAME));
        ea->EaValueLength = sizeof(FILTER_PORT_EA) + SizeOfContext;
        eaValue = (PFILTER_PORT_EA)PTR_ADD_OFFSET(ea->EaName, ea->EaNameLength + sizeof(ANSI_NULL));
        eaValue->Unknown = NULL;
        eaValue->SizeOfContext = SizeOfContext;
        eaValue->PortName = &portName;

        if (SizeOfContext > 0)
            RtlCopyMemory(eaValue->ConnectionContext, ConnectionContext, SizeOfContext);

        RtlInitUnicodeString(&objectName, L"\\FileSystem\\Filters\\FltMgrMsg");
        InitializeObjectAttributes(&objectAttributes, &objectName, OBJ_CASE_INSENSITIVE | (WindowsVersion < WINDOWS_10 ? 0 : OBJ_DONT_REPARSE), NULL, NULL);
        if (SecurityAttributes) {
            if (SecurityAttributes->bInheritHandle)
                objectAttributes.Attributes |= OBJ_INHERIT;
            objectAttributes.SecurityDescriptor = SecurityAttributes->lpSecurityDescriptor;
        }

        status = NtCreateFile(Port, FILE_READ_ACCESS | FILE_WRITE_ACCESS | SYNCHRONIZE, &objectAttributes, &isb, NULL, 0, 0, FILE_OPEN_IF, Options & FLT_PORT_FLAG_SYNC_HANDLE ? FILE_SYNCHRONOUS_IO_NONALERT : 0, ea, eaLength);

        free(ea);

        return status;
    }


    NTSTATUS DeviceIoControl(
        _In_ HANDLE Handle,
        _In_ ULONG IoControlCode,
        _In_reads_bytes_(InBufferSize) PVOID InBuffer,
        _In_ ULONG InBufferSize,
        _Out_writes_bytes_to_opt_(OutBufferSize, *BytesReturned) PVOID OutBuffer,
        _In_ ULONG OutBufferSize,
        _Out_opt_ PULONG BytesReturned,
        _Inout_opt_ LPOVERLAPPED Overlapped
        )
    {
        NTSTATUS status;

        if (BytesReturned)
            *BytesReturned = 0;

        if (Overlapped)
        {
            Overlapped->Internal = STATUS_PENDING;

            if (DEVICE_TYPE_FROM_CTL_CODE(IoControlCode) == FILE_DEVICE_FILE_SYSTEM)
                status = NtFsControlFile(Handle, Overlapped->hEvent, NULL, Overlapped, (PIO_STATUS_BLOCK)Overlapped, IoControlCode, InBuffer, InBufferSize, OutBuffer, OutBufferSize);
            else
                status = NtDeviceIoControlFile(Handle, Overlapped->hEvent, NULL, Overlapped, (PIO_STATUS_BLOCK)Overlapped, IoControlCode, InBuffer, InBufferSize, OutBuffer, OutBufferSize);

            if (NT_INFORMATION(status) && BytesReturned)
                *BytesReturned = (ULONG)Overlapped->InternalHigh;
        }
        else
        {
            IO_STATUS_BLOCK ioStatusBlock;

            if (DEVICE_TYPE_FROM_CTL_CODE(IoControlCode) == FILE_DEVICE_FILE_SYSTEM)
                status = NtFsControlFile(Handle, NULL, NULL, NULL, &ioStatusBlock, IoControlCode, InBuffer, InBufferSize, OutBuffer, OutBufferSize);
            else
                status = NtDeviceIoControlFile(Handle, NULL, NULL, NULL, &ioStatusBlock, IoControlCode, InBuffer, InBufferSize, OutBuffer, OutBufferSize);

            if (status == STATUS_PENDING)
            {
                status = NtWaitForSingleObject(Handle, FALSE, NULL);
                if (NT_SUCCESS(status))
                    status = ioStatusBlock.Status;
            }

            if (BytesReturned)
                *BytesReturned = (ULONG)ioStatusBlock.Information;
        }

        return status;
    }

    _Must_inspect_result_
    NTSTATUS PortGetMessage(
        _In_ HANDLE Port,
        _Out_writes_bytes_(MessageBufferSize) PFILTER_MESSAGE_HEADER MessageBuffer,
        _In_ ULONG MessageBufferSize,
        _Inout_ LPOVERLAPPED Overlapped
        )
    {
        return DeviceIoControl(Port, FLT_CTL_GET_MESSAGE, NULL, 0, MessageBuffer, MessageBufferSize, NULL, Overlapped);
    }

    NTSTATUS PortSendMessage(
        _In_ HANDLE Port,
        _In_reads_bytes_(InBufferSize) PVOID InBuffer,
        _In_ ULONG InBufferSize,
        _Out_writes_bytes_to_opt_(OutBufferSize,*BytesReturned) PVOID OutBuffer,
        _In_ ULONG OutBufferSize,
        _Out_ PULONG BytesReturned
        )
    {
        return DeviceIoControl(Port, FLT_CTL_SEND_MESSAGE, InBuffer, InBufferSize, OutBuffer, OutBufferSize, BytesReturned, NULL);
    }

    NTSTATUS PortReplyMessage(
        _In_ HANDLE Port,
        _In_reads_bytes_(ReplyBufferSize) PFILTER_REPLY_HEADER ReplyBuffer,
        _In_ ULONG ReplyBufferSize
        )
    {
        return DeviceIoControl(Port, FLT_CTL_REPLY_MESSAGE, ReplyBuffer, ReplyBufferSize, NULL, 0, NULL, NULL);
    }

    static VOID WINAPI IoCallbackFunc(
        _Inout_ PTP_CALLBACK_INSTANCE Instance,
        _Inout_opt_ PVOID Context,
        _In_ PVOID ApcContext,
        _In_ PIO_STATUS_BLOCK IoSB,
        _In_ PTP_IO Io
    )
    {
        return ((SFltPortClient*)Context)->IoCallback(ApcContext, IoSB, Io);
    }

    VOID IoCallback(
        _In_ PVOID ApcContext,
        _In_ PIO_STATUS_BLOCK IoSB,
        _In_ PTP_IO Io
        )
    {
        NTSTATUS status;
        PKPH_UMESSAGE msg;

        if (!PhAcquireRundownProtection(&Rundown))
            return;

        msg = CONTAINING_RECORD(ApcContext, KPH_UMESSAGE, Overlapped);

        if (IoSB->Status != STATUS_SUCCESS)
            goto Exit;

        if (IoSB->Information < KPH_MESSAGE_MIN_SIZE) {
            //assert(FALSE);
            goto Exit;
        }

        if (RegisteredCallback) {
            FLTMGR_CALLBACK_CONTEXT Context = { (ULONG_PTR)&msg->MessageHeader, this, RegisteredCallbackParam };
            RegisteredCallback(&msg->Message, &Context, ReplyMessageFunc);
        }

    Exit:

        RtlZeroMemory(&msg->Overlapped, FIELD_OFFSET(OVERLAPPED, hEvent));

        TpStartAsyncIoOperation(ThreadPoolIo);

        status = PortGetMessage(FltPortHandle, &msg->MessageHeader, FIELD_OFFSET(KPH_UMESSAGE, Overlapped), &msg->Overlapped);

        if (status != STATUS_PENDING)
        {
            if (status == STATUS_PORT_DISCONNECTED)
            {
                //
                // Mark the port disconnected so IsConnected returns false.
                // This can happen if the driver goes away before the client.
                //
                PortDisconnected = TRUE;
            }
            else
            {
                //assert(FALSE);
            }

            TpCancelAsyncIoOperation(ThreadPoolIo);
        }

        PhReleaseRundownProtection(&Rundown);
    }

    NTSTATUS Start(
        _In_ std::wstring PortName
        )
    {
        NTSTATUS status;
        ULONG numberOfThreads;

        if (FltPortHandle) {
            status = STATUS_ALREADY_INITIALIZED;
            goto Exit;
        }

        status = ConnectCommunicationPort(PortName, 0, NULL, 0, NULL, &FltPortHandle);
        if (!NT_SUCCESS(status))
            goto Exit;

        PortDisconnected = FALSE;

        PhInitializeRundownProtection(&Rundown);
        //PhInitializeFreeList(&ReplyFreeList, sizeof(KPH_UREPLY), 16);

#ifndef _DEBUG
        if (PhSystemBasicInformation.NumberOfProcessors >= KPH_COMMS_MIN_THREADS)
            numberOfThreads = PhSystemBasicInformation.NumberOfProcessors * KPH_COMMS_THREAD_SCALE;
        else
#endif
            numberOfThreads = KPH_COMMS_MIN_THREADS;

        MessageCount = numberOfThreads * KPH_COMMS_MESSAGE_SCALE;
        if (MessageCount > KPH_COMMS_MAX_MESSAGES)
            MessageCount = KPH_COMMS_MAX_MESSAGES;

        status = TpAllocPool(&ThreadPool, NULL);
        if (!NT_SUCCESS(status))
            goto Exit;

        TpSetPoolMinThreads(ThreadPool, KPH_COMMS_MIN_THREADS);
        TpSetPoolMaxThreads(ThreadPool, numberOfThreads);
        KphpTpSetPoolThreadBasePriority(ThreadPool, THREAD_PRIORITY_HIGHEST);

        TpInitializeCallbackEnviron(&ThreadPoolEnv);
        TpSetCallbackNoActivationContext(&ThreadPoolEnv);
        TpSetCallbackPriority(&ThreadPoolEnv, TP_CALLBACK_PRIORITY_HIGH);
        TpSetCallbackThreadpool(&ThreadPoolEnv, ThreadPool);
        //TpSetCallbackLongFunction(&ThreadPoolEnv);

        PhSetFileCompletionNotificationMode(FltPortHandle, FILE_SKIP_SET_EVENT_ON_HANDLE);

        status = TpAllocIoCompletion(&ThreadPoolIo, FltPortHandle, IoCallbackFunc, this, &ThreadPoolEnv);
        if (!NT_SUCCESS(status))
            goto Exit;

        Messages = (PKPH_UMESSAGE)malloc(sizeof(KPH_UMESSAGE) * MessageCount);
        if (!Messages) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit;
        }
        memset(Messages, 0, sizeof(KPH_UMESSAGE) * MessageCount);

        for (ULONG i = 0; i < MessageCount; i++)
        {
            status = NtCreateEvent(&Messages[i].Overlapped.hEvent, EVENT_ALL_ACCESS, NULL, NotificationEvent, FALSE);
        
            if (!NT_SUCCESS(status))
                goto Exit;
        
            RtlZeroMemory(&Messages[i].Overlapped,
                FIELD_OFFSET(OVERLAPPED, hEvent));

            TpStartAsyncIoOperation(ThreadPoolIo);

            status = PortGetMessage(FltPortHandle, &Messages[i].MessageHeader, FIELD_OFFSET(KPH_UMESSAGE, Overlapped), &Messages[i].Overlapped);
            if (status == STATUS_PENDING)
                status = STATUS_SUCCESS;
            else {
                status = STATUS_FLT_INTERNAL_ERROR;
                goto Exit;
            }
        }

        status = STATUS_SUCCESS;

    Exit:
        if (!NT_SUCCESS(status))
            Stop();

        return status;
    }

    VOID Stop()
    {
        if (!FltPortHandle)
            return;

        PhWaitForRundownProtection(&Rundown);

        if (ThreadPoolIo)
        {
            TpWaitForIoCompletion(ThreadPoolIo, TRUE);
            TpReleaseIoCompletion(ThreadPoolIo);
            ThreadPoolIo = NULL;
        }

        TpDestroyCallbackEnviron(&ThreadPoolEnv);

        if (ThreadPool)
        {
            TpReleasePool(ThreadPool);
            ThreadPool = NULL;
        }

        RegisteredCallback = NULL;
        PortDisconnected = TRUE;

        if (FltPortHandle)
        {
            NtClose(FltPortHandle);
            FltPortHandle = NULL;
        }

        if (Messages)
        {
            for (ULONG i = 0; i < MessageCount; i++)
            {
                if (Messages[i].Overlapped.hEvent)
                {
                    NtClose(Messages[i].Overlapped.hEvent);
                    Messages[i].Overlapped.hEvent = NULL;
                }
            }

            MessageCount = 0;

            free(Messages);
            Messages = NULL;
        }

        //PhDeleteFreeList(&ReplyFreeList);
    }

    static NTSTATUS ReplyMessageFunc(
        _In_ PFLTMGR_CALLBACK_CONTEXT Context, 
        _In_ PKPH_MESSAGE Message
        )
    {
        return ((SFltPortClient*)Context->Client)->ReplyMessage(Context->ReplyToken, Message);
    }

    NTSTATUS ReplyMessage(
        _In_ ULONG_PTR ReplyToken,
        _In_ PKPH_MESSAGE Message
        )
    {
        NTSTATUS status;
        PKPH_UREPLY reply;
        PFILTER_MESSAGE_HEADER header;

        reply = NULL;
        header = (PFILTER_MESSAGE_HEADER)ReplyToken;

        if (!FltPortHandle) {
            status = STATUS_FLT_NOT_INITIALIZED;
            goto Exit;
        }

        if (!header || (header->ReplyLength == 0))
        {
            //
            // Kernel is not expecting a reply.
            //
            status = STATUS_INVALID_PARAMETER_1;
            goto Exit;
        }

        reply = (PKPH_UREPLY)malloc(sizeof(KPH_UREPLY));

        RtlZeroMemory(reply, sizeof(KPH_UREPLY));
        RtlCopyMemory(&reply->ReplyHeader, header, sizeof(reply->ReplyHeader));
        RtlCopyMemory(&reply->Message, Message, Message->Header.Size);

        status = PortReplyMessage(FltPortHandle, &reply->ReplyHeader, sizeof(reply->ReplyHeader) + Message->Header.Size);
        if (status == STATUS_PORT_DISCONNECTED) {

            //
            // Mark the port disconnected so IsConnected returns false.
            // This can happen if the driver goes away before the client.
            //
            PortDisconnected = TRUE;
        }

    Exit:

        if (reply)
            free(reply);
    
        return status;
    }

    BOOLEAN IsConnected()
    {
        return (FltPortHandle && !PortDisconnected);
    }


    FLTMGR_CALLBACK RegisteredCallback = NULL;
    PVOID RegisteredCallbackParam = NULL;

    HANDLE FltPortHandle = NULL;
    BOOLEAN PortDisconnected = TRUE;
    PTP_POOL ThreadPool = NULL;
    TP_CALLBACK_ENVIRON ThreadPoolEnv;
    PTP_IO ThreadPoolIo = NULL;
    ULONG MessageCount = 0;
    PH_RUNDOWN_PROTECT Rundown;
    PKPH_UMESSAGE Messages = NULL;


};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CFltPortClient
//

CFltPortClient::CFltPortClient(FLTMGR_CALLBACK Callback, PVOID Param)
{
    m = new SFltPortClient;

    m->RegisteredCallback = Callback;
    m->RegisteredCallbackParam = Param;
}

CFltPortClient::~CFltPortClient()
{
    delete m;
}

//bool CFltPortClient__Callback(CFltPortClient* This, uint32 msgId, const CBuffer* req, CBuffer* rpl)
//{
//    bool bOk = false;
//    for (auto I = This->m_EventHandlers.find(msgId); I != This->m_EventHandlers.end() && I->first == msgId; I++) {
//        if(NT_SUCCESS(I->second(msgId, req, rpl)))
//            bOk = true;
//        rpl = NULL; // only the first ergistered handler gets a chance to rply
//    }
//    return bOk;
//}
//
//extern "C" static VOID NTAPI KsiCommsCallback(
//    _In_ PCKPH_MESSAGE Message, 
//    _In_ PFLTMGR_CALLBACK_CONTEXT Context, 
//    _In_ FLTMGR_REPLYFUNC ReplyFunc
//    )
//{
//    CBuffer InBuff(Message->Data, Message->Header.Size - sizeof(Message->Header), true);
//
//    CBuffer* pOutBuff = NULL;
//    if (Message->Header.MessageId == KphMsgProcessCreate) 
//    {
//        pOutBuff = new CBuffer(sizeof(KPH_MESSAGE));
//        PMSG_HEADER reqHeader = (PMSG_HEADER)pOutBuff->GetBuffer();
//        reqHeader->MessageId = Message->Header.MessageId;
//        reqHeader->Size = sizeof(MSG_HEADER);
//    }
//
//    bool bOk = CFltPortClient__Callback((CFltPortClient*)Context->Client->RegisteredCallbackParam, Message->Header.MessageId, &InBuff, pOutBuff);
//
//    if (pOutBuff)
//    {
//        PKPH_MESSAGE msg = (PKPH_MESSAGE)pOutBuff->GetBuffer();
//        msg->Header.Size = (ULONG)pOutBuff->GetSize();
//
//        if(bOk)
//            ReplyFunc(Context, msg);
//
//        delete pOutBuff;
//    }
//}

STATUS CFltPortClient::Connect(const wchar_t* Name)
{
    NTSTATUS status = m->Start(Name);
    if(!NT_SUCCESS(status))
        return ERR(status);
    return OK;
}

void CFltPortClient::Disconnect()
{
    m->Stop();
}

bool CFltPortClient::IsConnected()
{
    return m->IsConnected();
}

STATUS CFltPortClient::Call(const CBuffer& inBuff, CBuffer& outBuff)
{
    if (!m->FltPortHandle)
        return ERR(STATUS_FLT_NOT_INITIALIZED);

    ULONG bytesReturned = 0;
    NTSTATUS status = m->PortSendMessage(m->FltPortHandle, (PVOID)inBuff.GetBuffer(), (ULONG)inBuff.GetSize(), outBuff.GetBuffer(), (ULONG)outBuff.GetCapacity(), &bytesReturned);
    if (!NT_SUCCESS(status))
        return ERR(status);
    outBuff.SetSize(bytesReturned);
    
    return OK;
}
