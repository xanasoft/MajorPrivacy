#pragma once
#include "../Framework/Core/Types.h"
#include "Status.h"
#include "../lib_global.h"
#include "../Common/StVariant.h"
#include "PortMessage.h"

class LIBRARY_EXPORT CAbstractClient
{
public:
    virtual ~CAbstractClient() {}

    virtual STATUS Connect(const wchar_t* Name) = 0;
	virtual void Disconnect() = 0;
	virtual bool IsConnected() = 0;

	virtual void SetAutoConnect(bool AutoConnect) { m_AutoConnect = AutoConnect; }

	virtual uint32 GetServerPID() const = 0;

    virtual STATUS Call(const CBuffer& inBuff, CBuffer& outBuff) = 0;
    virtual RESULT(StVariant) Call(uint32 MessageId, const StVariant& Message, size_t RetBufferSize = 0x1000)
    {
	    CBuffer sendBuff;
	    sendBuff.WriteData(NULL, sizeof(MSG_HEADER)); // make room for header, pointer points after the header
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

	    resHeader = (PMSG_HEADER)recvBuff.ReadData(sizeof(MSG_HEADER));

		if(resHeader->Status == STATUS_BUFFER_TOO_SMALL)
		{
			ULONG* pSize = (ULONG*)recvBuff.ReadData(sizeof(ULONG));
			RetBufferSize = 0x1000 + *pSize;
			goto retry;
		}

		if (!NT_SUCCESS(resHeader->Status))
			return ERR(resHeader->Status);

		StVariant Result;
		if(recvBuff.GetSizeLeft() > 0)
			Result.FromPacket(&recvBuff);

		return CResult<StVariant>(resHeader->Status, Result);
    }

	//virtual bool RegisterHandler(uint32 MessageId, const std::function<uint32(uint32 msgId, const CBuffer* req, CBuffer* rpl)>& Handler) { return false; }

protected:
	bool m_AutoConnect = false;
};