#pragma once
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/Variant.h"
#include "AbstractClient.h"

class LIBRARY_EXPORT CDeviceClient : public CAbstractClient
{
public:
    CDeviceClient() 
    {
        m_IoDeviceHandle = NULL;
    }
    virtual ~CDeviceClient() {}

    virtual STATUS Connect(const wchar_t* Name) 
    { 
	    IO_STATUS_BLOCK IoStatusBlock;
	    NTSTATUS status = NtOpenFile(&m_IoDeviceHandle, FILE_GENERIC_READ, SNtObject(Name).Get(), &IoStatusBlock, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
        if (!NT_SUCCESS(status))
            return ERR(status);
        return OK;
    }
    virtual void Disconnect() 
    { 
        if (m_IoDeviceHandle) {
            NtClose(m_IoDeviceHandle);
            m_IoDeviceHandle = NULL;
        }
    }
    virtual bool IsConnected()
    {
        return m_IoDeviceHandle;
    }

    virtual STATUS Call(const CBuffer& inBuff, CBuffer& outBuff)
    {
        if (!m_IoDeviceHandle)
            return ERR(STATUS_DEVICE_NOT_CONNECTED);

        IO_STATUS_BLOCK IoStatusBlock;
        NTSTATUS status = NtDeviceIoControlFile(m_IoDeviceHandle, NULL, NULL, NULL, &IoStatusBlock, API_DEVICE_CTLCODE, (PVOID)inBuff.GetBuffer(), (ULONG)inBuff.GetSize(), outBuff.GetBuffer(), (ULONG)outBuff.GetCapacity());
        if (!NT_SUCCESS(status))
            return ERR(status);
        outBuff.SetSize(IoStatusBlock.Information);
    
        return OK;
    }

protected:
    HANDLE m_IoDeviceHandle;
};