#include "pch.h"
#include "ResLogEntry.h"
#include "../Library/API/PrivacyAPI.h"

CResLogEntry::CResLogEntry()
{
}

QString CResLogEntry::GetAccessStr(quint32 uAccessMask)
{
    QStringList permissions;

    if ((uAccessMask & GENERIC_READ) == GENERIC_READ) {                     permissions << "GENERIC_READ"; uAccessMask &= ~GENERIC_READ; }
    if ((uAccessMask & GENERIC_WRITE) == GENERIC_WRITE) {                   permissions << "GENERIC_WRITE"; uAccessMask &= ~GENERIC_WRITE; }
    if ((uAccessMask & GENERIC_EXECUTE) == GENERIC_EXECUTE) {               permissions << "GENERIC_EXECUTE"; uAccessMask &= ~GENERIC_EXECUTE; }
    if ((uAccessMask & GENERIC_ALL) == GENERIC_ALL) {                       permissions << "GENERIC_ALL"; uAccessMask &= ~GENERIC_ALL; }
    if ((uAccessMask & DELETE) == DELETE) {                                 permissions << "DELETE"; uAccessMask &= ~DELETE; }
    if ((uAccessMask & READ_CONTROL) == READ_CONTROL) {                     permissions << "READ_CONTROL"; uAccessMask &= ~READ_CONTROL; }
    if ((uAccessMask & WRITE_DAC) == WRITE_DAC) {                           permissions << "WRITE_DAC"; uAccessMask &= ~WRITE_DAC; }
    if ((uAccessMask & WRITE_OWNER) == WRITE_OWNER) {                       permissions << "WRITE_OWNER"; uAccessMask &= ~WRITE_OWNER; }
    if ((uAccessMask & SYNCHRONIZE) == SYNCHRONIZE) {                       permissions << "SYNCHRONIZE"; uAccessMask &= ~SYNCHRONIZE; }
    if ((uAccessMask & FILE_READ_DATA) == FILE_READ_DATA) {                 permissions << "FILE_READ_DATA"; uAccessMask &= ~FILE_READ_DATA; }
    if ((uAccessMask & FILE_WRITE_DATA) == FILE_WRITE_DATA) {               permissions << "FILE_WRITE_DATA"; uAccessMask &= ~FILE_WRITE_DATA; }
    if ((uAccessMask & FILE_APPEND_DATA) == FILE_APPEND_DATA) {             permissions << "FILE_APPEND_DATA"; uAccessMask &= ~FILE_APPEND_DATA; }
    if ((uAccessMask & FILE_READ_EA) == FILE_READ_EA) {                     permissions << "FILE_READ_EA"; uAccessMask &= ~FILE_READ_EA; }
    if ((uAccessMask & FILE_WRITE_EA) == FILE_WRITE_EA) {                   permissions << "FILE_WRITE_EA"; uAccessMask &= ~FILE_WRITE_EA; }
    if ((uAccessMask & FILE_EXECUTE) == FILE_EXECUTE) {                     permissions << "FILE_EXECUTE"; uAccessMask &= ~FILE_EXECUTE; }
    if ((uAccessMask & FILE_READ_ATTRIBUTES) == FILE_READ_ATTRIBUTES) {     permissions << "FILE_READ_ATTRIBUTES"; uAccessMask &= ~FILE_READ_ATTRIBUTES; }
    if ((uAccessMask & FILE_WRITE_ATTRIBUTES) == FILE_WRITE_ATTRIBUTES) {   permissions << "FILE_WRITE_ATTRIBUTES"; uAccessMask &= ~FILE_WRITE_ATTRIBUTES; }

    if (uAccessMask)
        permissions << QString::number(uAccessMask, 16);

    return QObject::tr("%1").arg(permissions.join(", "));
}

QString CResLogEntry::GetAccessStr() const
{
	return GetAccessStr(m_AccessMask);
}

void CResLogEntry::ReadValue(uint32 Index, const XVariant& Data)
{
    switch (Index)
    {
    case API_V_EVENT_PATH:				m_Path.Set(Data.AsQStr(), EPathType::eNative); break;
    case API_V_EVENT_ACCESS:		    m_AccessMask = Data.To<uint32>(); break;
    case API_V_EVENT_ACCESS_STATUS:		m_Status = (EEventStatus)Data.To<uint32>(); break;
    case API_V_EVENT_STATUS:		    m_NtStatus = Data.To<uint32>(); break;
    default: CAbstractLogEntry::ReadValue(Index, Data);
    }
}

QString CResLogEntry::GetStatusStr() const
{ 
    return QString("0x%1 (%2)").arg(m_NtStatus, 8, 16, QChar('0')).arg(m_Status == EEventStatus::eAllowed ? QObject::tr("Allowed") : QObject::tr("Blocked"));
}