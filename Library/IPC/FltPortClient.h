#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/StVariant.h"
#include "AbstractClient.h"

typedef struct _FLTMGR_CALLBACK_CONTEXT {
    unsigned long long ReplyToken;
    struct SFltPortClient* Client;
	PVOID Param;
} FLTMGR_CALLBACK_CONTEXT, * PFLTMGR_CALLBACK_CONTEXT;

typedef NTSTATUS (FLTMGR_REPLYFUNC)(_In_ PFLTMGR_CALLBACK_CONTEXT Context, _In_ struct _KPH_MESSAGE* Message);

typedef VOID (NTAPI* FLTMGR_CALLBACK)(_In_ struct _KPH_MESSAGE* Message, _In_ PFLTMGR_CALLBACK_CONTEXT Context, _In_ FLTMGR_REPLYFUNC ReplyFunc);


class LIBRARY_EXPORT CFltPortClient: public CAbstractClient
{
public:
	CFltPortClient(FLTMGR_CALLBACK Callback, PVOID Param);
	virtual ~CFltPortClient();

	STATUS Connect(const wchar_t* Name);
	void Disconnect();
	bool IsConnected();

	uint32 GetServerPID() const override { return 4; } // return SYSTEM pid

	STATUS Call(const CBuffer& inBuff, CBuffer& outBuff);

//	bool RegisterHandler(uint32 MessageId, const std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl)>& Handler) { 
//		// todo: add section and lock
//		m_EventHandlers.insert(std::make_pair(MessageId, Handler)); 
//		return true;
//	}
//	template<typename T, class C>
//	bool RegisterHandler(uint32 MessageId, T Handler, C This) { return RegisterHandler(MessageId, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)); }
//
//protected:
//	friend bool CFltPortClient__Callback(CFltPortClient* This, uint32 msgId, const CBuffer* req, CBuffer* rpl);
//
//	std::unordered_multimap<uint32, std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl)>> m_EventHandlers;

private:
	struct SFltPortClient* m;
};

