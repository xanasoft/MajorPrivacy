#include "pch.h"
#include "ResLogEntry.h"
#include "../Library/API/PrivacyAPI.h"

CResLogEntry::CResLogEntry()
{
}

enum class EResAccessClass
{
    eNone = 0,
    eReadAttributes,
    eWriteAttributes,
    eReadData,
	eWriteData,
    eChangePermissions,
};

EResAccessClass CResLogEntry__ClassifyAccess(quint32 uAccessMask)
{
    if((uAccessMask & GENERIC_ALL) == GENERIC_ALL
    || (uAccessMask & GENERIC_WRITE) == GENERIC_WRITE
    || (uAccessMask & DELETE) == DELETE
    || (uAccessMask & FILE_WRITE_DATA) == FILE_WRITE_DATA
    || (uAccessMask & FILE_APPEND_DATA) == FILE_APPEND_DATA)
        return EResAccessClass::eWriteData;

    if((uAccessMask & WRITE_DAC) == WRITE_DAC
	|| (uAccessMask & WRITE_OWNER) == WRITE_OWNER)
		return EResAccessClass::eChangePermissions;

    if((uAccessMask & FILE_WRITE_ATTRIBUTES) == FILE_WRITE_ATTRIBUTES
    || (uAccessMask & FILE_WRITE_EA) == FILE_WRITE_EA)
        return EResAccessClass::eWriteAttributes;

    if((uAccessMask & GENERIC_READ) == GENERIC_READ
    || (uAccessMask & GENERIC_EXECUTE) == GENERIC_EXECUTE
    || (uAccessMask & READ_CONTROL) == READ_CONTROL
    || (uAccessMask & FILE_READ_DATA) == FILE_READ_DATA
    || (uAccessMask & FILE_EXECUTE) == FILE_EXECUTE)
        return EResAccessClass::eReadData;

    if((uAccessMask & FILE_READ_ATTRIBUTES) == FILE_READ_ATTRIBUTES
    || (uAccessMask & FILE_READ_EA) == FILE_READ_EA)
    // (uAccessMask & SYNCHRONIZE) == SYNCHRONIZE
        return EResAccessClass::eReadAttributes;

    return EResAccessClass::eNone;
}

QColor CResLogEntry::GetAccessColor(quint32 uAccessMask, bool bStrong)
{
    switch (CResLogEntry__ClassifyAccess(uAccessMask))
    {
    case EResAccessClass::eChangePermissions: 
    case EResAccessClass::eWriteData:           return bStrong ? QColor(Qt::red) : QColor(255, 192, 192);
	case EResAccessClass::eReadData:            return bStrong ? QColor(Qt::green) : QColor(144, 238, 144);
    case EResAccessClass::eWriteAttributes:     return bStrong ? QColor(255, 165, 0) : QColor(255, 223, 128);
	case EResAccessClass::eReadAttributes:      return bStrong ? QColor(Qt::blue) : QColor(173, 216, 230);
    default: return QColor();
    }
}

QString CResLogEntry::GetAccessStr(quint32 uAccessMask)
{
    switch (CResLogEntry__ClassifyAccess(uAccessMask))
    {
	case EResAccessClass::eChangePermissions:   return QObject::tr("Change Permissions");
    case EResAccessClass::eWriteData:           return QObject::tr("Write Data");
	case EResAccessClass::eReadData:            return QObject::tr("Read Data");
    case EResAccessClass::eWriteAttributes:     return QObject::tr("Write Attributes");
	case EResAccessClass::eReadAttributes:      return QObject::tr("Read Attributes");
	default: return "";
    }
}

QString CResLogEntry::GetAccessStr() const
{
	return GetAccessStr(m_AccessMask);
}

QString CResLogEntry::GetAccessStrEx(quint32 uAccessMask)
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

QString CResLogEntry::GetAccessStrEx() const
{
	return GetAccessStrEx(m_AccessMask);
}

void CResLogEntry::ReadValue(uint32 Index, const XVariant& Data)
{
    switch (Index)
    {
    case API_V_FILE_NT_PATH:			m_NtPath = Data.AsQStr(); break;
    case API_V_ACCESS_MASK:	            m_AccessMask = Data.To<uint32>(); break;
    case API_V_EVENT_STATUS:		    m_Status = (EEventStatus)Data.To<uint32>(); break;
    case API_V_NT_STATUS:		        m_NtStatus = Data.To<uint32>(); break;
    default: CAbstractLogEntry::ReadValue(Index, Data);
    }
}

QString CResLogEntry::GetStatusStr() const
{ 
    QString Status = (m_Status == EEventStatus::eAllowed ? QObject::tr("Allowed") : QObject::tr("Blocked"));
    if(m_NtStatus)
        Status += QObject::tr(" (0x%1)").arg(m_NtStatus, 8, 16, QChar('0'));
    return Status;
}

bool CResLogEntry::Match(const CAbstractLogEntry* pEntry) const
{
    if (!CAbstractLogEntry::Match(pEntry))
        return false;

    const CResLogEntry* pResEntry = dynamic_cast<const CResLogEntry*>(pEntry);
	if (!pResEntry)
		return false;

	if (m_NtPath != pResEntry->m_NtPath)
		return false;
	if (m_AccessMask != pResEntry->m_AccessMask)
		return false;
	if (m_Status != pResEntry->m_Status)
		return false;
	if (m_NtStatus != pResEntry->m_NtStatus)
		return false;

	return true;
}