#include "pch.h"

#include <ph.h>

#include <objbase.h>
#include <Sddl.h>

#include "PipeServer.h"
#include "../Helpers/NtObj.h"
#include "../Helpers/Scoped.h"
#include "../Common/Exception.h"
#include "PortMessage.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPipeServer
//

CPipeServer::CPipeServer()
{
	InitializeCriticalSectionAndSpinCount(&m_Lock, 1000);

	m_hListener = NULL;
}

CPipeServer::~CPipeServer()
{
    HANDLE hListener = InterlockedExchangePointer(&m_hListener, NULL);

    if (hListener)
    {
        if (WaitForSingleObject(hListener, 5000) == WAIT_TIMEOUT)
            TerminateThread(hListener, 0);
    }

    std::vector<HANDLE> Threads;

	EnterCriticalSection(&m_Lock);
    std::unordered_map<HANDLE, SPipeClientPtr> PipeClients = m_PipeClients;
    for (auto I : PipeClients) {
        Threads.push_back(I.first);
        HANDLE hPipe = InterlockedExchangePointer(&I.second->pPipe->hPipe, NULL);
        NtClose(hPipe);
    }
    LeaveCriticalSection(&m_Lock);

    if (WaitForMultipleObjects((DWORD)Threads.size(), Threads.data(), TRUE, 5000) == WAIT_TIMEOUT)
    {
        //
        // termiate not yet finished workers
        //
            
        for (DWORD i = 0; i < (DWORD)Threads.size(); ++i)
            TerminateThread(Threads[i], 0);
    }

    if (hListener) 
        NtClose(hListener);

	DeleteCriticalSection(&m_Lock);
}

STATUS CPipeServer::Open(const std::wstring& name)
{
    STATUS Status = OK;

    m_PipeName = name;

    m_ListeningPipes.resize(PhSystemBasicInformation.NumberOfProcessors);
    for (ULONG i = 0; i < PhSystemBasicInformation.NumberOfProcessors; ++i) 
    {
        auto res = MakePipe();
        if (!res) {
            Status = res;
            break;
        }
        m_ListeningPipes[i] = res.GetValue();
    }

    if (Status) {
        DWORD idThread;
        m_hListener = CreateThread(NULL, 0, ServerThreadStub, this, 0, &idThread);
        if(!m_hListener)
            Status = ERR(PhGetLastWin32ErrorAsNtStatus());
    }

    return Status;
}

RESULT(PPipeSocket) CPipeServer::MakePipe()
{
    PPipeSocket pPile = std::make_shared<SPipeSocket>();
    if(!pPile->olRead.hEvent || !pPile->olWrite.hEvent)
        return ERR(PhGetLastWin32ErrorAsNtStatus());

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    ConvertStringSecurityDescriptorToSecurityDescriptor(
        L"D:(A;OICI;GRGW;;;WD)", // SDDL string
        SDDL_REVISION_1,
        &(sa.lpSecurityDescriptor),
        NULL);

    pPile->hPipe = CreateNamedPipe(
        m_PipeName.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        PIPE_BUFSIZE * 16,
        PIPE_BUFSIZE * 16,
        0,
        &sa
    );

    // SetSecurityInfo(pPile->hPipe, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL);

    if (pPile->hPipe == INVALID_HANDLE_VALUE)
        return ERR(PhGetLastWin32ErrorAsNtStatus());

    ConnectNamedPipe(pPile->hPipe, &pPile->olRead);

    RETURN(pPile);
}

DWORD __stdcall CPipeServer::ServerThreadStub(void* param)
{
#ifdef _DEBUG
    SetThreadDescription(GetCurrentThread(), L"CPipeServer::ServerThreadStub");
#endif

    ((CPipeServer *)param)->RunServerThread();

    return 0;
}

