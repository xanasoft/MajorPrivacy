#pragma once
#include "PortMessage.h"

struct LIBRARY_EXPORT SClientInfo
{
    uint64 Ref = 0;
    uint32 PID = 0;
    uint32 TID = 0;
    DWORD SessionId = -1;
    PVOID pContext = NULL;
};


class LIBRARY_EXPORT CAbstractServer
{
public:
    virtual ~CAbstractServer() {}

    virtual STATUS Open(const std::wstring& name) = 0;
    virtual void Close() = 0;

    enum EEvent
    {
        eClientConnected,
        eClientDisconnected,
    };

    // todo lock when registering events

    void RegisterEventHandler(const std::function<void(uint32 Event, SClientInfo& pClient)>& Handler) { 
        m_EventHandlers.push_back(Handler); 
    }

    bool RegisterHandler(uint32 MessageId, const std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl, const SClientInfo& pClient)>& Handler) { 
        return m_MessageHandlers.insert(std::make_pair(MessageId, Handler)).second; 
    }

    virtual int BroadcastMessage(uint32 msgId, const CBuffer* msg, uint32 PID = -1, uint32 TID = -1) = 0;

protected:

    virtual ULONG GetHeaderSize() const = 0;

    std::list<std::function<void(uint32 Event, SClientInfo& pClient)>> m_EventHandlers;

    std::unordered_map<uint32, std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl, const SClientInfo& pClient)>> m_MessageHandlers;
};