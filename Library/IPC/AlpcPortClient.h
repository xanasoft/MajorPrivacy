#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/StVariant.h"
#include "AbstractClient.h"

class LIBRARY_EXPORT CAlpcPortClient : public CAbstractClient
{
public:
    CAlpcPortClient();
    virtual ~CAlpcPortClient();

    virtual STATUS Connect(const wchar_t* Name);
    virtual void Disconnect();
    virtual bool IsConnected();

    uint32 GetServerPID() const override;

    virtual STATUS Call(const CBuffer& inBuff, CBuffer& outBuff);

protected:
    virtual STATUS Connect();

	struct SAlpcPortClient* m;
};