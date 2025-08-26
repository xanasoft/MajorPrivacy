#include "pch.h"
#include "WindowsFwLog.h"
#include "../Library/Common/Strings.h"
#include "../Framework/Common/Buffer.h"
#include "../Library/Helpers/EvtUtil.h"
#include "../Library/Helpers/NtUtil.h"

#include <WS2tcpip.h>

CWindowsFwLog::CWindowsFwLog()
{
	m_OldAuditingMode = -1;
}

CWindowsFwLog::~CWindowsFwLog()
{
	Stop();

	if (m_OldAuditingMode != -1) 
		SetAuditPolicy(&Audit_ObjectAccess_FirewallConnection_, m_OldAuditingMode, NULL);
}

bool CWindowsFwLog::ReadEvent(EVT_HANDLE hEvent, SWinFwLogEvent& Event)
{
	/*
<Event xmlns="http://schemas.microsoft.com/win/2004/08/events/event">
	<System>
		<Provider Name="Microsoft-Windows-Security-Auditing" Guid="{54849625-5478-4994-a5ba-3e3b0328c30d}" />
		<EventID>5157</EventID>
		<Version>1</Version>
		<Level>0</Level>
		<Task>12810</Task>
		<Opcode>0</Opcode>
		<Keywords>0x8010000000000000</Keywords>
		<TimeCreated SystemTime="2019-08-17T19:38:51.674547600Z" />
		<EventRecordID>38795210</EventRecordID>
		<Correlation />
		<Execution ProcessID="4" ThreadID="27028" />
		<Channel>Security</Channel>
		<Computer>Edison</Computer>
		<Security />
	</System>
	<EventData>
		<Data Name="ProcessID">23352</Data>
		<Data Name="Application">\device\harddiskvolume7\program files (x86)\microsoft visual studio\2017\enterprise\vc\tools\msvc\14.16.27023\bin\hostx86\x64\vctip.exe</Data>
		<Data Name="Direction">%%14593</Data>
		<Data Name="SourceAddress">10.70.0.12</Data>
		<Data Name="SourcePort">60237</Data>
		<Data Name="DestAddress">152.199.19.161</Data>
		<Data Name="DestPort">443</Data>
		<Data Name="Protocol">6</Data>
		<Data Name="FilterRTID">226530</Data>
		<Data Name="LayerName">%%14611</Data>
		<Data Name="LayerRTID">48</Data>
		<Data Name="RemoteUserID">S-1-0-0</Data>
		<Data Name="RemoteMachineID">S-1-0-0</Data>
	</EventData>
</Event>
	*/

	// Create the render context 
	static PCWSTR eventProperties[] = {
		L"Event/System/Provider/@Name",		// 0
		L"Event/System/EventID",			// 1
		L"Event/System/Level",				// 2
		//L"Event/System/Keywords",			// 3
		L"Event/System/TimeCreated/@SystemTime",		// 3

		L"Event/EventData/Data[@Name='ProcessID']",		// 4
		L"Event/EventData/Data[@Name='Application']",	// 5
		L"Event/EventData/Data[@Name='Direction']",		// 6
		L"Event/EventData/Data[@Name='SourceAddress']",	// 7
		L"Event/EventData/Data[@Name='SourcePort']",	// 8
		L"Event/EventData/Data[@Name='DestAddress']",	// 9
		L"Event/EventData/Data[@Name='DestPort']",		// 10
		L"Event/EventData/Data[@Name='Protocol']",		// 11
		L"Event/EventData/Data[@Name='LayerRTID']"		// 12

		/*L"Event/EventData/Data[@Name='FilterRTID']",		// 13
		L"Event/EventData/Data[@Name='LayerName']",			// 14
		L"Event/EventData/Data[@Name='RemoteUserID']",		// 15
		L"Event/EventData/Data[@Name='RemoteMachineID']"*/	// 16
	};

	std::vector<BYTE> data = GetEventData(hEvent, ARRAYSIZE(eventProperties), eventProperties);

	PEVT_VARIANT values = (PEVT_VARIANT)data.data();

	// Publisher name
	//std::wstring PublisherName = GetWinVariantString(values[0]);

	// Event ID
#pragma prefast(suppress : 6011) // buffer is not null
	uint16_t EventId = GetWinVariantNumber(values[1], (uint16_t)0);
	switch (EventId)
	{
	case WIN_LOG_EVENT_FW_BLOCKED:	Event.Type = EFwActions::Block; break;
	case WIN_LOG_EVENT_FW_ALLOWED:	Event.Type = EFwActions::Allow; break;
	}

	/*
	// Severity level
	uint16_t Level = GetWinVariantNumber(values[2], (byte)0);
	switch(values[2].ByteVal)
	{
		case WINEVENT_LEVEL_CRITICAL:
		case WINEVENT_LEVEL_ERROR:			Event.Level = EVENTLOG_ERROR_TYPE;	break;
		case WINEVENT_LEVEL_WARNING :		Event.Level = EVENTLOG_WARNING_TYPE;	break;
		case WINEVENT_LEVEL_INFO:
		case WINEVENT_LEVEL_VERBOSE:
		default:							Event.Level = EVENTLOG_INFORMATION_TYPE;
	}

	//
	uint64_t Keywords = GetWinVariantNumber(values[3], (uint64_t)0);
	if (Keywords & WINEVENT_KEYWORD_AUDIT_SUCCESS)
		//Event.Level = EVENTLOG_AUDIT_SUCCESS;
		Event.Type = EFwActions::Allow;
	else if (Keywords & WINEVENT_KEYWORD_AUDIT_FAILURE)
		//Event.Level = EVENTLOG_AUDIT_FAILURE;
		Event.Type = EFwActions::Block;
	*/

	PEVT_VARIANT uTime  = &values[3]; // EvtVarTypeFileTime
	Event.TimeStamp = GetWinVariantNumber<uint64>(*uTime, 0);

	// Process Info
	Event.ProcessId = GetWinVariantNumber(values[4], (uint64_t)-1);
	Event.ProcessFileName = GetWinVariantString(values[5]);

	// Direction
	std::wstring strDirection = GetWinVariantString(values[6]);
	if (strDirection == WIN_LOG_EVENT_FW_INBOUND)
		Event.Direction = EFwDirections::Inbound;
	else if (strDirection == WIN_LOG_EVENT_FW_OUTBOUN)
		Event.Direction = EFwDirections::Outbound;
	//else
	//	qDebug() << "CWindowsFwLog: Unknown direction:" << strDirection;

	// Addresses
	std::wstring SourceAddress = GetWinVariantString(values[7]);
	std::wstring SourcePort = GetWinVariantString(values[8]);
	std::wstring DestAddress = GetWinVariantString(values[9]);
	std::wstring DestPort = GetWinVariantString(values[10]);
	if (Event.Direction == EFwDirections::Outbound)
	{
		Event.LocalAddress.FromWString(SourceAddress);
		Event.LocalPort = (uint16)std::stoul(SourcePort);
		Event.RemoteAddress.FromWString(DestAddress);
		Event.RemotePort = (uint16)std::stoul(DestPort);
	}
	else if (Event.Direction == EFwDirections::Inbound)
	{
		Event.LocalAddress.FromWString(DestAddress);
		Event.LocalPort = (uint16)std::stoul(DestPort);
		Event.RemoteAddress.FromWString(SourceAddress);
		Event.RemotePort = (uint16)std::stoul(SourcePort);
	}

	// Protocol
	uint32 Protocol = GetWinVariantNumber(values[11], (uint32_t)0);
	if (Protocol == IPPROTO_TCP) // TCP
		Event.ProtocolType = EFwKnownProtocols::TCP;
	else if (Protocol == IPPROTO_UDP) // UDP
		Event.ProtocolType = EFwKnownProtocols::UDP;

	//uint32 LayerRTID = GetWinVariantNumber(values[12], (uint32_t)0);

	return true;
}

