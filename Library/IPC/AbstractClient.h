#pragma once
#include "../Framework/Core/Types.h"
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/StVariant.h"
#include "PortMessage.h"

struct SCallParams
{
	ULONG TimeOut = 100*1000;
	size_t RetBufferSize = 0x1000;
};

class LIBRARY_EXPORT CAbstractClient
{
public:
    virtual ~CAbstractClient() {}

    virtual STATUS Connect(const wchar_t* Name) = 0;
	virtual void Disconnect() = 0;
	virtual bool IsConnected() = 0;

	virtual void SetAutoConnect(bool AutoConnect) { m_AutoConnect = AutoConnect; }

	virtual uint32 GetServerPID() const = 0;

    virtual STATUS Call(const CBuffer& inBuff, CBuffer& outBuff, SCallParams* pParams) = 0;
    virtual RESULT(StVariant) Call(uint32 MessageId, const StVariant& Message, SCallParams* pParams)
    {
	    CBuffer sendBuff(Message.Allocator());
	    sendBuff.WriteData(NULL, sizeof(MSG_HEADER)); // make room for header, pointer points after the header
		PMSG_HEADER reqHeader = (PMSG_HEADER)sendBuff.GetBuffer();
		reqHeader->MessageId = MessageId;
		reqHeader->Size = sizeof(MSG_HEADER);

	    Message.ToPacket(&sendBuff);
        ((PMSG_HEADER)sendBuff.GetBuffer())->Size = (ULONG)sendBuff.GetSize();

	    CBuffer recvBuff(Message.Allocator());
	    STATUS Status;
		PMSG_HEADER resHeader;
		size_t RetBufferSize = pParams ? pParams->RetBufferSize : 0x1000;

	retry:

		recvBuff.AllocBuffer(RetBufferSize);

		Status = Call(sendBuff, recvBuff, pParams);
		if (!Status || recvBuff.GetSize() == 0)
			return Status;

	    resHeader = (PMSG_HEADER)recvBuff.ReadData(sizeof(MSG_HEADER));

		if(resHeader->Status == STATUS_BUFFER_TOO_SMALL)
		{
			ULONG* pSize = (ULONG*)recvBuff.ReadData(sizeof(ULONG));
			RetBufferSize = 0x1000 + *pSize;
			goto retry;
		}

		if (!NT_SUCCESS(resHeader->Status))
			return ERR(resHeader->Status);

		StVariant Result(Message.Allocator());
		if(recvBuff.GetSizeLeft() > 0)
			Result.FromPacket(&recvBuff);

		return CResult<StVariant>(resHeader->Status, Result);
    }

	//virtual bool RegisterHandler(uint32 MessageId, const std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl)>& Handler) { return false; }

protected:
	bool m_AutoConnect = false;
};