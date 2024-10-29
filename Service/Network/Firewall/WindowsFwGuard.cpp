#include "pch.h"
#include "WindowsFwGuard.h"
#include "../Library/Common/Strings.h"
#include "../Library/Common/Buffer.h"
#include "../Library/Helpers/EvtUtil.h"
#include "../Library/Helpers/NtUtil.h"

CWindowsFwGuard::CWindowsFwGuard()
{
	m_OldAuditingMode = -1;
}

CWindowsFwGuard::~CWindowsFwGuard()
{
	Stop();

	if (m_OldAuditingMode != -1)
		SetAuditPolicy(&Audit_ObjectAccess_FirewallRuleChange_, 1, m_OldAuditingMode, NULL);
}

void CWindowsFwGuard::OnEvent(EVT_HANDLE hEvent)
{
	// Create the render context 
	static PCWSTR eventProperties[] = { 
        L"Event/System/Provider/@Name",						//  0
		L"Event/System/EventID",			                //  1
		L"Event/System/Execution/@ProcessID",				//  2
		L"Event/System/Execution/@ThreadID",				//  3

		L"Event/EventData/Data[@Name='ProfileChanged']",	//  4 - ProfileChanged
		L"Event/EventData/Data[@Name='RuleId']",	        //  5 - RuleId
		L"Event/EventData/Data[@Name='RuleName']",			//  6 - RuleName
	};

	std::vector<BYTE> data = GetEventData(hEvent, ARRAYSIZE(eventProperties), eventProperties);

	PEVT_VARIANT values = (PEVT_VARIANT)data.data();

	SWinFwGuardEvent Event;

	// Publisher name
	//std::wstring PublisherName = GetWinVariantString(values[0]);
	
	// Event ID
#pragma prefast(suppress : 6011) // buffer is not null
	uint16_t EventId = GetWinVariantNumber(values[1], (uint16_t)0);
	switch (EventId)
	{
		case WIN_LOG_EVENT_FW_ADDED:	Event.Type = ERuleEvent::eAdded; break;
		case WIN_LOG_EVENT_FW_REMOVED:	Event.Type = ERuleEvent::eRemoved; break;
		case WIN_LOG_EVENT_FW_CHANGED:	Event.Type = ERuleEvent::eModified; break;
	}

	// Process Info
	//Event.ProcessId = GetWinVariantNumber(values[2], (uint64_t)-1); // always LSA?

    // event Info
    Event.ProfileChanged = GetWinVariantString(values[4]);
    Event.RuleId = GetWinVariantString(values[5]);
    Event.RuleName = GetWinVariantString(values[6]);

	std::unique_lock<std::mutex> Lock(m_HandlersMutex);

	for (auto Handler : m_Handlers)
		Handler(&Event);
}

STATUS CWindowsFwGuard::Start()
{
	if (m_hSubscription)
		return OK;

	int AuditingMode = AUDIT_POLICY_INFORMATION_TYPE_SUCCESS;		// rule changes

	if (!SetAuditPolicy(&Audit_ObjectAccess_FirewallRuleChange_, 1, AuditingMode, &m_OldAuditingMode)) // todo: save old and restore
		return ERR(STATUS_UNSUCCESSFUL, L"Failed to configure the auditing policy");

	std::wstring Query = L"*[System[(Level=4 or Level=0) and (EventID=" STR(WIN_LOG_EVENT_FW_ADDED) " or EventID=" STR(WIN_LOG_EVENT_FW_CHANGED) " or EventID=" STR(WIN_LOG_EVENT_FW_REMOVED)")]]";
	std::wstring XmlQuery = L"<QueryList><Query Id=\"0\" Path=\"" L"Security" L"\"><Select Path=\"" L"Security" L"\">" + Query + L"</Select></Query></QueryList>";

	if (CEventLogListener::Start(XmlQuery))
		return OK;

	DWORD err = GetLastError();
	NTSTATUS status = GetLastWin32ErrorAsNtStatus();
	if (ERROR_EVT_CHANNEL_NOT_FOUND == err)
		return ERR(status, L"Channel  was not found.");
	if (ERROR_EVT_INVALID_QUERY == err)
		// You can call EvtGetExtendedStatus to get information as to why the query is not valid.
		return ERR(status, L"The query is not valid");
	return ERR(status);
}

void CWindowsFwGuard::Stop()
{
	CEventLogListener::Stop();

	if (m_OldAuditingMode != -1) {
		SetAuditPolicy(&Audit_ObjectAccess_FirewallRuleChange_, 1, m_OldAuditingMode, NULL);
		m_OldAuditingMode = -1;
	}
}