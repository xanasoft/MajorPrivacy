#include "pch.h"

#include <ph.h>

#include <objbase.h>

#include "AlpcPortServer.h"
#include "../Helpers/NtObj.h"
#include "../Helpers/Scoped.h"
#include "../Helpers/WinUtil.h"
#include "../Common/Exception.h"
#include "PortMessage.h"


CAlpcPortServer::CAlpcPortServer() 
{
	InitializeCriticalSectionAndSpinCount(&m_Lock, 1000);

	m_hServerPort = NULL;

#ifdef _DEBUG
    m_NumThreads = 2;
#else
	m_NumThreads = PhSystemBasicInformation.NumberOfProcessors;
#endif
}

CAlpcPortServer::~CAlpcPortServer()
{
    Close();

	DeleteCriticalSection(&m_Lock);
}

STATUS CAlpcPortServer::Open(const std::wstring& name)
{
    NTSTATUS status;

    //
    // create port and allow access to all, set NULL dacl
    //

    ULONG sd_space[16];
    memset(&sd_space, 0, sizeof(sd_space));
    PSECURITY_DESCRIPTOR sd = (PSECURITY_DESCRIPTOR)&sd_space;
    InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(sd, TRUE, NULL, FALSE);

    ALPC_PORT_ATTRIBUTES serverPortAttr = { 0 };
    serverPortAttr.Flags = ALPC_PORFLG_ALLOW_LPC_REQUESTS;  // Enable LPC compatibility for datagrams
    serverPortAttr.MaxMessageLength = MAX_PORTMSG_LENGTH;
    serverPortAttr.MemoryBandwidth = 0;
    serverPortAttr.MaxPoolUsage = (SIZE_T)-1;
    serverPortAttr.MaxSectionSize = 0;
    serverPortAttr.MaxViewSize = 0;
    serverPortAttr.MaxTotalSectionSize = 0;
    serverPortAttr.DupObjectTypes = 0;

    status = NtAlpcCreatePort((HANDLE*)&m_hServerPort, SNtObject(name.c_str(), NULL, sd).Get(), &serverPortAttr);

    if (! NT_SUCCESS(status))
        return ERR(status);

    InterlockedExchangePointer(&m_hServerPort, m_hServerPort);

    //
    // start worker threads
    //

    m_Threads.resize(m_NumThreads);
    for (ULONG i = 0; i < m_NumThreads; ++i)
    {
        DWORD idThread;
        m_Threads[i] = CreateThread(NULL, 0, ThreadStub, this, 0, &idThread);
        if (!m_Threads[i])
            return ERR(STATUS_UNSUCCESSFUL);
    }

	return OK;
}

void CAlpcPortServer::Close()
{
    HANDLE PortHandle = InterlockedExchangePointer(&m_hServerPort, NULL);

    if (PortHandle)
    {
        //
        // wake up workers and shutdown by sending empty ALPC messages
        //

        SIZE_T BufferLength = sizeof(PORT_MESSAGE);
        UCHAR space[sizeof(PORT_MESSAGE)];
        for (ULONG i = 0; i < m_NumThreads; ++i)
        {
            PORT_MESSAGE* msg = (PORT_MESSAGE*)space;
            memset(msg, 0, sizeof(PORT_MESSAGE));
            msg->u1.s1.TotalLength = (USHORT)sizeof(PORT_MESSAGE);
            msg->u1.s1.DataLength = 0;
            NtAlpcSendWaitReceivePort(PortHandle, 0, msg, NULL, NULL, NULL, NULL, NULL);
        }
    }

    if (WaitForMultipleObjects((DWORD)m_Threads.size(), m_Threads.data(), TRUE, 5000) == WAIT_TIMEOUT)
    {
        //
        // terminate not yet finished workers
        //

        for (DWORD i = 0; i < (DWORD)m_Threads.size(); ++i)
            TerminateThread(m_Threads[i], 0);
    }

    for (DWORD i = 0; i < (DWORD)m_Threads.size(); ++i)
        NtClose(m_Threads[i]);
    m_Threads.clear();

    if (PortHandle)
        NtClose(PortHandle);
}

