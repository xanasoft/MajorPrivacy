#include "pch.h"
#include "ServerReadyEvent.h"

#include <phnt_windows.h>
#include <phnt.h>
#include <sddl.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CServerReadyEvent
//

CServerReadyEvent::CServerReadyEvent()
{
    m_hEvent = NULL;
}

CServerReadyEvent::~CServerReadyEvent()
{
    ClientRelease();
}

bool CServerReadyEvent::ClientCreate(const wchar_t* EventName)
{
    if (m_hEvent)
        return true; // Already created

    //
    // Create the event in Local\ namespace (session-local).
    // This works for non-elevated clients because we're not using Global\.
    // The worker process (same session) will be able to open and signal it.
    //
    // Manual-reset event so multiple waiters (if any) all wake up.
    // Initial state is non-signaled.
    //

    //SECURITY_ATTRIBUTES sa = { sizeof(sa) };
    //sa.bInheritHandle = FALSE;
    //sa.lpSecurityDescriptor = NULL;
    //if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
    //    L"D:(A;;0x1F0003;;;WD)",  // Allow Everyone EVENT_ALL_ACCESS
    //    SDDL_REVISION_1,
    //    &sa.lpSecurityDescriptor,
    //    NULL))
    //{
    //    m_hEvent = CreateEventW(&sa, TRUE, FALSE, EventName);
    //    LocalFree(sa.lpSecurityDescriptor);
    //}
    //else   
    m_hEvent = CreateEventW(NULL, TRUE, FALSE, EventName);

    if (m_hEvent)
    {
		DWORD err = GetLastError();
        if (err == ERROR_ALREADY_EXISTS)
        {
            // Another client already created it - that's fine, we can still wait on it
        }
        return true;
    }

    // Failed to create - name conflict with different object type, or other error
    return false;
}

bool CServerReadyEvent::ClientWait(DWORD TimeoutMs)
{
    if (!m_hEvent)
        return true;

    DWORD result = WaitForSingleObject(m_hEvent, TimeoutMs);
    return (result == WAIT_OBJECT_0);
}

void CServerReadyEvent::ClientRelease()
{
    if (m_hEvent)
    {
        CloseHandle(m_hEvent);
        m_hEvent = NULL;
    }
}

bool CServerReadyEvent::WorkerSignal(const wchar_t* EventName)
{
    //
    // Try to open existing event created by waiting client.
    // We only need EVENT_MODIFY_STATE to signal it.
    //

    HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, EventName);

    if (!hEvent)
    {
        // No event exists - either:
        // - Started as a service (no client waiting with event)
        // - Client didn't create event (older client, or fallback mode)
        // This is a normal condition, not an error.
        return false;
    }

    //
    // Signal the event to wake all waiting clients
    //

    SetEvent(hEvent);
    CloseHandle(hEvent);

    return true;
}
