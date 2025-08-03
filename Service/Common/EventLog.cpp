#include "pch.h"
#include "EventLog.h"
#include "../../Library/Helpers/MiscHelpers.h"
#include "../../Library/Common/FileIO.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/API/PrivacyAPIs.h"
#include "../ServiceCore.h"

#define API_EVENT_LOG_FILE_NAME L"EventLog.dat"
#define API_EVENT_LOG_FILE_VERSION 1
#define API_EVENT_LOG_MAX_ENTRIES 1000

CEventLogEntry::CEventLogEntry(ELogLevels Level, int Type, const StVariant& Data)
{
	m_TimeStamp = GetCurTimeStamp();
	m_EventLevel = Level;
	m_EventType = Type;
	m_EventData = Data;
}

StVariant CEventLogEntry::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
    StVariantWriter Data(pMemPool);
    if (Opts.Format == SVarWriteOpt::eIndex) {
        Data.BeginIndex();
        WriteIVariant(Data, Opts);
    } 
	//else {  
    //    Data.BeginMap();
    //    WriteMVariant(Data, Opts);
    //}
    return Data.Finish();
}

NTSTATUS CEventLogEntry::FromVariant(const class StVariant& Data)
{
	if (Data.GetType() == VAR_TYPE_INDEX)  StVariantReader(Data).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
    //else if (Data.GetType() == VAR_TYPE_MAP)         StVariantReader(Data).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
    else
        return STATUS_UNKNOWN_REVISION;
    return STATUS_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////
// CEventLog

CEventLog::CEventLog()
{
}

void CEventLog::AddEvent(ELogLevels Level, int Type, const StVariant& Data)
{
	std::unique_lock<std::shared_mutex> Lock(m_Mutex);

	CEventLogEntryPtr pEntry = std::make_shared<CEventLogEntry>(Level, Type, Data);
	m_Entries.push_back(pEntry);

	SVarWriteOpt Opts;
	Opts.Format = SVarWriteOpt::eIndex;
	theCore->BroadcastMessage(SVC_API_EVENT_LOG_ENTRY, pEntry->ToVariant(Opts));
}

STATUS CEventLog::Load()
{
	CBuffer Buffer;
	if (!ReadFile(theCore->GetDataFolder() + L"\\" API_EVENT_LOG_FILE_NAME, Buffer)) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_EVENT_LOG_FILE_NAME L" not found");
		return ERR(STATUS_NOT_FOUND);
	}

	StVariant Data;
	//try {
	auto ret = Data.FromPacket(&Buffer, true);
	//} catch (const CException&) {
	//	return ERR(STATUS_UNSUCCESSFUL);
	//}
	if (ret != StVariant::eErrNone) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_EVENT_LOG_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	if (Data[API_S_VERSION].To<uint32>() != API_EVENT_LOG_FILE_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_EVENT_LOG_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	LoadEntries(Data[API_S_EVENT_LOG]);

	return OK;
}

STATUS CEventLog::Store()
{
	SVarWriteOpt Opts;
	Opts.Format = SVarWriteOpt::eIndex;
	Opts.Flags = SVarWriteOpt::eSaveToFile;

	StVariant Data;
	Data[API_S_VERSION] = API_EVENT_LOG_FILE_VERSION;
	Data[API_S_EVENT_LOG] = SaveEntries(Opts);

	CBuffer Buffer;
	Data.ToPacket(&Buffer);
	WriteFile(theCore->GetDataFolder() + L"\\" API_EVENT_LOG_FILE_NAME, Buffer);

	return OK;
}

STATUS CEventLog::LoadEntries(const StVariant& Entries)
{
	std::unique_lock<std::shared_mutex> Lock(m_Mutex);

	m_Entries.clear();

	StVariantReader(Entries).ReadRawList([&](const StVariant& Entry) {
		CEventLogEntryPtr pEntry = std::make_shared<CEventLogEntry>();
		if (pEntry->FromVariant(Entry) == STATUS_SUCCESS)
			m_Entries.push_back(pEntry);
	});
	return OK;
}

StVariant CEventLog::SaveEntries(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool)
{
	std::shared_lock<std::shared_mutex> Lock(m_Mutex);

	StVariantWriter EventLog(pMemPool);
	EventLog.BeginList();

	size_t from = 0;
	if(m_Entries.size() > API_EVENT_LOG_MAX_ENTRIES)
		from = m_Entries.size() - API_EVENT_LOG_MAX_ENTRIES;

	for(size_t i=from; i < m_Entries.size(); i++) {
		const CEventLogEntryPtr& pEntry = m_Entries[i];
		EventLog.WriteVariant(pEntry->ToVariant(Opts, pMemPool));
	}
	return EventLog.Finish();
}

void CEventLog::ClearEvents()
{
	std::unique_lock<std::shared_mutex> Lock(m_Mutex);

	m_Entries.clear();
}