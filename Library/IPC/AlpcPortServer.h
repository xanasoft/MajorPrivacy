#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Framework/Common/Buffer.h"
#include "../Common/StVariant.h"
#include "AbstractServer.h"

class LIBRARY_EXPORT CAlpcPortServer: public CAbstractServer
{
public:
	CAlpcPortServer();
	~CAlpcPortServer();

    STATUS Open(const std::wstring& name) override;
    void Close() override;

    template<typename T, class C>
    void RegisterEventHandler(T Handler, C This) { CAbstractServer::RegisterEventHandler(std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2)); }

    template<typename T, class C>
    bool RegisterHandler(uint32 MessageId, T Handler, C This) { return CAbstractServer::RegisterHandler(MessageId, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); }

    int BroadcastMessage(uint32 msgId, const CBuffer* msg, uint32 PID = -1, uint32 TID = -1) override;

protected:

    ULONG GetHeaderSize() const override { return sizeof(PIPE_MSG_HEADER); }

    static DWORD __stdcall ThreadStub(void* param);
    void RunThread();

    struct SPortClient
    {
        SPortClient() 
        {
            InitializeCriticalSectionAndSpinCount(&WriteLock, 1000);
        }
        ~SPortClient()
        {
            DeleteCriticalSection(&WriteLock);
        }

        BOOLEAN replying = FALSE;
        ULONG sequence = 0;
        SClientInfo Info;
        HANDLE hPort = NULL;
        CBuffer recvBuff;
        CBuffer sendBuff;
        CRITICAL_SECTION WriteLock;
    };
    typedef std::shared_ptr<SPortClient> SPortClientPtr;


    void PortConnect(PORT_MESSAGE* msg);
    void PortDisconnect(PORT_MESSAGE* msg, PVOID context);
    void PortRequest(HANDLE PortHandle, PORT_MESSAGE* msg, const SPortClientPtr& voidClient);
    SPortClientPtr PortFindClient(PORT_MESSAGE* msg, PVOID context);
    void PortReply(PORT_MESSAGE* msg, const SPortClientPtr& pClient);
    void CallTarget(const CBuffer& recvBuff, CBuffer& sendBuff, HANDLE PortHandle, PORT_MESSAGE* PortMessage, const SClientInfo& Info);
    STATUS PortDatagram(const CBuffer& sendBuff, const SPortClientPtr& pClient);

    CRITICAL_SECTION m_Lock;
    std::unordered_map<PVOID, SPortClientPtr> m_Clients;
    
    volatile HANDLE m_hServerPort;
    std::vector<HANDLE> m_Threads;
    ULONG m_NumThreads = 1;
};


//#define MAX_PORTMSG_LENGTH 328    // Max port message length // should this be 648 ?
//#define MAX_PORTMSG_LENGTH 648
#define MAX_PORTMSG_LENGTH 0xFFFF   // For ALPC this can be max of 64KB
//#define MAX_REQUEST_LENGTH      (2048 * 1024)
#define MSG_DATA_LEN            (MAX_PORTMSG_LENGTH - sizeof(PORT_MESSAGE))