DWORD __stdcall CAlpcPortServer::ThreadStub(void *param)
{
#ifdef _DEBUG
    MySetThreadDescription(GetCurrentThread(), L"CAlpcPortServer::ThreadStub");
#endif

    HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    ((CAlpcPortServer *)param)->RunThread();

    if (result == S_OK || result == S_FALSE)
        CoUninitialize();

    return 0;
}

void CAlpcPortServer::RunThread()
{
    NTSTATUS status;
    UCHAR space[MAX_PORTMSG_LENGTH], spaceReply[MAX_PORTMSG_LENGTH];
    PORT_MESSAGE* msg = (PORT_MESSAGE*)space;
    PORT_MESSAGE* ReplyMsg = NULL;
    PVOID PortContext = NULL;

    // Attributes for receiving PortContext (optional)
    SIZE_T attrSize = AlpcGetHeaderSize(ALPC_MESSAGE_CONTEXT_ATTRIBUTE);
    PALPC_MESSAGE_ATTRIBUTES recvMsgAttr = (PALPC_MESSAGE_ATTRIBUTES)alloca(attrSize); // alloc from stack
    status = AlpcInitializeMessageAttribute(ALPC_MESSAGE_CONTEXT_ATTRIBUTE, recvMsgAttr, attrSize, &attrSize);
    if (!NT_SUCCESS(status))
        return;

    while (1)
    {
        ULONG sendFlags = 0;

        if (ReplyMsg) 
        {
            memcpy(spaceReply, ReplyMsg, (USHORT)ReplyMsg->u1.s1.TotalLength);
            ReplyMsg = (PORT_MESSAGE*)spaceReply;

            // Reply + release the previously received request message in the same call.
            sendFlags = ALPC_MSGFLG_REPLY_MESSAGE | ALPC_MSGFLG_RELEASE_MESSAGE;
        }

        // Receive next message on the SERVER port
        SIZE_T BufferLength = MAX_PORTMSG_LENGTH;
        recvMsgAttr->ValidAttributes = 0;
        RtlZeroMemory(space, sizeof(space));

        status = NtAlpcSendWaitReceivePort(m_hServerPort, sendFlags, ReplyMsg, NULL, msg, &BufferLength, recvMsgAttr, NULL);

        if (!m_hServerPort)
            break;

        if (ReplyMsg) 
        {
            ReplyMsg = NULL;

            if (!NT_SUCCESS(status)) 
                continue;  // ignore errors when sending reply
        } 
        else if (!NT_SUCCESS(status)) 
        {
            if (status == STATUS_UNSUCCESSFUL)
                continue; // can be considered a warning rather than an error
            break;
        }

        // Extract PortContext if present
        PortContext = NULL;
        if (recvMsgAttr->ValidAttributes & ALPC_MESSAGE_CONTEXT_ATTRIBUTE) {
            PALPC_CONTEXT_ATTR contextAttr = (PALPC_CONTEXT_ATTR)AlpcGetMessageAttribute(recvMsgAttr, ALPC_MESSAGE_CONTEXT_ATTRIBUTE); // this returns an address within recvMsgAttr
            if (contextAttr)
                PortContext = contextAttr->PortContext;
        }

        USHORT msgType = msg->u2.s2.Type & 0xFF;

        if (msgType == LPC_CONNECTION_REQUEST) {

            PortConnect(msg);

            // No reply; must release notification
            NtAlpcSendWaitReceivePort(m_hServerPort, ALPC_MSGFLG_RELEASE_MESSAGE, msg, NULL, NULL, NULL, NULL, NULL);

        } else if (msgType == LPC_REQUEST) {

            SPortClientPtr pClient = PortFindClient(msg, PortContext);
            if (!pClient) {
                // No reply possible; release the received request
                NtAlpcSendWaitReceivePort(m_hServerPort, ALPC_MSGFLG_RELEASE_MESSAGE, msg, NULL, NULL, NULL, NULL, NULL);
                continue;
            }

            if (!pClient->replying)
                PortRequest(pClient->hPort, msg, pClient);

            // msg->u2.ZeroInit = 0;
            // Build reply in-place into msg (or call PortReply to fill it)
            msg->u2.s2.Type = LPC_REPLY;
            msg->u2.s2.DataInfoOffset = 0;

            if (pClient->replying)
                PortReply(msg, pClient);
            else {
                msg->u1.s1.DataLength = 0;
                msg->u1.s1.TotalLength = (USHORT)sizeof(PORT_MESSAGE);
            }

            // Schedule reply to be sent at top of next loop iteration.
            // The next NtAlpcSendWaitReceivePort() will send reply+release,
            // then receive the next message on m_hServerPort.
            ReplyMsg = msg;

        } else if (msgType == LPC_PORT_CLOSED || msgType == LPC_CLIENT_DIED) {

            PortDisconnect(msg, PortContext);

            // Notification; release it
            NtAlpcSendWaitReceivePort(m_hServerPort, ALPC_MSGFLG_RELEASE_MESSAGE, msg, NULL, NULL, NULL, NULL, NULL);

        } else {

            // Any other message types: release to avoid accumulation
            NtAlpcSendWaitReceivePort(m_hServerPort, ALPC_MSGFLG_RELEASE_MESSAGE, msg, NULL, NULL, NULL, NULL, NULL);
        }
    }
}

