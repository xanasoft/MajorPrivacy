#pragma once
#include "../Library/Status.h"
#include "../Library/Helpers/Scoped.h"
#include "../Library/Helpers/EvtUtil.h"

struct SWinFwGuardEvent
{
    enum class EType
    {
        Undefined = 0,
        Added = 1,
        Removed = 2,
        Changed = 3
    };

    EType Type = EType::Undefined;
    std::wstring ProfileChanged;
    std::wstring RuleId;
    std::wstring RuleName;
};

class CWindowsFwGuard: public CEventLogListener
{
public:
	CWindowsFwGuard();
	virtual ~CWindowsFwGuard();

	virtual STATUS Start();
	virtual void Stop();
    
	void RegisterHandler(const std::function<uint32(const SWinFwGuardEvent* pEvent)>& Handler) { 
		std::unique_lock<std::mutex> Lock(m_HandlersMutex);
		m_Handlers.push_back(Handler); 
	}
	template<typename T, class C>
    void RegisterHandler(T Handler, C This) { RegisterHandler(std::bind(Handler, This, std::placeholders::_1)); }

protected:
    
	virtual void OnEvent(EVT_HANDLE hEvent);

	uint32 m_OldAuditingMode;
    
	std::mutex m_HandlersMutex;
	std::vector<std::function<uint32(const SWinFwGuardEvent* pEvent)>> m_Handlers;
};