void CPipeServer::RunServerThread()
{
    std::vector<HANDLE> Events;
    std::transform(m_ListeningPipes.begin(), m_ListeningPipes.end(), std::back_inserter(Events), [](auto I) { return I->olRead.hEvent; });

    for (;;)
    {
        DWORD dwWait = WaitForMultipleObjects((DWORD)Events.size(), Events.data(), FALSE, 1000);

        if (!m_hListener) break;

        if (dwWait >= WAIT_OBJECT_0 && dwWait < WAIT_OBJECT_0 + Events.size()) 
        {
            int index = dwWait - WAIT_OBJECT_0;

            PPipeSocket& pPipe = m_ListeningPipes[index];

            ULONG ClientPid = -1; // todo: fix me when a process duplicates the clients pipe handle we won't notice that here
            GetNamedPipeClientProcessId(pPipe->hPipe, &ClientPid);

            ClientConnected(pPipe, ClientPid, -1);

            auto res = MakePipe();
            if (!res) {
                // todo handle error
                break;
            }
            pPipe = res.GetValue();
            Events[index] = pPipe->olRead.hEvent;
        }
    }
}

void CPipeServer::ClientConnected(const PPipeSocket& pPipe, uint32 PID, uint32 TID)
{
    SPipeClientPtr pClient = std::make_shared<SPipeClient>();
    pClient->Info.PID = PID;
    pClient->Info.TID = TID;
    pClient->pPipe = pPipe;
    pClient->pServer = this;

    DWORD idThread;
    pClient->hThread = CreateThread(NULL, 0, ClientThreadStub, pClient.get(), 0, &idThread);
    if (!m_hListener) {
        // todo handle error
        NtClose(pPipe->hPipe);
        return;
    }

    CSectionLock Lock(&m_Lock);
    m_PipeClients[pClient->hThread] = pClient;

    for(auto I : m_EventHandlers)
        I(eClientConnected, pClient->Info);
}

DWORD __stdcall CPipeServer::ClientThreadStub(void *param)
{
#ifdef _DEBUG
    SetThreadDescription(GetCurrentThread(), L"CPipeServer::ClientThreadStub");
#endif

    SPipeClient* pClient = (SPipeClient*)param;
    
    HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // run

    pClient->pServer->RunClientThread(pClient);

    // cleanup

    if (result == S_OK || result == S_FALSE)
        CoUninitialize();

    CSectionLock Lock(&pClient->pServer->m_Lock);

    for(auto I : pClient->pServer->m_EventHandlers)
        I(eClientDisconnected, pClient->Info);

    pClient->pServer->m_PipeClients.erase(pClient->hThread);

    return 0;
}

void CPipeServer::RunClientThread(SPipeClient* pClient)
{
    CBuffer recvBuff;
    CBuffer sendBuff;

    while (pClient->pPipe->hPipe)
    {
        // reset buffer and read
        recvBuff.SetSize(0);
        if(!pClient->pPipe->ReadPacket(recvBuff))
            break;


        // process request
        NTSTATUS status;
        sendBuff.SetData(NULL, sizeof(MSG_HEADER)); // make room for header, pointer points after the header
//#ifndef _DEBUG
        try
//#endif
        {
            status = STATUS_INVALID_SYSTEM_SERVICE;

            MSG_HEADER* in_msg = (MSG_HEADER*)recvBuff.ReadData(sizeof(MSG_HEADER));

            auto I = m_MessageHandlers.find(in_msg->MessageId);
            if (I != m_MessageHandlers.end())
                status = I->second(in_msg->MessageId, &recvBuff, &sendBuff, pClient->Info);

        }
//#ifndef _DEBUG
        catch (const CException&)
        {
            status = STATUS_INVALID_PARAMETER;
        }
//#endif
        MSG_HEADER* out_msg = (MSG_HEADER*)sendBuff.GetBuffer();
        out_msg->Flag = 0;
        out_msg->Size = (ULONG)sendBuff.GetSize();
        out_msg->Status = status;

        
        // send reply and reset buffer
        CSectionLock Lock(&pClient->WriteLock);
        pClient->pPipe->WritePacket(sendBuff);
        sendBuff.SetSize(0);
    }
}

