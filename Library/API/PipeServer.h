#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/Buffer.h"
#include "../Common/Variant.h"


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

class LIBRARY_EXPORT CPipeServer
{
public:
	CPipeServer();
	~CPipeServer();

    STATUS Open(const std::wstring& name);

    bool RegisterHandler(uint32 MessageId, const std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl, uint32 PID, uint32 TID)>& Handler) { 
        return m_MessageHandlers.insert(std::make_pair(MessageId, Handler)).second; 
    }
    template<typename T, class C>
    bool RegisterHandler(uint32 MessageId, T Handler, C This) { return RegisterHandler(MessageId, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)); }

    int BroadcastMessage(uint32 msgId, const CBuffer* msg);

protected:

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

    void ClientConnected(const PPipeSocket& pPipe);

    CRITICAL_SECTION m_Lock;

    std::wstring m_PipeName;

    volatile HANDLE m_hListener;

    std::vector<PPipeSocket> m_ListeningPipes;
    std::unordered_map<HANDLE, SPipeClientPtr> m_PipeClients;
    
    std::unordered_map<uint32, std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl, uint32 PID, uint32 TID)>> m_MessageHandlers;
};

