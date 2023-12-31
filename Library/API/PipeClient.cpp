#include "pch.h"
#include "PipeClient.h"
#include "PipeServer.h"

/*#include <ntstatus.h>
#define WIN32_NO_STATUS
typedef long NTSTATUS;
#include <windows.h>
#include <winternl.h>*/

#include <phnt_windows.h>
#include <phnt.h>

#include <ph.h>

#include "../Helpers/Service.h"
#include "../Helpers/NtIO.h"
#include "../Helpers/AppUtil.h"

#include "PortMessage.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SPipeClient
//

struct SPipeClient
{
	~SPipeClient()
	{
		if (hEvent)
			NtClose(hEvent);
	}

	static DWORD __stdcall ThreadStub(void* param)
	{
		((CPipeClient*)param)->RunThread();
		return 0;
	}

    PPipeSocket pPipe;
	std::wstring PipeName;

	volatile HANDLE hThread = NULL;
	HANDLE hEvent = NULL;

	CBuffer* recvBuff = NULL;
	std::mutex Mutex;

	VOID(NTAPI* Callback)(const CBuffer& buff, PVOID Param);
	PVOID Param;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPipeClient
//

CPipeClient::CPipeClient(VOID (NTAPI* Callback)(const CBuffer& buff, PVOID Param), PVOID Param) 
{
    m = new SPipeClient;

	m->Callback = Callback;
	m->Param = Param;

	m->hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
}

CPipeClient::~CPipeClient() 
{
	Disconnect();

    delete m;
}

STATUS CPipeClient::Connect(const wchar_t* Name)
{
	m->PipeName = Name;

	return Connect();
}

STATUS CPipeClient::Connect()
{
    if (m->pPipe)
		return OK;
	if (m->hThread) {
		NtClose(m->hThread);
		m->hThread = NULL;
	}

    m->pPipe = std::make_shared<SPipeSocket>();
	if(!m->pPipe->olRead.hEvent || !m->pPipe->olWrite.hEvent) {
        m->pPipe.reset();
        return ERR(PhGetLastWin32ErrorAsNtStatus());
    }

retry:
	m->pPipe->hPipe = CreateFile(m->PipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (m->pPipe->hPipe == INVALID_HANDLE_VALUE) {
		m->pPipe->hPipe = NULL;

		if (GetLastError() == ERROR_PIPE_BUSY)
		{
			if (WaitNamedPipe(m->PipeName.c_str(), 1000))
				goto retry;
		}

        m->pPipe.reset();
		return ERR(PhGetLastWin32ErrorAsNtStatus());
	}

	DWORD dwMode = PIPE_READMODE_MESSAGE; 
	SetNamedPipeHandleState(m->pPipe->hPipe, &dwMode, NULL, NULL); 

    DWORD idThread;
    m->hThread = CreateThread(NULL, 0, SPipeClient::ThreadStub, this, 0, &idThread);
	if (!m->hThread) {
		m->pPipe.reset();
		return ERR(PhGetLastWin32ErrorAsNtStatus());
	}

    return OK;	    
}

void CPipeClient::Disconnect() 
{ 
    m->pPipe.reset();

	HANDLE hThread = InterlockedExchangePointer(&m->hThread, NULL);
	if (hThread)
	{
		if (WaitForSingleObject(hThread, 5000) == WAIT_TIMEOUT)
			TerminateThread(hThread, 0);
		NtClose(hThread);
	}
}

bool CPipeClient::IsConnected()
{
	return !!m->pPipe;
}

STATUS CPipeClient::Call(const CBuffer& sendBuff, CBuffer& recvBuff)
{
	if (!m->pPipe) {
		if(!Connect())
			return ERR(STATUS_PORT_DISCONNECTED);
	}

	m->Mutex.lock();
	m->recvBuff = &recvBuff;
	ResetEvent(m->hEvent);
	m->Mutex.unlock();

	STATUS Status = m->pPipe->WritePacket(sendBuff);

	HANDLE Events[] = { m->hEvent, m->hThread };
	if (WaitForMultipleObjects(2, Events, FALSE, INFINITE) != WAIT_OBJECT_0) // if not hEvent then the thread died
	{
		Disconnect();
		Status = ERR(STATUS_UNSUCCESSFUL);
	}

	return Status;
}

void CPipeClient::RunThread()
{
    CBuffer recvBuff;

	while (m->hThread)
	{
        // reset buffer and read
        recvBuff.SetSize(0);
        if(!m->pPipe->ReadPacket(recvBuff))
            break;

		MSG_HEADER* msg = (MSG_HEADER*)recvBuff.GetBuffer();
		if (msg->Flag)
		{
			if (m->Callback)
			{
				m->Callback(recvBuff, m->Param);
			}
		}
		else
		{
			m->Mutex.lock();
			if (m->recvBuff) 
			{
				m->recvBuff->CopyBuffer(recvBuff.GetBuffer(), recvBuff.GetSize());
				m->recvBuff = NULL;

				SetEvent(m->hEvent);
			}
			m->Mutex.unlock();
		}
	}
}