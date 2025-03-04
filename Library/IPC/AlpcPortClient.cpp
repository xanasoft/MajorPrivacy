#include "pch.h"
#include "AlpcPortClient.h"
#include "AlpcPortServer.h"

/*#include <ntstatus.h>
#define WIN32_NO_STATUS
typedef long NTSTATUS;
#include <windows.h>
#include <winternl.h>*/

#include <phnt_windows.h>
#include <phnt.h>

#include "../Helpers/Service.h"
#include "../Helpers/NtIO.h"
#include "../Helpers/AppUtil.h"

#include "PortMessage.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SAlpcPortClient
//

struct SAlpcPortClient
{
    SAlpcPortClient()
    {
        PortHandle = NULL;
		SizeofPortMsg = sizeof(PORT_MESSAGE);
		//if (IsWow64())
		//	SizeofPortMsg += sizeof(ULONG) * 4;
    }

    HANDLE PortHandle;

	DWORD dwServerPid = 0;

	ULONG MaxDataLen = 0;
	ULONG SizeofPortMsg = 0;
	ULONG CallSeqNumber = 0;

	std::wstring PortName;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAlpcPortClient
//

CAlpcPortClient::CAlpcPortClient() 
{
    m = new SAlpcPortClient;
}

CAlpcPortClient::~CAlpcPortClient() 
{
    delete m;
}

STATUS CAlpcPortClient::Connect(const wchar_t* Name)
{
	m->PortName = Name;

	return Connect();
}

STATUS CAlpcPortClient::Connect()
{
	STATUS Status;

    if (m->PortHandle)
		return OK;

	SECURITY_QUALITY_OF_SERVICE QoS;
	QoS.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
	QoS.ImpersonationLevel = SecurityImpersonation;
	QoS.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
	QoS.EffectiveOnly = TRUE;

	UNICODE_STRING PortName;
	RtlInitUnicodeString(&PortName, m->PortName.c_str());
	NTSTATUS status = NtConnectPort(&m->PortHandle, &PortName, &QoS, NULL, NULL, &m->MaxDataLen, NULL, NULL);
	if (!NT_SUCCESS(status))
		return ERR(status); // 2203
	m->MaxDataLen -= m->SizeofPortMsg;

	// Function associate PortHandle with thread, and sends LPC_TERMINATION_MESSAGE to specified port immediately after call NtTerminateThread.
	//NtRegisterThreadTerminatePort(m->PortHandle);

	// Get PID
	CBuffer sendBuff;
	sendBuff.SetData(NULL, sizeof(MSG_HEADER)); // make room for header, pointer points after the header
	PMSG_HEADER reqHeader = (PMSG_HEADER)sendBuff.GetBuffer();
	reqHeader->MessageId = -1;
	reqHeader->Size = sizeof(MSG_HEADER);

	CBuffer recvBuff;
	recvBuff.SetData(NULL, sizeof(MSG_HEADER) + sizeof(uint32));
	PMSG_HEADER resHeader = (PMSG_HEADER)recvBuff.GetBuffer();
	
	if (NT_SUCCESS(Call(sendBuff, recvBuff)))
	{
		if (NT_SUCCESS(resHeader->Status)) {
			recvBuff.SetPosition(sizeof(MSG_HEADER));
			m->dwServerPid = recvBuff.ReadValue<uint32>();
		}
	}
	//

    return Status;	    
}

void CAlpcPortClient::Disconnect() 
{ 
    if (m->PortHandle) {
		NtClose(m->PortHandle);
		m->PortHandle = NULL;
	}
}

bool CAlpcPortClient::IsConnected()
{
	return m->PortHandle;
}

uint32 CAlpcPortClient::GetServerPID() const
{
	return m->dwServerPid;
}

STATUS CAlpcPortClient::Call(const CBuffer& sendBuff, CBuffer& recvBuff)
{
	if (!m->PortHandle) {
		if (!m_AutoConnect)
			return ERR(STATUS_PORT_DISCONNECTED);
		if (!Connect())
			return ERR(STATUS_PORT_CONNECTION_REFUSED);
	}

	UCHAR RequestBuff[MAX_PORTMSG_LENGTH];
	PORT_MESSAGE* ReqHeader = (PORT_MESSAGE*)RequestBuff;
	UCHAR* ReqData = RequestBuff + m->SizeofPortMsg;

	UCHAR ResponseBuff[MAX_PORTMSG_LENGTH];
	PORT_MESSAGE* ResHeader = (PORT_MESSAGE*)ResponseBuff;
	UCHAR* ResData = ResponseBuff + m->SizeofPortMsg;

	UCHAR CurSeqNumber = (UCHAR)m->CallSeqNumber++;

	// Send the request in chunks
	UCHAR* Buffer = (UCHAR*)sendBuff.GetBuffer();
	ULONG BuffLen = (ULONG)sendBuff.GetSize();
	while (BuffLen)
	{
		ULONG send_len = BuffLen > m->MaxDataLen ? m->MaxDataLen : BuffLen;

		// set header
		memset(ReqHeader, 0, m->SizeofPortMsg);
		ReqHeader->u1.s1.DataLength = (USHORT)send_len;
		ReqHeader->u1.s1.TotalLength = (USHORT)(m->SizeofPortMsg + send_len);

		// set req data
		memcpy(ReqData, Buffer, send_len);

		// use highest byte of the length as sequence field
		if (Buffer == (UCHAR*)sendBuff.GetBuffer())
			ReqData[3] = CurSeqNumber;

		// advance position
		Buffer += send_len;
		BuffLen -= send_len;

		NTSTATUS status = NtRequestWaitReplyPort(m->PortHandle, (PORT_MESSAGE*)RequestBuff, (PORT_MESSAGE*)ResponseBuff);

		if (!NT_SUCCESS(status))
		{
			NtClose(m->PortHandle);
			m->PortHandle = NULL;
			return ERR(STATUS_UNSUCCESSFUL);
			//return SB_ERR(SB_ServiceFail, QVariantList() << QString("request %1").arg(status, 8, 16), status); // 2203
		}

		if (BuffLen && ResHeader->u1.s1.DataLength)
			return ERR(STATUS_UNSUCCESSFUL);
			//return SB_ERR(SB_ServiceFail, QVariantList() << QString("early reply")); // 2203
	}

	// the last call to NtRequestWaitReplyPort should return the first chunk of the reply
	if (ResHeader->u1.s1.DataLength >= sizeof(MSG_HEADER))
	{
		if (ResData[3] != CurSeqNumber)
			return ERR(STATUS_UNSUCCESSFUL);
			//return SB_ERR(SB_ServiceFail, QVariantList() << QString("mismatched reply")); // 2203

		// clear highest byte of the size field
		ResData[3] = 0;
		BuffLen = ((MSG_HEADER*)ResData)->Size;
	}
	else
		BuffLen = 0;
	if (BuffLen == 0)
		return ERR(STATUS_UNSUCCESSFUL);
		//return SB_ERR(SB_ServiceFail, QVariantList() << QString("null reply (msg %1 len %2)").arg(req->msgid, 8, 16).arg(req->length)); // 2203

	// read remaining chunks
	recvBuff.SetSize(BuffLen, true);
	Buffer = (UCHAR*)recvBuff.GetBuffer();
	for (;;)
	{
		NTSTATUS status;
		if ((ULONG)ResHeader->u1.s1.DataLength > BuffLen)
			status = STATUS_PORT_MESSAGE_TOO_LONG;
		else
		{
			// get data
			memcpy(Buffer, ResData, ResHeader->u1.s1.DataLength);

			// adcance position
			Buffer += ResHeader->u1.s1.DataLength;
			BuffLen -= ResHeader->u1.s1.DataLength;

			// are we done yet?
			if (!BuffLen)
				break;

			// set header
			memset(ReqHeader, 0, m->SizeofPortMsg);
			ReqHeader->u1.s1.TotalLength = (USHORT)m->SizeofPortMsg;

			status = NtRequestWaitReplyPort(m->PortHandle, (PORT_MESSAGE*)RequestBuff, (PORT_MESSAGE*)ResponseBuff);
		}

		if (!NT_SUCCESS(status))
		{
			recvBuff.Clear();

			NtClose(m->PortHandle);
			m->PortHandle = NULL;
			//return SB_ERR(SB_ServiceFail, QVariantList() << QString("reply %1").arg(status, 8, 16), status); // 2203
			return ERR(STATUS_UNSUCCESSFUL);
		}
	}

	return OK;
}