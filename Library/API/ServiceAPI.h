#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/StVariant.h"
#include "../Service/Network/Firewall/FirewallDefs.h"
#include "../Crypto/KeyExchange.h"

class LIBRARY_EXPORT CServiceAPI
{
public:
	CServiceAPI();
	virtual ~CServiceAPI();

	static STATUS InstallSvc(bool bAutoStart);
	STATUS ConnectSvc(bool bCanInstall = false);
	STATUS ConnectEngine(bool bCanStart = false);
	STATUS ReConnect();
	STATUS Connect();
	void Disconnect();
	bool IsConnected();

	uint32 GetProcessId() const;

	RESULT(StVariant) Call(uint32 MessageId, const StVariant& Message, struct SCallParams* pParams = NULL);

	uint32 GetABIVersion();

	const CBuffer& GetSharedSecret() const { return m_SharedSecret; }

	uint32 GetConfigStatus();
	STATUS StoreConfigChanges();
	STATUS DiscardConfigChanges();

	STATUS NegotiateKey();
	STATUS GetSharedSecret(CBuffer& SharedSecret) const;

	bool RegisterEventHandler(uint32 MessageId, const std::function<void(uint32 msgId, const CBuffer* pEvent)>& Handler);
    template<typename T, class C>
    bool RegisterEventHandler(uint32 MessageId, T Handler, C This) { return RegisterEventHandler(MessageId, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2)); }

protected:
	friend void CServiceAPI__EmitEvent(CServiceAPI* This, const CBuffer* pEvent);

	ULONG GetHeaderSize() const;

	std::unordered_map<uint32, std::function<void(uint32 msgId, const CBuffer* pEvent)>> m_EventHandlers;
	std::mutex m_EventHandlersMutex;

private:
	std::mutex m_CallMutex;
	class CAbstractClient* m_pClient;

	CBuffer m_SharedSecret;
};