void CAlpcPortServer::PortConnect(PORT_MESSAGE *msg)
{
    SPortClientPtr pClient;

    {
        CSectionLock Lock(&m_Lock);

        pClient = SPortClientPtr(new SPortClient);

        pClient->Info.Ref = (ULONG_PTR)pClient.get();

        pClient->Info.PID = (ULONG)(ULONG_PTR)msg->ClientId.UniqueProcess;
        pClient->Info.TID = (ULONG)(ULONG_PTR)msg->ClientId.UniqueThread;
        ProcessIdToSessionId(pClient->Info.PID, &pClient->Info.SessionId);

        m_Clients[pClient.get()] = pClient;

        ALPC_PORT_ATTRIBUTES portAttr = { 0 };
        portAttr.Flags = ALPC_PORFLG_ALLOW_LPC_REQUESTS;  // Enable LPC compatibility for datagrams
        portAttr.MaxMessageLength = MAX_PORTMSG_LENGTH;
        portAttr.MemoryBandwidth = 0;
        portAttr.MaxPoolUsage = (SIZE_T)-1;
        portAttr.MaxSectionSize = 0;
        portAttr.MaxViewSize = 0;
        portAttr.MaxTotalSectionSize = 0;
        portAttr.DupObjectTypes = 0;

        // NtAlpcAcceptConnectPort combines accept and complete in one call
        // The last parameter (TRUE) indicates acceptance
        NTSTATUS status = NtAlpcAcceptConnectPort(&pClient->hPort, m_hServerPort, 0, NULL, &portAttr, pClient.get(), msg, NULL, TRUE);
        if (!NT_SUCCESS(status)) {
            m_Clients.erase(pClient.get());
            return;
        }
    }

    for(auto I : m_EventHandlers)
        I(eClientConnected, pClient->Info);
}

void CAlpcPortServer::PortDisconnect(PORT_MESSAGE* msg, PVOID context)
{
    SPortClientPtr pClient;

    {
        CSectionLock Lock(&m_Lock);

        auto F = m_Clients.find(context);
        if (F == m_Clients.end())
            return;

        pClient = F->second;
        m_Clients.erase(F);
    }

    for(auto I : m_EventHandlers)
        I(eClientDisconnected, pClient->Info);
}

