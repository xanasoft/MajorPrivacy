#pragma once
#include "Types.h"
#include "Status.h"
#include "../lib_global.h"
#include "../Common/Variant.h"
#include "PortMessage.h"

class LIBRARY_EXPORT CAbstractClient
{
public:
    virtual ~CAbstractClient() {}

    virtual STATUS Connect(const wchar_t* Name) = 0;
	virtual void Disconnect() = 0;
	virtual bool IsConnected() = 0;

    virtual STATUS Call(const CBuffer& inBuff, CBuffer& outBuff) = 0;
    virtual RESULT(CVariant) Call(uint32 MessageId, const CVariant& Message, size_t RetBufferSize = 0x1000)
    {
	    CBuffer sendBuff;
	    sendBuff.SetData(NULL, sizeof(MSG_HEADER)); // make room for header, pointer points after the header
		PMSG_HEADER reqHeader = (PMSG_HEADER)sendBuff.GetBuffer();
		reqHeader->MessageId = MessageId;
		reqHeader->Size = sizeof(MSG_HEADER);

	    Message.ToPacket(&sendBuff);
        ((PMSG_HEADER)sendBuff.GetBuffer())->Size = (ULONG)sendBuff.GetSize();

	    CBuffer recvBuff;
	    STATUS Status;
		PMSG_HEADER resHeader;

	retry:

		recvBuff.AllocBuffer(RetBufferSize);

		Status = Call(sendBuff, recvBuff);
		if (!Status || recvBuff.GetSize() == 0)
			return Status;

	    resHeader = (PMSG_HEADER)recvBuff.GetData(sizeof(MSG_HEADER));

		if(resHeader->Status == STATUS_BUFFER_TOO_SMALL)
		{
			ULONG* pSize = (ULONG*)recvBuff.GetData(sizeof(ULONG));
			RetBufferSize = 0x1000 + *pSize;
			goto retry;
		}

		if (!NT_SUCCESS(resHeader->Status))
			return ERR(resHeader->Status);

	    CVariant Result;
		if(recvBuff.GetSizeLeft() > 0)
			Result.FromPacket(&recvBuff);

		return CResult<CVariant>(resHeader->Status, Result);
    }

	//virtual bool RegisterHandler(uint32 MessageId, const std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl)>& Handler) { return false; }
};