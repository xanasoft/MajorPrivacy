#pragma once
#include "../Library/Status.h"
#include "../Library/Helpers/Scoped.h"
#include "../Library/Helpers/EvtUtil.h"
#include "WindowsFirewall.h"
#include "../Library/Common/Address.h"

struct SWinFwLogEvent
{
	EFwActions Type = EFwActions::Undefined;
	std::wstring strDirection;
	EFwDirections Direction = EFwDirections::Unknown;

	uint64 ProcessId = -1;
	std::wstring ProcessFileName;

	EFwKnownProtocols ProtocolType = EFwKnownProtocols::Any;
	CAddress LocalAddress;
	uint16 LocalPort = 0;
	CAddress RemoteAddress;
	uint16 RemotePort = 0;

	uint64 TimeStamp = 0;
};

class CWindowsFwLog: public CEventLogListener
{
public:
	CWindowsFwLog();
	virtual ~CWindowsFwLog();

	static std::wstring GetXmlQuery();

	static bool ReadEvent(EVT_HANDLE hEvent, SWinFwLogEvent& Event);

	virtual STATUS Start(int AuditingMode);
	virtual STATUS UpdatePolicy(int AuditingMode);
	virtual uint32 GetCurrentPolicy();
	virtual void Stop();

	void RegisterHandler(const std::function<uint32(const SWinFwLogEvent* pEvent)>& Handler) { 
		std::unique_lock<std::mutex> Lock(m_HandlersMutex);
		m_Handlers.push_back(Handler); 
	}
	template<typename T, class C>
    void RegisterHandler(T Handler, C This) { RegisterHandler(std::bind(Handler, This, std::placeholders::_1)); }

protected:

	virtual void OnEvent(EVT_HANDLE hEvent);

	uint32 m_OldAuditingMode;

	std::mutex m_HandlersMutex;
	std::vector<std::function<uint32(const SWinFwLogEvent* pEvent)>> m_Handlers;
};