void CAlpcPortServer::PortRequest(HANDLE PortHandle, PORT_MESSAGE *msg, const SPortClientPtr& pClient)
{
    pClient->Info.TID = (ULONG)(ULONG_PTR)msg->ClientId.UniqueThread; // this can change if a connection is used by another thread

    if (pClient->recvBuff.GetSize() == 0) // first segment of new message
    {
        ULONG* msg_Data = (ULONG*)((UCHAR*)msg + sizeof(PORT_MESSAGE));
        ULONG msgid = msg_Data[1];

        pClient->sequence = ((PORT_MSG_HEADER*)msg_Data)->Sequence;

        ULONG TotalSize = msg_Data[0];
        //if (msgid && TotalSize &&
        if (TotalSize &&
            //TotalSize < MAX_REQUEST_LENGTH &&
            TotalSize >= GetHeaderSize() &&
            TotalSize >= (USHORT)msg->u1.s1.DataLength)
        {
            pClient->recvBuff.SetSize(0, true, TotalSize);
        }
        else
        {
            pClient->sequence = 0;
            goto finish;
        }

    } 
    else // subsequent segment of pending message
    {
        if ((ULONG)pClient->recvBuff.GetSize() + (USHORT)msg->u1.s1.DataLength > ((PORT_MSG_HEADER*)pClient->recvBuff.GetBuffer())->h.Size)
            goto finish;
    }

    pClient->recvBuff.AppendData((UCHAR*)msg + sizeof(PORT_MESSAGE), (USHORT)msg->u1.s1.DataLength);

    if ((ULONG)pClient->recvBuff.GetSize() < ((PORT_MSG_HEADER*)pClient->recvBuff.GetBuffer())->h.Size)
        return; // packet is not yet complete

    pClient->sendBuff.SetSize(0);
    CallTarget(pClient->recvBuff, pClient->sendBuff, PortHandle, msg, pClient->Info);
    pClient->recvBuff.SetSize(0);
    pClient->sendBuff.SetPosition(0);

finish:
    pClient->replying = TRUE;
}

CAlpcPortServer::SPortClientPtr CAlpcPortServer::PortFindClient(PORT_MESSAGE *msg, PVOID context)
{
    CSectionLock Lock(&m_Lock);
    auto F = m_Clients.find(context);
    if (F == m_Clients.end())
        return nullptr;
    return F->second;
}

void CAlpcPortServer::CallTarget(const CBuffer& recvBuff, CBuffer& sendBuff, HANDLE PortHandle, PORT_MESSAGE *PortMessage,  const SClientInfo& Info)
{
    NTSTATUS status;

    uint32 PID = (uint32)(ULONG_PTR)PortMessage->ClientId.UniqueProcess;
    uint32 TID = (uint32)(ULONG_PTR)PortMessage->ClientId.UniqueThread;

    sendBuff.WriteData(NULL, GetHeaderSize()); // make room for header, pointer points after the header
//#ifndef _DEBUG
    try
//#endif
    {
        status = STATUS_INVALID_SYSTEM_SERVICE;

        PORT_MSG_HEADER* in_msg = (PORT_MSG_HEADER*)recvBuff.ReadData(GetHeaderSize());

        auto I = m_MessageHandlers.find(in_msg->h.MessageId);
        if (I != m_MessageHandlers.end())
            status = I->second(in_msg->h.MessageId, &recvBuff, &sendBuff, Info);

    }
//#ifndef _DEBUG
    catch (const CException&)
    {
        status = STATUS_INVALID_PARAMETER;
    }
//#endif
    PORT_MSG_HEADER* out_msg = (PORT_MSG_HEADER*)sendBuff.GetBuffer();
    out_msg->h.Size = (ULONG)sendBuff.GetSize();
    out_msg->h.Status = status;
    
    RevertToSelf();
}

