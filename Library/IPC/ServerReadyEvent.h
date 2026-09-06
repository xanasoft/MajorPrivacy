#pragma once
#include "../Status.h"
#include "../lib_global.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CServerReadyEvent
//
// Provides event-based synchronization between client and worker/engine for startup coordination.
// Used when client starts a worker process in the same session (not for service case).
//
// - Client: Creates a named event in Local\ namespace, starts worker, waits for signal
// - Worker: Opens existing event and signals it when ready
//
// Note: For service connections, use service state checking instead (SERVICE_RUNNING).
//

class LIBRARY_EXPORT CServerReadyEvent
{
public:
    CServerReadyEvent();
    ~CServerReadyEvent();

    // Client side: Create the event in Local\ namespace
    // Returns true if event was created (caller should wait after starting worker)
    // Returns false if creation failed (name conflict, etc.)
    bool ClientCreate(const wchar_t* EventName);

    // Client side: Wait for worker to signal ready
    // Returns true if signaled within timeout
    // Returns false on timeout or error
    bool ClientWait(DWORD TimeoutMs);

    // Client side: Release the event handle
    void ClientRelease();

    // Returns true if an event handle is held
    bool HasEvent() const { return m_hEvent != NULL; }

    // Returns the event handle (for use with WaitForMultipleObjects)
    HANDLE GetHandle() const { return m_hEvent; }

    // Worker/Server side: Open existing event and signal it
    // Call this when the worker is ready to accept connections
    // Returns true if event existed and was signaled
    // Returns false if no event exists (started as service, no waiting client)
    static bool WorkerSignal(const wchar_t* EventName);

private:
    HANDLE m_hEvent;
};
