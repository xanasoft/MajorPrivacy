#pragma once
#include "../Types.h"
#include "../lib_global.h"
#ifndef ASSERT
#define ASSERT(x)
#endif
//#include "../Common/Variant.h"
#include "../Common/Buffer.h"

#include <winevt.h>
#include <winmeta.h>


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities to configure Auditing Policies

extern LIBRARY_EXPORT GUID Audit_ObjectAccess_FirewallPacketDrops_;
extern LIBRARY_EXPORT GUID Audit_ObjectAccess_FirewallConnection_;

extern LIBRARY_EXPORT GUID Audit_ObjectAccess_FirewallRuleChange_;

#define AUDIT_POLICY_INFORMATION_TYPE_NONE    0
#define AUDIT_POLICY_INFORMATION_TYPE_SUCCESS 1
#define AUDIT_POLICY_INFORMATION_TYPE_FAILURE 2

#define WIN_LOG_EVENT_FW_BLOCKED 5157
#define WIN_LOG_EVENT_FW_ALLOWED 5156

#define WIN_LOG_EVENT_FW_OUTBOUN L"%%14593"
#define WIN_LOG_EVENT_FW_INBOUND L"%%14592"

#define WIN_LOG_EVENT_FW_ADDED   4946
#define WIN_LOG_EVENT_FW_CHANGED 4947
#define WIN_LOG_EVENT_FW_REMOVED 4948

LIBRARY_EXPORT bool SetAuditPolicy(const GUID* pSubCategoryGuids, ULONG dwPolicyCount, uint32 AuditingMode, uint32* pOldAuditingMode);
LIBRARY_EXPORT uint32 GetAuditPolicy(const GUID* pSubCategoryGuids, ULONG dwPolicyCount);


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities for custom Custom Event Logging

LIBRARY_EXPORT bool EventLogExists(const std::wstring& logName/*, const std::wstring& machineName = L"."*/);
LIBRARY_EXPORT bool EventSourceExists(const std::wstring& sourceName/*, const std::wstring& machineName = L"."*/);
LIBRARY_EXPORT bool CreateEventSource(const std::wstring& sourceName, std::wstring logName = L"");
LIBRARY_EXPORT bool EmptyEventLog(const std::wstring& logName);
LIBRARY_EXPORT bool DeleteEventLog(const std::wstring& logName);
LIBRARY_EXPORT bool DeleteEventSource(const std::wstring& sourceName);


////////////////////////////////////////////////////////////////////////////////////////////////////////
// CEventLogListener

class LIBRARY_EXPORT CEventLogListener
{
public:
	CEventLogListener();
	virtual ~CEventLogListener();

	virtual bool Start(const std::wstring& XmlQuery);
	virtual void Stop();

	static std::wstring GetEventXml(EVT_HANDLE hEvent);
	static std::vector<BYTE> GetEventData(EVT_HANDLE hEvent, DWORD ValuePathsCount, LPCWSTR* ValuePaths);

	//CVariant GetEvtVariant(const struct _EVT_VARIANT& value, const CVariant& defValue = CVariant());

	static std::wstring GetWinVariantString(const EVT_VARIANT& value, const std::wstring& defValue = std::wstring());

	template <class T>
	static T GetWinVariantNumber(const EVT_VARIANT& value, T defValue = T())
	{
		switch (value.Type)
		{
		case EvtVarTypeBoolean:	return (T)value.BooleanVal;

		case EvtVarTypeSByte:	return (T)value.SByteVal;
		case EvtVarTypeByte:	return (T)value.ByteVal;
		case EvtVarTypeInt16:	return (T)value.Int16Val;
		case EvtVarTypeUInt16:	return (T)value.UInt16Val;
		case EvtVarTypeInt32:	return (T)value.Int32Val;
		case EvtVarTypeUInt32:	return (T)value.UInt32Val;
		case EvtVarTypeInt64:	return (T)value.Int64Val;
		case EvtVarTypeUInt64:	return (T)value.UInt64Val;

		case EvtVarTypeFileTime:return (T)value.FileTimeVal; // UInt64Val

		case EvtVarTypeSingle:	return (T)value.SingleVal;
		case EvtVarTypeDouble:	return (T)value.DoubleVal;
		}
		return defValue;
	}

	static EVT_HANDLE EvtSubscribe(EVT_HANDLE Session, HANDLE SignalEvent, LPCWSTR ChannelPath, LPCWSTR Query, EVT_HANDLE Bookmark, PVOID Context, EVT_SUBSCRIBE_CALLBACK Callback, DWORD Flags);
	static EVT_HANDLE EvtQuery(EVT_HANDLE Session, LPCWSTR Path, LPCWSTR Query, DWORD Flags);
	static BOOL EvtNext(EVT_HANDLE ResultSet, DWORD EventsSize, PEVT_HANDLE Events, DWORD Timeout, DWORD Flags, PDWORD Returned);
	static BOOL EvtSeek(EVT_HANDLE ResultSet, LONGLONG Position, EVT_HANDLE Bookmark, DWORD Timeout, DWORD Flags);
	static EVT_HANDLE EvtCreateRenderContext(DWORD ValuePathsCount, LPCWSTR* ValuePaths, DWORD Flags);
	static BOOL EvtRender(EVT_HANDLE Context, EVT_HANDLE Fragment, DWORD Flags, DWORD BufferSize, PVOID Buffer, PDWORD BufferUsed, PDWORD PropertyCount);
	static BOOL EvtClose(EVT_HANDLE Object);

protected:
	static DWORD WINAPI Callback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent);
	
	virtual void OnEvent(EVT_HANDLE hEvent) = 0;

	HANDLE m_hSubscription;
	
private:
	static struct SEventLogDll* Dll();
	static struct SEventLogDll* m_Dll;
public:
	static void FreeDll();
};


////////////////////////////////////////////////////////////////////////////////////////////////////////
// CEventLogCallback

class LIBRARY_EXPORT CEventLogCallback : public CEventLogListener
{
public:
	CEventLogCallback(void(*pCallback)(PVOID pParam, EVT_HANDLE hEvent), PVOID pParam) {
		m_pCallback = pCallback;
		m_pParam = pParam;
	}

protected:

	virtual void OnEvent(EVT_HANDLE hEvent) { m_pCallback(m_pParam, hEvent); }

	void(*m_pCallback)(PVOID pParam, EVT_HANDLE hEvent);
	PVOID m_pParam;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////
// CEventLogger

class LIBRARY_EXPORT CEventLogger
{
public:
	CEventLogger(const wchar_t* name);
	virtual ~CEventLogger();

	void LogEvent(WORD wType, WORD wCategory, DWORD dwEventID, const std::wstring& string, const CBuffer* pData = NULL);
	void LogEvent(WORD wType, WORD wCategory, DWORD dwEventID, const std::vector<const wchar_t*>& strings, const CBuffer* pData = NULL);

	void LogEventLine(WORD wType, WORD wCategory, DWORD dwEventID, const wchar_t* pFormat, ...);
	void LogEventLine(WORD wType, WORD wCategory, DWORD dwEventID, const CBuffer* pData, const wchar_t* pFormat, ...);

	static void ClearLog(const wchar_t* name);

protected:

	void LogEventLine(WORD wType, WORD wCategory, DWORD dwEventID, const CBuffer* pData, const wchar_t* pFormat, va_list ArgList);

	HANDLE m_Handle;

};