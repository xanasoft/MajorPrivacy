#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Framework/Common/Buffer.h"
#include "../Common/StVariant.h"

class LIBRARY_EXPORT CAlpcPortServer
{
public:
	CAlpcPortServer();
	~CAlpcPortServer();

    STATUS Open(const std::wstring& name);

    bool RegisterHandler(uint32 MessageId, const std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl, uint32 PID, uint32 TID)>& Handler) { 
        return m_MessageHandlers.insert(std::make_pair(MessageId, Handler)).second; 
    }
    template<typename T, class C>
    bool RegisterHandler(uint32 MessageId, T Handler, C This) { return RegisterHandler(MessageId, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)); }

protected:
    static DWORD __stdcall ThreadStub(void* param);
    void RunThread();

    struct SThread
    {
        HANDLE idThread = NULL;
        BOOLEAN replying = FALSE;
        volatile BOOLEAN in_use = FALSE;
        UCHAR sequence = 0;
        HANDLE hPort = NULL;
        CBuffer recvBuff;
        CBuffer sendBuff;
    };
    typedef std::shared_ptr<SThread> SThreadPtr;

	struct SProcess
    {
        HANDLE idProcess = NULL;
        LARGE_INTEGER CreateTime = {0};
        std::unordered_map<HANDLE, SThreadPtr> Threads;
    };
    typedef std::shared_ptr<SProcess> SProcessPtr;


    void PortConnect(PORT_MESSAGE* msg);
    void PortDisconnectHelper(const SProcessPtr& clientProcess, const SThreadPtr& clientThread);
    void PortDisconnect(PORT_MESSAGE* msg);
    void PortDisconnectByCreateTime(LARGE_INTEGER* CreateTime);
    void PortRequest(HANDLE PortHandle, PORT_MESSAGE* msg, const SThreadPtr& voidClient);
    void PortFindClientUnsafe(const CLIENT_ID& ClientId, SProcessPtr& clientProcess, SThreadPtr& clientThread);
    SThreadPtr PortFindClient(PORT_MESSAGE* msg);
    void PortReply(PORT_MESSAGE* msg, const SThreadPtr& voidClient);
    void CallTarget(const CBuffer& recvBuff, CBuffer& sendBuff, HANDLE PortHandle, PORT_MESSAGE* PortMessage);

    CRITICAL_SECTION m_Lock;
    std::unordered_map<HANDLE, SProcessPtr> m_Clients;
    
    volatile HANDLE m_hServerPort;
    std::vector<HANDLE> m_Threads;
    ULONG m_NumThreads = 1;

    std::unordered_map<uint32, std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl, uint32 PID, uint32 TID)>> m_MessageHandlers;
};


#define MAX_PORTMSG_LENGTH 328    // Max port message length // should this be 648 ?
//#define MAX_PORTMSG_LENGTH 0xFFFF   // For ALPC this can be max of 64KB
//#define MAX_REQUEST_LENGTH      (2048 * 1024)
#define MSG_DATA_LEN            (MAX_PORTMSG_LENGTH - sizeof(PORT_MESSAGE))