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

#ifndef _DEBUG
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

#if MAX_PORTMSG_LENGTH > 648
    ALPC_PORT_ATTRIBUTES serverPortAttr = { 0 };
    serverPortAttr.MaxMessageLength = MAX_PORTMSG_LENGTH;
    status = NtAlpcCreatePort((HANDLE*)&m_hServerPort, SNtObject(name.c_str(), NULL, sd).Get(), &serverPortAttr);
#else
    status = NtCreatePort((HANDLE *)&m_hServerPort, SNtObject(name.c_str(), NULL, sd).Get(), 0, MAX_PORTMSG_LENGTH, NULL);
#endif

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
        // wake up workers and shutdown
        //

        UCHAR space[MAX_PORTMSG_LENGTH];
        for (ULONG i = 0; i < m_NumThreads; ++i)
        {
            PORT_MESSAGE* msg = (PORT_MESSAGE*)space;
            memset(msg, 0, MAX_PORTMSG_LENGTH);
            msg->u1.s1.TotalLength = (USHORT)sizeof(PORT_MESSAGE);
            NtRequestPort(PortHandle, msg);
        }
    }

    if (WaitForMultipleObjects((DWORD)m_Threads.size(), m_Threads.data(), TRUE, 5000) == WAIT_TIMEOUT)
    {
        //
        // termiate not yet finished workers
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
    PORT_MESSAGE *msg = (PORT_MESSAGE *)space;
    HANDLE hReplyPort;
    PORT_MESSAGE *ReplyMsg;

    //
    // initially we have no reply to send.  we will also revert to
    // this no-reply state after each reply has been sent
    //

    hReplyPort = m_hServerPort;
    ReplyMsg = NULL;

    while (1) {

        //
        // send the outgoing reply in ReplyMsg, if any, to the port in
        // hReplyPort.  then wait for an incoming message.  note that even
        // if hReplyPort indicates a client port to send the message, the
        // server-side NtReplyWaitReceivePort will listen on the associated
        // server port, after the message has been sent.
        //

        if (ReplyMsg) {

            memcpy(spaceReply, ReplyMsg, ReplyMsg->u1.s1.TotalLength);
            ReplyMsg = (PORT_MESSAGE *)spaceReply;
        }

        status = NtReplyWaitReceivePort(hReplyPort, NULL, ReplyMsg, msg);

        if (! m_hServerPort)    // service is shutting down
            break;

        if (ReplyMsg) {

            hReplyPort = m_hServerPort;
            ReplyMsg = NULL;

            if (! NT_SUCCESS(status))
                continue;       // ignore errors on client port

        } else if (! NT_SUCCESS(status)) {

            if (status == STATUS_UNSUCCESSFUL) {
                // can be considered a warning rather than an error
                continue;
            }
            break;              // abort on errors on server port
        }

        if (msg->u2.s2.Type == LPC_CONNECTION_REQUEST) {

            PortConnect(msg);

        } else if (msg->u2.s2.Type == LPC_REQUEST) {

            SThreadPtr client = PortFindClient(msg);
            if (! client)
                continue;

            if (! client->replying)
                PortRequest(client->hPort, msg, client);

            msg->u2.ZeroInit = 0;

            if (client->replying)
                PortReply(msg, client);
            else {
                msg->u1.s1.DataLength = (USHORT) 0;
                msg->u1.s1.TotalLength = sizeof(PORT_MESSAGE);
            }

            hReplyPort = client->hPort;
            ReplyMsg = msg;

            client->in_use = FALSE;

        } else if (msg->u2.s2.Type == LPC_PORT_CLOSED ||
                   msg->u2.s2.Type == LPC_CLIENT_DIED) {

            PortDisconnect(msg);
        }
    }
}