void CWindowsFwLog::OnEvent(EVT_HANDLE hEvent)
{
	SWinFwLogEvent Event;
	ReadEvent(hEvent, Event);
	if(Event.Type == EFwActions::Undefined)
		return;

	std::unique_lock<std::mutex> Lock(m_HandlersMutex);
	for (auto Handler : m_Handlers)
		Handler(&Event);
}

std::wstring CWindowsFwLog::GetXmlQuery()
{
	// *[System[Provider[ @Name='%2'] and (EventID=%3)]]
	// Note: 
	//			Allowed connections LayerRTID == 48 
	//			Blocked connections LayerRTID == 44
	//			Opening a TCP port for listening LayerRTID == 38 and 36 // Resource allocation
	//			Opening a UDP port LayerRTID == 38 and 36 // Resource allocation
	uint32 LayerRTID = 44;
	std::wstring Query = L"*[System[(Level=4 or Level=0) and (EventID=" STR(WIN_LOG_EVENT_FW_BLOCKED) " or EventID=" STR(WIN_LOG_EVENT_FW_ALLOWED) ")]] and *[EventData[Data[@Name='LayerRTID']>='" + std::to_wstring(LayerRTID) + L"']]";
	std::wstring XmlQuery = L"<QueryList><Query Id=\"0\" Path=\"" L"Security" L"\"><Select Path=\"" L"Security" L"\">" + Query + L"</Select></Query></QueryList>";

	return XmlQuery;
}

STATUS CWindowsFwLog::Start(int AuditingMode)
{
	if (m_hSubscription)
		return OK;

	if (!SetAuditPolicy(&Audit_ObjectAccess_FirewallConnection_, AuditingMode, &m_OldAuditingMode))
		return ERR(STATUS_UNSUCCESSFUL, L"Failed to configure the auditing policy");

	if (CEventLogListener::Start(GetXmlQuery()))
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

STATUS CWindowsFwLog::UpdatePolicy(int AuditingMode)
{
	uint32 OldAuditingMode = -1;
	if (!SetAuditPolicy(&Audit_ObjectAccess_FirewallConnection_, AuditingMode, &OldAuditingMode))
		return ERR(STATUS_UNSUCCESSFUL, L"Failed to configure the auditing policy");
	if(m_OldAuditingMode == -1) // keep the original old mode if there is any
		m_OldAuditingMode = OldAuditingMode;
	return OK;
}

uint32 CWindowsFwLog::GetCurrentPolicy()
{
	return GetAuditPolicy(&Audit_ObjectAccess_FirewallConnection_);
}

void CWindowsFwLog::Stop()
{
	CEventLogListener::Stop();

	if (m_OldAuditingMode != -1) {
		SetAuditPolicy(&Audit_ObjectAccess_FirewallConnection_, m_OldAuditingMode, NULL);
		m_OldAuditingMode = -1;
	}
}