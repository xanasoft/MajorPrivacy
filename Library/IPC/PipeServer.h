#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Framework/Common/Buffer.h"
#include "../Common/StVariant.h"


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
// SPipeClientInfo
//

struct LIBRARY_EXPORT SPipeClientInfo
{
    uint32 PID = 0;
    uint32 TID = 0;
    PVOID pContext = NULL;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPipeServer
//

class LIBRARY_EXPORT CPipeServer
{
public:
	CPipeServer();
	~CPipeServer();

    enum EEvent
	{
		eClientConnected,
		eClientDisconnected,
	};

    STATUS Open(const std::wstring& name);
    void Close();

    // todo lock when registering events

    void RegisterEventHandler(const std::function<void(uint32 Event, SPipeClientInfo& pClient)>& Handler) { 
        m_EventHandlers.push_back(Handler); 
    }
    template<typename T, class C>
    void RegisterEventHandler(T Handler, C This) { RegisterEventHandler(std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2)); }

    bool RegisterHandler(uint32 MessageId, const std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl, const SPipeClientInfo& pClient)>& Handler) { 
        return m_MessageHandlers.insert(std::make_pair(MessageId, Handler)).second; 
    }
    template<typename T, class C>
    bool RegisterHandler(uint32 MessageId, T Handler, C This) { return RegisterHandler(MessageId, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); }

    int BroadcastMessage(uint32 msgId, const CBuffer* msg, uint32 PID = -1, uint32 TID = -1);

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
        SPipeClientInfo Info;
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

    std::list<std::function<void(uint32 Event, SPipeClientInfo& pClient)>> m_EventHandlers;
    std::unordered_map<uint32, std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl, const SPipeClientInfo& pClient)>> m_MessageHandlers;
};

