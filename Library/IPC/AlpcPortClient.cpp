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

	static DWORD __stdcall ThreadStub(void* param)
	{
		((CAlpcPortClient*)param)->RunThread();
		return 0;
	}

    HANDLE PortHandle;

	DWORD dwServerPid = 0;

	ULONG MaxDataLen = 0;
	ULONG SizeofPortMsg = 0;
	ULONG CallSeqNumber = 0;
	std::mutex Mutex;

	std::wstring PortName;

	volatile HANDLE hThread = NULL;

	VOID(NTAPI* Callback)(const CBuffer& buff, PVOID Param);
	PVOID Param;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAlpcPortClient
//

CAlpcPortClient::CAlpcPortClient(VOID (NTAPI* Callback)(const CBuffer& buff, PVOID Param), PVOID Param) 
{
    m = new SAlpcPortClient;

	m->Callback = Callback;
	m->Param = Param;

}

CAlpcPortClient::~CAlpcPortClient() 
{
	Disconnect();

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

	if (m->hThread) {
		NtClose(m->hThread);
		m->hThread = NULL;
	}

	UNICODE_STRING PortName;
	RtlInitUnicodeString(&PortName, m->PortName.c_str());

	ALPC_PORT_ATTRIBUTES portAttr = { 0 };
	portAttr.Flags = ALPC_PORFLG_ALLOW_LPC_REQUESTS;
	portAttr.MaxMessageLength = MAX_PORTMSG_LENGTH;
	portAttr.MemoryBandwidth = 0;
	portAttr.MaxPoolUsage = (SIZE_T)-1;
	portAttr.MaxSectionSize = 0;
	portAttr.MaxViewSize = 0;
	portAttr.MaxTotalSectionSize = 0;
	portAttr.DupObjectTypes = 0;

	SECURITY_QUALITY_OF_SERVICE QoS;
	QoS.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
	QoS.ImpersonationLevel = SecurityImpersonation;
	QoS.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
	QoS.EffectiveOnly = TRUE;
	portAttr.SecurityQos = QoS;

	// ALPC requires a connection message buffer even for simple connections
	UCHAR ConnMsgBuffer[sizeof(PORT_MESSAGE)];
	PORT_MESSAGE* ConnMsg = (PORT_MESSAGE*)ConnMsgBuffer;
	memset(ConnMsg, 0, sizeof(PORT_MESSAGE));
	ConnMsg->u1.s1.TotalLength = (USHORT)sizeof(PORT_MESSAGE);
	ConnMsg->u1.s1.DataLength = 0;

	SIZE_T BufferLength = sizeof(PORT_MESSAGE);
	HANDLE PortHandle;
	NTSTATUS status = NtAlpcConnectPort(&PortHandle, &PortName, NULL, &portAttr, ALPC_MSGFLG_SYNC_REQUEST, NULL, ConnMsg, &BufferLength, NULL, NULL, NULL);
	if (!NT_SUCCESS(status))
		return ERR(status);
	m->PortHandle = PortHandle;
	m->MaxDataLen = (ULONG)portAttr.MaxMessageLength - m->SizeofPortMsg;

	m->dwServerPid = (DWORD)(UINT_PTR)ConnMsg->ClientId.UniqueProcess;

	DWORD idThread;
	m->hThread = CreateThread(NULL, 0, SAlpcPortClient::ThreadStub, this, 0, &idThread);

    return Status;
}