void CAlpcPortServer::PortConnect(PORT_MESSAGE *msg)
{
    NTSTATUS status;
    SProcessPtr clientProcess;
    SThreadPtr clientThread;

    //
    // find a previous connection to that same client, or create a new one
    //

    CSectionLock Lock(&m_Lock);

    PortFindClientUnsafe(msg->ClientId, clientProcess, clientThread);

    //
    // create new process and thread structures where needed
    //

    if (! clientProcess) {

        clientProcess = SProcessPtr(new SProcess);


            clientProcess->idProcess = msg->ClientId.UniqueProcess;
            m_Clients[msg->ClientId.UniqueProcess] = clientProcess;


            //
            // prepare for the case where a disconnect message only
            // specifies process creation time:  record the process
            // creation time, so it can be used later
            //

            clientProcess->CreateTime.HighPart = 0;
            clientProcess->CreateTime.LowPart = 0;
            HANDLE hProcess = OpenProcess(
                PROCESS_QUERY_INFORMATION, FALSE,
                (ULONG)(ULONG_PTR)msg->ClientId.UniqueProcess);
            if (hProcess) {
                FILETIME time, time1, time2, time3;
                BOOL ok = GetProcessTimes(
                    hProcess, &time, &time1, &time2, &time3);
                if (ok) {
                    clientProcess->CreateTime.HighPart = time.dwHighDateTime;
                    clientProcess->CreateTime.LowPart  = time.dwLowDateTime;
                }
                CloseHandle(hProcess);
            }
    }

    if (clientProcess && (! clientThread)) {

        clientThread = SThreadPtr(new SThread);

        clientThread->idThread = msg->ClientId.UniqueThread;
        clientProcess->Threads[msg->ClientId.UniqueThread] = clientThread;
    }

    //
    // if we couldn't create a new connection (not enough memory)
    // reject the new connection
    //

    if (! clientThread) {

        HANDLE hPort;
        NtAcceptConnectPort(&hPort, NULL, msg, FALSE, NULL, NULL);

        return;
    }

    //
    // if a previous connection was found, close it
    //

    if (clientThread->hPort) {

        while (clientThread->in_use)
            Sleep(3);

        NtClose(clientThread->hPort);
        //if (clientThread->buf_hdr)
        //    FreeMsg(clientThread->buf_hdr);

        clientThread->replying = FALSE;
        clientThread->in_use = FALSE;
        clientThread->sequence = 0;
        clientThread->hPort = NULL;
        //clientThread->buf_hdr = NULL;
        //clientThread->buf_ptr = NULL;
        clientThread->recvBuff.SetSize(0);
        clientThread->sendBuff.SetSize(0);
    }

    //
    // if a new client structure was created, accept the connection
    //

    status = NtAcceptConnectPort(&clientThread->hPort, NULL, msg, TRUE, NULL, NULL);
    if (NT_SUCCESS(status))
        status = NtCompleteConnectPort(clientThread->hPort);
}

void CAlpcPortServer::PortDisconnectHelper(const SProcessPtr& clientProcess, const SThreadPtr& clientThread)
{
    if (!clientProcess)
        return;

    if (clientThread) 
    {
        while (clientThread->in_use)
            Sleep(3);

        clientProcess->Threads.erase(clientThread->idThread);
        NtClose(clientThread->hPort);
    }

    if (clientProcess->Threads.empty()) 
        m_Clients.erase(clientProcess->idProcess);
}

void CAlpcPortServer::PortDisconnect(PORT_MESSAGE *msg)
{
    SProcessPtr clientProcess;
    SThreadPtr clientThread;

    //
    // on Windows Vista, a LPC_PORT_CLOSED messages arrives with zero CID,
    // but includes a process timestamp
    //

    if ((! msg->ClientId.UniqueProcess) || (! msg->ClientId.UniqueThread)) 
    {
        if (msg->u1.s1.DataLength == 8)
            PortDisconnectByCreateTime((LARGE_INTEGER*)((UCHAR*)msg + sizeof(PORT_MESSAGE)));
        return;
    }

    //
    // find a previous connection to that same client
    //

    CSectionLock Lock(&m_Lock);

    PortFindClientUnsafe(msg->ClientId, clientProcess, clientThread);

    PortDisconnectHelper(clientProcess, clientThread);
}

