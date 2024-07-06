#include "pch.h"
#include "ExecLogEntry.h"
#include "../Library/API/PrivacyAPI.h"

CExecLogEntry::CExecLogEntry()
{
}

QString CExecLogEntry::GetRoleStr() const
{
	switch (m_Role) // todo tr
	{
	case EExecLogRole::eBooth: return "Booth";
	case EExecLogRole::eActor: return "Actor";
	case EExecLogRole::eTarget: return "Target";
	default: return "Unknown";
	}
}

QString CExecLogEntry::GetTypeStr() const
{
	QString Type;
	switch (m_Type) // todo tr
	{
	case EExecLogType::eProcessStarted:		Type = "Process Started"; break;
	case EExecLogType::eImageLoad:			Type = "Image Load"; break;
	case EExecLogType::eThreadAccess:		Type = "Thread Access"; break;
	case EExecLogType::eProcessAccess:		Type = "Process Access"; break;
	default: Type = "Unknown";
	}

	if (m_Type == EExecLogType::eProcessAccess)
	{
		return Type + QString(" (%1)").arg(GetProcessPermissions(m_AccessMask).join(", "));
	}
	else if(m_Type == EExecLogType::eThreadAccess)
	{
		return Type + QObject::tr(" (%1)").arg(GetThreadPermissions(m_AccessMask).join(", "));
	}
	return Type;
}

QString CExecLogEntry::GetStatusStr() const
{
	switch (m_Status) // todo tr
	{
	case EEventStatus::eAllowed:	return QObject::tr("Allowed");
	case EEventStatus::eUntrusted:	return QObject::tr("Allowed (Untrusted)");
	case EEventStatus::eEjected:	return QObject::tr("Allowed (Ejected)");
	case EEventStatus::eProtected:
	case EEventStatus::eBlocked:	return QObject::tr("Blocked");
	default: return "Unknown";
	}
}

void CExecLogEntry::ReadValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_PROC_EVENT_ROLE:			m_Role = (EExecLogRole)Data.To<uint32>(); break;
	case API_V_PROC_EVENT_TYPE:			m_Type = (EExecLogType)Data.To<uint32>(); break;
	case API_V_PROC_EVENT_STATUS:		m_Status = (EEventStatus)Data.To<uint32>(); break;
	case API_V_PROC_EVENT_MISC:			m_MiscID = Data.To<uint64>(); break;
	case API_V_PROC_EVENT_ACCESS_MASK:	m_AccessMask = Data.To<uint32>(); break;
	default: CAbstractLogEntry::ReadValue(Index, Data);
	}
}

QStringList CExecLogEntry::GetProcessPermissions(quint32 uAccessMask)
{
	QStringList permissions;

	if ((uAccessMask & PROCESS_ALL_ACCESS) == PROCESS_ALL_ACCESS) {									permissions << "ALL_ACCESS";			uAccessMask &= ~PROCESS_ALL_ACCESS; }		
	if ((uAccessMask & PROCESS_CREATE_PROCESS) == PROCESS_CREATE_PROCESS) {							permissions << "CREATE_PROCESS";		uAccessMask &= ~PROCESS_CREATE_PROCESS; }
#define PROCESS_DEBUG (PROCESS_CREATE_THREAD | PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION \
							| PROCESS_SUSPEND_RESUME | PROCESS_TERMINATE | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE)
	if ((uAccessMask & PROCESS_DEBUG) == PROCESS_DEBUG) {											permissions << "DEBUG";					uAccessMask &= ~PROCESS_DEBUG;						goto skip_dbg; }
	if ((uAccessMask & PROCESS_CREATE_THREAD) == PROCESS_CREATE_THREAD) {							permissions << "CREATE_THREAD";			uAccessMask &= ~PROCESS_CREATE_THREAD; }
	if ((uAccessMask & PROCESS_DUP_HANDLE) == PROCESS_DUP_HANDLE) {									permissions << "DUP_HANDLE";			uAccessMask &= ~PROCESS_DUP_HANDLE; }
	if ((uAccessMask & PROCESS_QUERY_INFORMATION) == PROCESS_QUERY_INFORMATION) {					permissions << "QUERY";					uAccessMask &= ~PROCESS_QUERY_INFORMATION; }
	if ((uAccessMask & PROCESS_QUERY_LIMITED_INFORMATION) == PROCESS_QUERY_LIMITED_INFORMATION) {	permissions << "QUERY_LIMITED";			uAccessMask &= ~PROCESS_QUERY_LIMITED_INFORMATION; }
	if ((uAccessMask & PROCESS_SUSPEND_RESUME) == PROCESS_SUSPEND_RESUME) {							permissions << "SUSPEND_RESUME";		uAccessMask &= ~PROCESS_SUSPEND_RESUME; }
	if ((uAccessMask & PROCESS_TERMINATE) == PROCESS_TERMINATE) {									permissions << "TERMINATE";				uAccessMask &= ~PROCESS_TERMINATE; }
	if ((uAccessMask & PROCESS_VM_OPERATION) == PROCESS_VM_OPERATION) {								permissions << "VM_OPERATION";			uAccessMask &= ~PROCESS_VM_OPERATION; }
	if ((uAccessMask & PROCESS_VM_READ) == PROCESS_VM_READ) {										permissions << "VM_READ";				uAccessMask &= ~PROCESS_VM_READ; }
	if ((uAccessMask & PROCESS_VM_WRITE) == PROCESS_VM_WRITE) {										permissions << "VM_WRITE";				uAccessMask &= ~PROCESS_VM_WRITE; }