void CAlpcPortClient::Disconnect() 
{ 
	HANDLE hThread = InterlockedExchangePointer(&m->hThread, NULL);
	if (hThread)
	{
		if (WaitForSingleObject(hThread, 5000) == WAIT_TIMEOUT)
			TerminateThread(hThread, 0);
		NtClose(hThread);
	}

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

STATUS CAlpcPortClient::Call(const CBuffer& sendBuff, CBuffer& recvBuff, SCallParams* pParams)
{
	if (!m->PortHandle) {
		if (!m_AutoConnect)
			return ERR(STATUS_PORT_DISCONNECTED);
		if (!Connect())
			return ERR(STATUS_PORT_CONNECTION_REFUSED);
	}

	NTSTATUS status;

	UCHAR RequestBuff[MAX_PORTMSG_LENGTH];
	PORT_MESSAGE* ReqHeader = (PORT_MESSAGE*)RequestBuff;
	UCHAR* ReqData = RequestBuff + m->SizeofPortMsg;

	UCHAR ResponseBuff[MAX_PORTMSG_LENGTH];
	PORT_MESSAGE* ResHeader = (PORT_MESSAGE*)ResponseBuff;
	UCHAR* ResData = ResponseBuff + m->SizeofPortMsg;

	ULONG CurSeqNumber = m->CallSeqNumber++;

	std::unique_lock lock(m->Mutex); // since a message may be sent in multiple chunks, we need to lock the port for the duration of the call

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
			((PORT_MSG_HEADER*)ReqData)->Sequence = CurSeqNumber;

		// advance position
		Buffer += send_len;
		BuffLen -= send_len;

		SIZE_T RecvBufferLength = MAX_PORTMSG_LENGTH;
		status = NtAlpcSendWaitReceivePort(m->PortHandle, ALPC_MSGFLG_SYNC_REQUEST, (PORT_MESSAGE*)RequestBuff, NULL, (PORT_MESSAGE*)ResponseBuff, &RecvBufferLength, NULL, NULL);

		if (!NT_SUCCESS(status))
		{
			NtClose(m->PortHandle);
			m->PortHandle = NULL;
			return ERR(STATUS_UNSUCCESSFUL);
		}

		if (BuffLen && ResHeader->u1.s1.DataLength)
			return ERR(STATUS_UNSUCCESSFUL);
	}

	// the last call should return the first chunk of the reply
	if (ResHeader->u1.s1.DataLength >= GetHeaderSize())
	{
		if (((PORT_MSG_HEADER*)ReqData)->Sequence != CurSeqNumber)
			return ERR(STATUS_UNSUCCESSFUL);

		BuffLen = ((PORT_MSG_HEADER*)ResData)->h.Size;
	}
	else
		BuffLen = 0;
	if (BuffLen == 0)
		return ERR(STATUS_UNSUCCESSFUL);

	// read remaining chunks
	recvBuff.SetSize(BuffLen, true);
	Buffer = (UCHAR*)recvBuff.GetBuffer();
	for (;;)
	{
		if ((USHORT)ResHeader->u1.s1.DataLength > BuffLen)
			status = STATUS_PORT_MESSAGE_TOO_LONG;
		else
		{
			// get data
			memcpy(Buffer, ResData, (USHORT)ResHeader->u1.s1.DataLength);

			// advance position
			Buffer += (USHORT)ResHeader->u1.s1.DataLength;
			BuffLen -= (USHORT)ResHeader->u1.s1.DataLength;

			// are we done yet?
			if (!BuffLen)
				break;

			// set header
			memset(ReqHeader, 0, m->SizeofPortMsg);
			ReqHeader->u1.s1.TotalLength = (USHORT)m->SizeofPortMsg;

			SIZE_T RecvBufferLength = MAX_PORTMSG_LENGTH;
			status = NtAlpcSendWaitReceivePort(m->PortHandle, ALPC_MSGFLG_SYNC_REQUEST, (PORT_MESSAGE*)RequestBuff, NULL, (PORT_MESSAGE*)ResponseBuff, &RecvBufferLength, NULL, NULL);
		}

		if (!NT_SUCCESS(status))
		{
			recvBuff.Clear();

			NtClose(m->PortHandle);
			m->PortHandle = NULL;
			return ERR(STATUS_UNSUCCESSFUL);
		}
	}

	return OK;
}

void CAlpcPortClient::RunThread()
{
	CBuffer recvBuff;
	UCHAR* Buffer = nullptr;
	ULONG  BuffLen = 0;
	NTSTATUS status;

	UCHAR Buff[MAX_PORTMSG_LENGTH];
	PORT_MESSAGE* Header = (PORT_MESSAGE*)Buff;
	UCHAR* Data = Buff + m->SizeofPortMsg;

	while (m->hThread)
	{
		SIZE_T BufferLength = MAX_PORTMSG_LENGTH;

		status = NtAlpcSendWaitReceivePort(m->PortHandle, 0, NULL, NULL, Header, &BufferLength, NULL, NULL);

		if (!NT_SUCCESS(status))
			continue;

		// new message?
		if (BuffLen == 0)
		{
			if ((USHORT)Header->u1.s1.DataLength >= GetHeaderSize())
			{
				BuffLen = ((PORT_MSG_HEADER*)Data)->h.Size;
				recvBuff.SetPosition(0);
				recvBuff.SetSize(BuffLen, true);
				Buffer = (UCHAR*)recvBuff.GetBuffer();
			}
			else
			{
				NtAlpcSendWaitReceivePort(m->PortHandle, ALPC_MSGFLG_RELEASE_MESSAGE, Header, NULL, NULL, NULL, NULL, NULL);
				continue;
			}
		}

		if ((USHORT)Header->u1.s1.DataLength > BuffLen)
		{
			NtAlpcSendWaitReceivePort(m->PortHandle, ALPC_MSGFLG_RELEASE_MESSAGE, Header, NULL, NULL, NULL, NULL, NULL);
			BuffLen = 0;
			Buffer = nullptr;
			continue;
		}

		// get data
		memcpy(Buffer, Data, (USHORT)Header->u1.s1.DataLength);
		// advance position
		Buffer += (USHORT)Header->u1.s1.DataLength;
		BuffLen -= (USHORT)Header->u1.s1.DataLength;

		// release message
		NtAlpcSendWaitReceivePort(m->PortHandle, ALPC_MSGFLG_RELEASE_MESSAGE, Header, NULL, NULL, NULL, NULL, NULL);

		// are we done yet?
		if (BuffLen > 0)
			continue;

		PORT_MSG_HEADER* msg = (PORT_MSG_HEADER*)recvBuff.GetBuffer();

		if (msg->Sequence == -1)
		{
			if (m->Callback)
				m->Callback(recvBuff, m->Param);
		}

		// reset
		Buffer = nullptr;
		BuffLen = 0;
	}
}