void CAlpcPortServer::PortDisconnectByCreateTime(LARGE_INTEGER *CreateTime)
{
    typedef HANDLE (*P_GetProcessIdOfThread)(HANDLE Thread);

    //
    // find the process id by its creation timestamp
    //

    /*WCHAR txt[128];
    wsprintf(txt, L"Message has no CID but has timestamp %08X-%08X", CreateTime->HighPart, CreateTime->LowPart);
    OutputDebugString(txt);*/

    CSectionLock Lock(&m_Lock);

    SProcessPtr clientProcess = NULL;
    SThreadPtr  clientThread = NULL;
    for (auto I = m_Clients.begin(); I != m_Clients.end(); I++){

        clientProcess = I->second;

        if (clientProcess->CreateTime.HighPart == CreateTime->HighPart &&
            clientProcess->CreateTime.LowPart  == CreateTime->LowPart) {

            for (auto J = clientProcess->Threads.begin(); J != clientProcess->Threads.end(); J++){

                clientThread = J->second;

                //
                // for each thread in the process, assume it is stale,
                // unless we can open it, and it still has the same
                // process id
                //

                BOOLEAN DeleteThread = TRUE;

                HANDLE hThread = OpenThread(
                    THREAD_QUERY_INFORMATION, FALSE,
                    (ULONG)(ULONG_PTR)clientThread->idThread);
                if (hThread) {
                    HANDLE ThreadProcessId = (HANDLE)(UINT_PTR)GetProcessIdOfThread(hThread);
                    if (ThreadProcessId == clientProcess->idProcess)
                        DeleteThread = FALSE;
                    CloseHandle(hThread);
                }

                //
                // fix-me: when closing the port without waiting some ms after the 
                //          thread terminated this fails and the client object is not cleared
                //

                if (DeleteThread) {

                    PortDisconnectHelper(clientProcess, clientThread);

                    break;
                }
            }

            break;
        }
    }
}

void CAlpcPortServer::PortRequest(HANDLE PortHandle, PORT_MESSAGE *msg, const SThreadPtr& client)
{
    if (client->recvBuff.GetSize() == 0) // first segment of new message
    {
        ULONG* msg_Data = (ULONG*)((UCHAR*)msg + sizeof(PORT_MESSAGE));
        ULONG msgid = msg_Data[1];

        client->sequence = ((UCHAR *)msg_Data)[3];
        ((UCHAR *)msg_Data)[3] = 0;

        ULONG TotalSize = msg_Data[0];
        //if (msgid && TotalSize &&
        if (TotalSize &&
            //TotalSize < MAX_REQUEST_LENGTH &&
            TotalSize >= sizeof(MSG_HEADER) &&
            TotalSize >= (ULONG)msg->u1.s1.DataLength)
        {
            client->recvBuff.SetSize(0, true, TotalSize);
        }
        else
        {
            client->sequence = 0;
            goto finish;
        }

    } 
    else // subsequent segment of pending message
    {
        if ((ULONG)client->recvBuff.GetSize() + msg->u1.s1.DataLength > ((MSG_HEADER*)client->recvBuff.GetBuffer())->Size)
            goto finish;
    }

    client->recvBuff.AppendData((UCHAR*)msg + sizeof(PORT_MESSAGE), msg->u1.s1.DataLength);

    if ((ULONG)client->recvBuff.GetSize() < ((MSG_HEADER*)client->recvBuff.GetBuffer())->Size)
        return; // packet is not yet complete

    client->sendBuff.SetSize(0);
    CallTarget(client->recvBuff, client->sendBuff, PortHandle, msg);
    client->recvBuff.SetSize(0);
    client->sendBuff.SetPosition(0);

finish:
    client->replying = TRUE;
}

