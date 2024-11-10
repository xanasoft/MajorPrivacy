#pragma once
#include "../TraceLogEntry.h"

#include "../../Library/API/PrivacyDefs.h"

class CExecLogEntry: public CAbstractLogEntry
{
public:
	CExecLogEntry();

	EExecLogRole GetRole() const { return m_Role; }
	QString GetRoleStr() const;
	EExecLogType GetType() const { return m_Type; }
	QString GetTypeStr() const;
	EEventStatus GetStatus() const { return m_Status; }
	QString GetStatusStr() const;
	quint64 GetMiscID() const { return m_MiscID; }
	quint32 GetAccessMask() const { return m_AccessMask; }

	static QStringList GetProcessPermissions(quint32 AccessMask);
	static QStringList GetThreadPermissions(quint32 AccessMask);

protected:

	virtual void ReadValue(uint32 Index, const XVariant& Data);

	EExecLogRole		m_Role = EExecLogRole::eUndefined;
	EExecLogType		m_Type = EExecLogType::eUnknown;
	EEventStatus		m_Status = EEventStatus::eUndefined;
	quint64				m_MiscID = 0; // Image UID or program file UID
	quint32				m_AccessMask = 0;
	uint32				m_NtStatus = 0;

};

typedef std::shared_ptr<CExecLogEntry> CExecLogEntryPtr;
