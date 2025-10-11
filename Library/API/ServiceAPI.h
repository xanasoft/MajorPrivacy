#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/StVariant.h"
#include "../Service/Network/Firewall/FirewallDefs.h"

class LIBRARY_EXPORT CServiceAPI
{
public:
	CServiceAPI();
	virtual ~CServiceAPI();

	static STATUS InstallSvc(bool bAutoStart);
	STATUS ConnectSvc();
	STATUS ConnectEngine(bool bCanStart = false);
	void Disconnect();
	bool IsConnected();

	uint32 GetProcessId() const;

	RESULT(StVariant) Call(uint32 MessageId, const StVariant& Message, struct SCallParams* pParams = NULL);

	uint32 GetABIVersion();

	uint32 GetConfigStatus();
	STATUS StoreConfigChanges();
	STATUS DiscardConfigChanges();

	bool RegisterEventHandler(uint32 MessageId, const std::function<void(uint32 msgId, const CBuffer* pEvent)>& Handler);
    template<typename T, class C>
    bool RegisterEventHandler(uint32 MessageId, T Handler, C This) { return RegisterEventHandler(MessageId, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2)); }

protected:
	friend void CServiceAPI__EmitEvent(CServiceAPI* This, const CBuffer* pEvent);

	std::unordered_map<uint32, std::function<void(uint32 msgId, const CBuffer* pEvent)>> m_EventHandlers;
	std::mutex m_EventHandlersMutex;

private:
	std::mutex m_CallMutex;
	class CAbstractClient* m_pClient;
};