void CAlpcPortServer::PortFindClientUnsafe(const CLIENT_ID& ClientId, SProcessPtr &clientProcess, SThreadPtr &clientThread)
{
    //
    // Note: this is not thread safe, you must lock m_lock before calling this function
    //

    auto I = m_Clients.find(ClientId.UniqueProcess);
    if(I == m_Clients.end())
        return;
    clientProcess = I->second;

    auto J = clientProcess->Threads.find(ClientId.UniqueThread);
    if(J == clientProcess->Threads.end())
        return;
    clientThread = J->second;
}

CAlpcPortServer::SThreadPtr CAlpcPortServer::PortFindClient(PORT_MESSAGE *msg)
{
    SProcessPtr clientProcess;
    SThreadPtr clientThread;

    CSectionLock Lock(&m_Lock);

    PortFindClientUnsafe(msg->ClientId, clientProcess, clientThread);

    if (clientThread)
        clientThread->in_use = TRUE;

    return clientThread;
}


void CAlpcPortServer::CallTarget(const CBuffer& recvBuff, CBuffer& sendBuff, HANDLE PortHandle, PORT_MESSAGE *PortMessage)
{
    NTSTATUS status;

    uint32 PID = (uint32)(ULONG_PTR)PortMessage->ClientId.UniqueProcess;
    uint32 TID = (uint32)(ULONG_PTR)PortMessage->ClientId.UniqueThread;

    sendBuff.WriteData(NULL, sizeof(MSG_HEADER)); // make room for header, pointer points after the header
//#ifndef _DEBUG
    try
//#endif
    {
        status = STATUS_INVALID_SYSTEM_SERVICE;

        MSG_HEADER* in_msg = (MSG_HEADER*)recvBuff.ReadData(sizeof(MSG_HEADER));

        if (in_msg->MessageId == -1)
        {
            DWORD pid = GetCurrentProcessId();
			sendBuff.WriteValue<uint32>(pid);
            status = STATUS_SUCCESS;
        }
        else
        {
            auto I = m_MessageHandlers.find(in_msg->MessageId);
            if (I != m_MessageHandlers.end())
                status = I->second(in_msg->MessageId, &recvBuff, &sendBuff, PID, TID);
        }

    }
//#ifndef _DEBUG
    catch (const CException&)
    {
        status = STATUS_INVALID_PARAMETER;
    }
//#endif
    MSG_HEADER* out_msg = (MSG_HEADER*)sendBuff.GetBuffer();
    out_msg->Size = (ULONG)sendBuff.GetSize();
    out_msg->Status = status;
    
    RevertToSelf();
}

void CAlpcPortServer::PortReply(PORT_MESSAGE *msg, const SThreadPtr& client)
{
    if (client->sendBuff.GetSize() == 0) {
        msg->u1.s1.DataLength = (USHORT) 0;
        msg->u1.s1.TotalLength = sizeof(PORT_MESSAGE);
        client->replying = FALSE;
        return;
    }

    SIZE_T len_togo = client->sendBuff.GetSizeLeft();
    if (len_togo > MSG_DATA_LEN)
        len_togo = MSG_DATA_LEN;

    msg->u1.s1.DataLength = (USHORT) len_togo;
    msg->u1.s1.TotalLength = (USHORT)(sizeof(PORT_MESSAGE) + len_togo);
    const byte* ptr = client->sendBuff.ReadData(len_togo);
    if (!ptr) return;
    memcpy((UCHAR*)msg + sizeof(PORT_MESSAGE), ptr, len_togo);

    if (client->sendBuff.GetPosition() == len_togo) // first segment
        ((UCHAR*)msg + sizeof(PORT_MESSAGE))[3] = client->sequence;

    if (client->sendBuff.GetPosition() >= ((MSG_HEADER*)client->sendBuff.GetBuffer())->Size) 
    {
        client->sendBuff.SetSize(0);
        client->replying = FALSE;
    }
}