#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/Variant.h"
#include "../Service/Network/Firewall/FirewallDefs.h"

class LIBRARY_EXPORT CServiceAPI
{
public:
	CServiceAPI();
	virtual ~CServiceAPI();

	static STATUS InstallSvc();
	STATUS ConnectSvc();
	STATUS ConnectEngine();
	void Disconnect();
	bool IsConnected();

	RESULT(CVariant) Call(uint32 MessageId, const CVariant& Message);

	uint32 GetABIVersion();

	uint32 GetConfigStatus();
	STATUS CommitConfigChanges(const CBuffer& ConfigSignature = CBuffer());
	STATUS DiscardConfigChanges();

	bool RegisterEventHandler(uint32 MessageId, const std::function<void(uint32 msgId, const CBuffer* pEvent)>& Handler);
    template<typename T, class C>
    bool RegisterEventHandler(uint32 MessageId, T Handler, C This) { return RegisterEventHandler(MessageId, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2)); }

protected:
	friend void CServiceAPI__EmitEvent(CServiceAPI* This, const CBuffer* pEvent);

	std::unordered_map<uint32, std::function<void(uint32 msgId, const CBuffer* pEvent)>> m_EventHandlers;
	std::mutex m_EventHandlersMutex;

	HANDLE m_hEngineProcess = NULL;

private:
	class CAbstractClient* m_pClient;
};