int CPipeServer::BroadcastMessage(uint32 msgId, const CBuffer* msg, uint32 PID, uint32 TID)
{
    CBuffer sendBuff(sizeof(MSG_HEADER) + msg->GetSize());

    MSG_HEADER* out_msg = (MSG_HEADER*)sendBuff.SetData(NULL, sizeof(MSG_HEADER));
    out_msg->Flag = 1;
    out_msg->Size = (ULONG)sendBuff.GetSize();
    out_msg->MessageId = msgId;

    sendBuff.WriteData(msg->GetBuffer(), msg->GetSize());

    //
    // Broadcast the message to all conencted clients
    //

	CSectionLock Lock(&m_Lock);

    int Success = 0;

    for (auto I : m_PipeClients) 
    {
        if((PID != -1 && I.second->Info.PID != PID) || (TID != -1 && I.second->Info.TID != TID))
			continue;
        CSectionLock Lock(&I.second->WriteLock);
        if (I.second->pPipe->WritePacket(sendBuff))
            Success++;
    }

    return Success;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SPipeSocket
//

SPipeSocket::SPipeSocket()
{
    olRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    olWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

SPipeSocket::~SPipeSocket()
{
    if (hPipe) 
        NtClose(hPipe);

    if (olRead.hEvent) 
        NtClose(olRead.hEvent);
    if (olWrite.hEvent) 
        NtClose(olWrite.hEvent);
}

STATUS SPipeSocket::ReadPacket(CBuffer& recvBuff)
{
    do
    {
        ResetEvent(olRead.hEvent);

        if (recvBuff.GetCapacityLeft() < PIPE_BUFSIZE)
            recvBuff.SetSize(recvBuff.GetSize(), true, max(PIPE_BUFSIZE, recvBuff.GetCapacityLeft() + recvBuff.GetCapacity()));

        DWORD bytesRead;
        if (!ReadFile(hPipe, recvBuff.GetData(0), PIPE_BUFSIZE, &bytesRead, &olRead))
        {
            DWORD dwError = GetLastError();
            if (dwError == ERROR_IO_PENDING)
            {
                DWORD Ret = WaitForSingleObject(olRead.hEvent, INFINITE);
                if(Ret != WAIT_OBJECT_0)
					return ERR(PhDosErrorToNtStatus(dwError));
                if (!GetOverlappedResult(hPipe, &olRead, &bytesRead, FALSE))
                    return ERR(PhDosErrorToNtStatus(dwError));
            }
            else
                return ERR(PhDosErrorToNtStatus(dwError));
        }

        recvBuff.SetSize(recvBuff.GetSize() + bytesRead);
        recvBuff.SetPosition(recvBuff.GetSize());

    } while (recvBuff.GetSize() < sizeof(MSG_HEADER) || recvBuff.GetSize() < ((MSG_HEADER*)recvBuff.GetBuffer())->Size);

    recvBuff.SetPosition(0);

    return OK;
}

STATUS SPipeSocket::WritePacket(const CBuffer& sendBuff)
{
    sendBuff.SetPosition(0);

    do
    {
        ResetEvent(olWrite.hEvent);
            
        DWORD bytesToGo = min(PIPE_BUFSIZE, (DWORD)sendBuff.GetSizeLeft());

        DWORD bytesWritten;
        if (!WriteFile(hPipe, sendBuff.GetData(0), bytesToGo, &bytesWritten, &olWrite)) 
        {
            DWORD dwError = GetLastError();
            if (dwError == ERROR_IO_PENDING) 
            {
                WaitForSingleObject(olWrite.hEvent, 30*1000);
                if (!GetOverlappedResult(hPipe, &olWrite, &bytesWritten, FALSE))
                    return ERR(PhDosErrorToNtStatus(dwError));
            }
            else
                return ERR(PhDosErrorToNtStatus(dwError));
        }

        sendBuff.SetPosition(sendBuff.GetPosition() + bytesWritten);

    } while (sendBuff.GetSizeLeft() > 0);
    
    return OK;
}