void CAlpcPortServer::PortReply(PORT_MESSAGE *msg, const SPortClientPtr& pClient)
{
    if (pClient->sendBuff.GetSize() == 0) {
        msg->u1.s1.DataLength = 0;
        msg->u1.s1.TotalLength = (USHORT)sizeof(PORT_MESSAGE);
        pClient->replying = FALSE;
        return;
    }

    SIZE_T len_togo = pClient->sendBuff.GetSizeLeft();
    if (len_togo > MSG_DATA_LEN)
        len_togo = MSG_DATA_LEN;

    msg->u1.s1.DataLength = (USHORT)len_togo;
    msg->u1.s1.TotalLength = (USHORT)(sizeof(PORT_MESSAGE) + len_togo);
    const byte* ptr = pClient->sendBuff.ReadData(len_togo);
    if (!ptr) return;
    memcpy((UCHAR*)msg + sizeof(PORT_MESSAGE), ptr, len_togo);

    if (pClient->sendBuff.GetPosition() == len_togo) // first segment
        ((PORT_MSG_HEADER*)((UCHAR*)msg + sizeof(PORT_MESSAGE)))->Sequence = pClient->sequence;

    if (pClient->sendBuff.GetPosition() >= ((PORT_MSG_HEADER*)pClient->sendBuff.GetBuffer())->h.Size) 
    {
        pClient->sendBuff.SetSize(0);
        pClient->replying = FALSE;
    }
}

STATUS CAlpcPortServer::PortDatagram(const CBuffer& sendBuff, const SPortClientPtr& pClient)
{
    CSectionLock Lock(&pClient->WriteLock);

    UCHAR space[MAX_PORTMSG_LENGTH];
    PORT_MESSAGE* msg = (PORT_MESSAGE*)space;

    sendBuff.SetPosition(0);

    do
    {
        SIZE_T len_togo = sendBuff.GetSizeLeft();
        if (len_togo > MSG_DATA_LEN)
            len_togo = MSG_DATA_LEN;

        memset(msg, 0, sizeof(PORT_MESSAGE));
        msg->u1.s1.DataLength = (USHORT)len_togo;
        msg->u1.s1.TotalLength = (USHORT)(sizeof(PORT_MESSAGE) + len_togo);

        // datagram type (LPC compatibility)
        msg->u2.s2.Type = LPC_DATAGRAM;
        msg->u2.s2.DataInfoOffset = 0;

        const byte* ptr = sendBuff.ReadData(len_togo);
        if (!ptr) break;
        memcpy((UCHAR*)msg + sizeof(PORT_MESSAGE), ptr, len_togo);
        
        // Try sending through client's communication port with context
        NTSTATUS status = NtAlpcSendWaitReceivePort(pClient->hPort, 0, msg, NULL, NULL, NULL, NULL, NULL);
        if (!NT_SUCCESS(status))
            return ERR(status);

    } while (sendBuff.GetSizeLeft() > 0);

    return OK;
}

int CAlpcPortServer::BroadcastMessage(uint32 msgId, const CBuffer* msg, uint32 PID, uint32 TID)
{
    CBuffer sendBuff(GetHeaderSize() + msg->GetSize());

    PORT_MSG_HEADER* out_msg = (PORT_MSG_HEADER*)sendBuff.WriteData(NULL, GetHeaderSize());
    out_msg->h.Size = GetHeaderSize() + (ULONG)msg->GetSize();
    out_msg->h.MessageId = msgId;
	out_msg->Sequence = -1; // broadcast message

    sendBuff.WriteData(msg->GetBuffer(), msg->GetSize());

    //
    // Broadcast the message to all conencted clients
    //

    CSectionLock Lock(&m_Lock);

    int Success = 0;

    for (auto I : m_Clients)
    {
        if ((PID != -1 && I.second->Info.PID != PID) || (TID != -1 && I.second->Info.TID != TID))
            continue;
        if (PortDatagram(sendBuff, I.second) == OK)
            Success++;
    }

    return Success;
}