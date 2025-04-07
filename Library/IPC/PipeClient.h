#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/StVariant.h"
#include "AbstractClient.h"

class LIBRARY_EXPORT CPipeClient : public CAbstractClient
{
public:
    CPipeClient(VOID (NTAPI* Callback)(const CBuffer& buff, PVOID Param), PVOID Param);
    virtual ~CPipeClient();

    virtual STATUS Connect(const wchar_t* Name);
    virtual void Disconnect();
    virtual bool IsConnected();

    uint32 GetServerPID() const override;

    virtual STATUS Call(const CBuffer& inBuff, CBuffer& outBuff);

protected:
    friend struct SPipeClient;
    void RunThread();

    virtual STATUS Connect();

	struct SPipeClient* m;
};