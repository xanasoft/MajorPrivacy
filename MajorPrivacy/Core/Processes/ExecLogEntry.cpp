#include "pch.h"
#include "ExecLogEntry.h"
#include "../Library/API/PrivacyAPI.h"
#include "../../Windows/AccessRuleWnd.h"

CExecLogEntry::CExecLogEntry()
{
}

QString CExecLogEntry::GetRoleStr() const
{
	return GetRoleStr(m_Role);
}

QString CExecLogEntry::GetRoleStr(EExecLogRole Role)
{
	switch (Role)
	{
	case EExecLogRole::eBooth:				return QObject::tr("Booth");
	case EExecLogRole::eActor:				return QObject::tr("Actor");
	case EExecLogRole::eTarget:				return QObject::tr("Target");
	default: return QObject::tr("Unknown");
	}
}

QString CExecLogEntry::GetTypeStr(EExecLogType Type)
{
	switch (Type)
	{
	case EExecLogType::eProcessStarted:		return QObject::tr("Process Started");
	case EExecLogType::eImageLoad:			return QObject::tr("Image Loaded");
	case EExecLogType::eThreadAccess:		return QObject::tr("Thread Access");
	case EExecLogType::eProcessAccess:		return QObject::tr("Process Access");
	default: return QObject::tr("Unknown");
	}
}

QString CExecLogEntry::GetTypeStr() const
{
	switch (m_Type)
	{
	case EExecLogType::eProcessStarted:		return QObject::tr("Process Started");
	case EExecLogType::eImageLoad:			return QObject::tr("Image Loaded");
	case EExecLogType::eProcessAccess:		return GetAccessStr(m_AccessMask, 0);
	case EExecLogType::eThreadAccess:		return GetAccessStr(0, m_AccessMask);
	default: return QObject::tr("Unknown");
	}
}

QString CExecLogEntry::GetTypeStrEx() const
{
	QString Type = GetTypeStr(m_Type);
	if (m_Type == EExecLogType::eProcessAccess)
		return Type + QString(" (%1)").arg(GetAccessStrEx(m_AccessMask, 0));
	if (m_Type == EExecLogType::eThreadAccess)
		return Type + QString(" (%1)").arg(GetAccessStrEx(0, m_AccessMask));
	return Type;
}

QColor CExecLogEntry::GetTypeColor() const
{
	switch (m_Type)
	{
	case EExecLogType::eProcessStarted:		return QColor(192, 255, 192);
	case EExecLogType::eImageLoad:			return QColor(255, 255, 192);
	case EExecLogType::eProcessAccess:		return GetAccessColor(m_AccessMask, 0);
	case EExecLogType::eThreadAccess:		return GetAccessColor(0, m_AccessMask);
	default: return QColor();
	}
}

QString CExecLogEntry::GetStatusStr() const
{
	return CAccessRuleWnd::GetStatusStr(m_Status);
}

