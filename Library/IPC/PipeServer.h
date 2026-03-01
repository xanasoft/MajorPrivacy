#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Framework/Common/Buffer.h"
#include "../Common/StVariant.h"
#include "AbstractServer.h"


#define PIPE_BUFSIZE 0x1000

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SPipeSocket
//

struct LIBRARY_EXPORT SPipeSocket
{
    SPipeSocket();
    ~SPipeSocket();

    STATUS ReadPacket(CBuffer& recvBuff);
    STATUS WritePacket(const CBuffer& sendBuff);

    volatile HANDLE hPipe = NULL;
    OVERLAPPED olRead = { 0 };
    OVERLAPPED olWrite = { 0 };
};
typedef std::shared_ptr<SPipeSocket> PPipeSocket;



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPipeServer
//

class LIBRARY_EXPORT CPipeServer: public CAbstractServer
{
public:
	CPipeServer();
	~CPipeServer();

    STATUS Open(const std::wstring& name) override;
    void Close() override;

    template<typename T, class C>
    void RegisterEventHandler(T Handler, C This) { CAbstractServer::RegisterEventHandler(std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2)); }

    template<typename T, class C>
    bool RegisterHandler(uint32 MessageId, T Handler, C This) { return CAbstractServer::RegisterHandler(MessageId, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); }

    int BroadcastMessage(uint32 msgId, const CBuffer* msg, uint32 PID = -1, uint32 TID = -1) override;

protected:

    ULONG GetHeaderSize() const override { return sizeof(PIPE_MSG_HEADER); }

    struct SPipeClient
    {
        SPipeClient() 
        {
            InitializeCriticalSectionAndSpinCount(&WriteLock, 1000);
        }
        ~SPipeClient()
        {
            if (hThread) 
                NtClose(hThread);
            
	        DeleteCriticalSection(&WriteLock);
        }

        HANDLE hThread = NULL;
        SClientInfo Info;
        PPipeSocket pPipe;
        CRITICAL_SECTION WriteLock;
        CPipeServer* pServer = NULL;
    };
    typedef std::shared_ptr<SPipeClient> SPipeClientPtr;

    static DWORD __stdcall ServerThreadStub(void* param);
    void RunServerThread();

    static DWORD __stdcall ClientThreadStub(void* param);
    void RunClientThread(SPipeClient* pClient);

    RESULT(PPipeSocket) MakePipe();

    void ClientConnected(const PPipeSocket& pPipe, uint32 PID, uint32 TID);

    CRITICAL_SECTION m_Lock;

    std::wstring m_PipeName;

    volatile HANDLE m_hListener;

    std::vector<PPipeSocket> m_ListeningPipes;
    std::unordered_map<HANDLE, SPipeClientPtr> m_PipeClients;
};