skip_dbg:
	if ((uAccessMask & PROCESS_SET_INFORMATION) == PROCESS_SET_INFORMATION) {						permissions << "SET";					uAccessMask &= ~PROCESS_SET_INFORMATION; }
	if ((uAccessMask & PROCESS_SET_LIMITED_INFORMATION) == PROCESS_SET_LIMITED_INFORMATION) {		permissions << "SET_LIMITED";			uAccessMask &= ~PROCESS_SET_LIMITED_INFORMATION; }
	if ((uAccessMask & PROCESS_SET_QUOTA) == PROCESS_SET_QUOTA) {									permissions << "SET_QUOTA";				uAccessMask &= ~PROCESS_SET_QUOTA; }
	if ((uAccessMask & SYNCHRONIZE) == SYNCHRONIZE) {												permissions << "SYNCHRONIZE";			uAccessMask &= ~SYNCHRONIZE; }
	if ((uAccessMask & DELETE) == DELETE) {															permissions << "DELETE";				uAccessMask &= ~DELETE; }
	if ((uAccessMask & READ_CONTROL) == READ_CONTROL) {												permissions << "READ_CONTROL";			uAccessMask &= ~READ_CONTROL; }
	if ((uAccessMask & WRITE_DAC) == WRITE_DAC) { 													permissions << "WRITE_DAC";				uAccessMask &= ~WRITE_DAC; }
	if ((uAccessMask & WRITE_OWNER) == WRITE_OWNER) {												permissions << "WRITE_OWNER";			uAccessMask &= ~WRITE_OWNER; }

	if (uAccessMask)
		permissions << ("P:0x" + QString::number(uAccessMask, 16));

	return permissions;
}

QStringList CExecLogEntry::GetThreadPermissions(quint32 uAccessMask)
{
	QStringList permissions;

	if ((uAccessMask & THREAD_ALL_ACCESS) == THREAD_ALL_ACCESS) {									permissions << "ALL_ACCESS";			uAccessMask &= ~THREAD_ALL_ACCESS; }
	if ((uAccessMask & THREAD_DIRECT_IMPERSONATION) == THREAD_DIRECT_IMPERSONATION) {				permissions << "DIRECT_IMPERSONATION";	uAccessMask &= ~THREAD_DIRECT_IMPERSONATION; }
	if ((uAccessMask & THREAD_GET_CONTEXT) == THREAD_GET_CONTEXT) {									permissions << "GET_CONTEXT";			uAccessMask &= ~THREAD_GET_CONTEXT; }
	if ((uAccessMask & THREAD_IMPERSONATE) == THREAD_IMPERSONATE) {									permissions << "IMPERSONATE";			uAccessMask &= ~THREAD_IMPERSONATE; }
	if ((uAccessMask & THREAD_QUERY_INFORMATION) == THREAD_QUERY_INFORMATION) {						permissions << "QUERY";					uAccessMask &= ~THREAD_QUERY_INFORMATION; }
	if ((uAccessMask & THREAD_QUERY_LIMITED_INFORMATION) == THREAD_QUERY_LIMITED_INFORMATION) {		permissions << "QUERY_LIMITED";			uAccessMask &= ~THREAD_QUERY_LIMITED_INFORMATION; }
	if ((uAccessMask & THREAD_SET_CONTEXT) == THREAD_SET_CONTEXT) {									permissions << "SET_CONTEXT";			uAccessMask &= ~THREAD_SET_CONTEXT; }
	if ((uAccessMask & THREAD_SET_INFORMATION) == THREAD_SET_INFORMATION) {							permissions << "SET";					uAccessMask &= ~THREAD_SET_INFORMATION; }
	if ((uAccessMask & THREAD_SET_LIMITED_INFORMATION) == THREAD_SET_LIMITED_INFORMATION) {			permissions << "SET_LIMITED";			uAccessMask &= ~THREAD_SET_LIMITED_INFORMATION; }
	if ((uAccessMask & THREAD_SET_THREAD_TOKEN) == THREAD_SET_THREAD_TOKEN) {						permissions << "SET_THREAD_TOKEN";		uAccessMask &= ~THREAD_SET_THREAD_TOKEN; }
	if ((uAccessMask & THREAD_SUSPEND_RESUME) == THREAD_SUSPEND_RESUME) {							permissions << "SUSPEND_RESUME";		uAccessMask &= ~THREAD_SUSPEND_RESUME; }
	if ((uAccessMask & THREAD_RESUME) == THREAD_RESUME) {											permissions << "RESUME";				uAccessMask &= ~THREAD_RESUME; }
	if ((uAccessMask & THREAD_TERMINATE) == THREAD_TERMINATE) {										permissions << "TERMINATE";				uAccessMask &= ~THREAD_TERMINATE; }
	if ((uAccessMask & SYNCHRONIZE) == SYNCHRONIZE) {												permissions << "SYNCHRONIZE";			uAccessMask &= ~SYNCHRONIZE; }
	if ((uAccessMask & DELETE) == DELETE) {															permissions << "DELETE";				uAccessMask &= ~DELETE; }
	if ((uAccessMask & READ_CONTROL) == READ_CONTROL) {												permissions << "READ_CONTROL";			uAccessMask &= ~READ_CONTROL; }
	if ((uAccessMask & WRITE_DAC) == WRITE_DAC) { 													permissions << "WRITE_DAC";				uAccessMask &= ~WRITE_DAC; }
	if ((uAccessMask & WRITE_OWNER) == WRITE_OWNER) {												permissions << "WRITE_OWNER";			uAccessMask &= ~WRITE_OWNER; }

	if (uAccessMask)
		permissions << ("T:0x" + QString::number(uAccessMask, 16));

	return permissions;
}