void CExecLogEntry::ReadValue(uint32 Index, const QtVariant& Data)
{
	switch (Index)
	{
	case API_V_EVENT_ROLE:				m_Role = (EExecLogRole)Data.To<uint32>(); break;
	case API_V_EVENT_TYPE:				m_Type = (EExecLogType)Data.To<uint32>(); break;
	case API_V_EVENT_STATUS:			m_Status = (EEventStatus)Data.To<uint32>(); break;
	case API_V_PROC_MISC_ID:			m_MiscID = Data.To<uint64>(); break;
	case API_V_PROC_MISC_ENCLAVE:		m_OtherEnclave.FromVariant(Data); break;
	case API_V_ACCESS_MASK:				m_AccessMask = Data.To<uint32>(); break;
	case API_V_NT_STATUS:				m_NtStatus = Data.To<uint32>(); break;
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

EProcAccessClass CExecLogEntry::ClassifyProcessAccess(quint32 uAccessMask)
{
	if((uAccessMask & PROCESS_ALL_ACCESS) == PROCESS_ALL_ACCESS
	|| (uAccessMask & PROCESS_DUP_HANDLE) == PROCESS_DUP_HANDLE
	|| (uAccessMask & PROCESS_VM_OPERATION) == PROCESS_VM_OPERATION // Allocate/Free virtual memory
	|| (uAccessMask & PROCESS_CREATE_THREAD) == PROCESS_CREATE_THREAD) // Required to create a thread in the process
		return EProcAccessClass::eControlExecution;

	if ((uAccessMask & PROCESS_VM_WRITE) == PROCESS_VM_WRITE)
		return EProcAccessClass::eWriteMemory;

	if((uAccessMask & WRITE_DAC) == WRITE_DAC
	|| (uAccessMask & WRITE_OWNER) == WRITE_OWNER)
		return EProcAccessClass::eChangePermissions;

	if((uAccessMask & PROCESS_VM_READ) == PROCESS_VM_READ)
		return EProcAccessClass::eReadMemory;

	if((uAccessMask & PROCESS_CREATE_PROCESS) == PROCESS_CREATE_PROCESS // Required to use this process as the parent process with
	|| (uAccessMask & PROCESS_SUSPEND_RESUME) == PROCESS_SUSPEND_RESUME
	|| (uAccessMask & PROCESS_TERMINATE) == PROCESS_TERMINATE)
		return EProcAccessClass::eSpecial;
	
	if((uAccessMask & PROCESS_SET_INFORMATION) == PROCESS_SET_INFORMATION
	|| (uAccessMask & PROCESS_SET_LIMITED_INFORMATION) == PROCESS_SET_LIMITED_INFORMATION
	|| (uAccessMask & PROCESS_SET_QUOTA) == PROCESS_SET_QUOTA)
		return EProcAccessClass::eWriteProperties;

	if((uAccessMask & PROCESS_QUERY_INFORMATION) == PROCESS_QUERY_INFORMATION
	|| (uAccessMask & PROCESS_QUERY_LIMITED_INFORMATION) == PROCESS_QUERY_LIMITED_INFORMATION)
	// (uAccessMask & SYNCHRONIZE) == SYNCHRONIZE
		return EProcAccessClass::eReadProperties;

	return EProcAccessClass::eNone;
}

EProcAccessClass CExecLogEntry::ClassifyThreadAccess(quint32 uAccessMask)
{
	if((uAccessMask & THREAD_ALL_ACCESS) == THREAD_ALL_ACCESS
	|| (uAccessMask & THREAD_SET_CONTEXT) == THREAD_SET_CONTEXT)
		return EProcAccessClass::eControlExecution;

	if((uAccessMask & WRITE_DAC) == WRITE_DAC
	|| (uAccessMask & WRITE_OWNER) == WRITE_OWNER)
		return EProcAccessClass::eChangePermissions;

	if((uAccessMask & THREAD_SUSPEND_RESUME) == THREAD_SUSPEND_RESUME
	|| (uAccessMask & THREAD_RESUME) == THREAD_RESUME
	|| (uAccessMask & THREAD_TERMINATE) == THREAD_TERMINATE
	|| (uAccessMask & THREAD_DIRECT_IMPERSONATION) == THREAD_DIRECT_IMPERSONATION
	|| (uAccessMask & THREAD_IMPERSONATE) == THREAD_IMPERSONATE
	|| (uAccessMask & THREAD_SET_THREAD_TOKEN) == THREAD_SET_THREAD_TOKEN)
		return EProcAccessClass::eSpecial;

	if((uAccessMask & THREAD_SET_INFORMATION) == THREAD_SET_INFORMATION
	|| (uAccessMask & THREAD_SET_LIMITED_INFORMATION) == THREAD_SET_LIMITED_INFORMATION)
		return EProcAccessClass::eWriteProperties;

	if((uAccessMask & THREAD_QUERY_INFORMATION) == THREAD_QUERY_INFORMATION
	|| (uAccessMask & THREAD_QUERY_LIMITED_INFORMATION) == THREAD_QUERY_LIMITED_INFORMATION
	|| (uAccessMask & THREAD_GET_CONTEXT) == THREAD_GET_CONTEXT)
	// (uAccessMask & SYNCHRONIZE) == SYNCHRONIZE
		return EProcAccessClass::eReadProperties;

	return EProcAccessClass::eNone;
}

QColor CExecLogEntry::GetAccessColor(quint32 uProcessAccessMask, quint32 uThreadAccessMask)
{
	EProcAccessClass ProcessClass = ClassifyProcessAccess(uProcessAccessMask);
	EProcAccessClass ThreadClass = ClassifyThreadAccess(uThreadAccessMask);

	return GetAccessColor((EProcAccessClass)std::max((int)ProcessClass, (int)ThreadClass));
}

QColor CExecLogEntry::GetAccessColor(EProcAccessClass eAccess)
{
	switch (eAccess)
	{
	case EProcAccessClass::eControlExecution:
	case EProcAccessClass::eChangePermissions:
	case EProcAccessClass::eWriteMemory:		return QColor(255, 192, 192);
	case EProcAccessClass::eReadMemory:			return QColor(173, 216, 230);
	case EProcAccessClass::eSpecial:
	case EProcAccessClass::eWriteProperties:	return QColor(255, 223, 128);
	case EProcAccessClass::eReadProperties:		return QColor(144, 238, 144);
	default: return QColor();
	}
}

QString CExecLogEntry::GetAccessStr(quint32 uProcessAccessMask, quint32 uThreadAccessMask)
{
	EProcAccessClass ProcessClass = ClassifyProcessAccess(uProcessAccessMask);
	EProcAccessClass ThreadClass = ClassifyThreadAccess(uThreadAccessMask);

	switch ((EProcAccessClass)std::max((int)ProcessClass, (int)ThreadClass))
	{
	case EProcAccessClass::eControlExecution:	return QObject::tr("Control Execution");
	case EProcAccessClass::eChangePermissions:	return QObject::tr("Change Permissions");
	case EProcAccessClass::eWriteMemory:		return QObject::tr("Write Memory");
	case EProcAccessClass::eReadMemory:			return QObject::tr("Read Memory");
	case EProcAccessClass::eSpecial:			return QObject::tr("Special Permissions");
	case EProcAccessClass::eWriteProperties:	return QObject::tr("Write Properties");
	case EProcAccessClass::eReadProperties:		return QObject::tr("Read Properties");
	default: return "";
	}
}

QString CExecLogEntry::GetAccessStrEx(quint32 uProcessAccessMask, quint32 uThreadAccessMask)
{
	QStringList Permissions;
	Permissions += GetProcessPermissions(uProcessAccessMask);
	Permissions += GetThreadPermissions(uThreadAccessMask);
	return Permissions.join(", ");
}

bool CExecLogEntry::Match(const CAbstractLogEntry* pEntry) const
{
	if (!CAbstractLogEntry::Match(pEntry))
		return false;

	const CExecLogEntry* pExec = dynamic_cast<const CExecLogEntry*>(pEntry);
	if (!pExec)
		return false;

	if (m_Role != pExec->m_Role)
		return false;
	if (m_Type != pExec->m_Type)
		return false;
	if (m_Status != pExec->m_Status)
		return false;
	if (m_MiscID != pExec->m_MiscID)
		return false;
	if (m_OtherEnclave != pExec->m_OtherEnclave)
		return false;
	if (m_AccessMask != pExec->m_AccessMask)
		return false;
	if (m_NtStatus != pExec->m_NtStatus)
		return false;

	return true;
}