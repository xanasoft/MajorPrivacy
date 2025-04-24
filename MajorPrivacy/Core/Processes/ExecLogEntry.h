#pragma once
#include "../TraceLogEntry.h"

#include "../../Library/API/PrivacyDefs.h"

enum class EProcAccessClass
{
	eNone = 0,
	eReadProperties,
	eWriteProperties,
	eSpecial,
	eReadMemory,
	eWriteMemory,
	eChangePermissions,
	eControlExecution,
};

class CExecLogEntry: public CAbstractLogEntry
{
public:
	CExecLogEntry();

	EExecLogRole GetRole() const		{ return m_Role; }
	QString GetRoleStr() const;
	EExecLogType GetType() const		{ return m_Type; }
	QString GetTypeStr() const;
	QString GetTypeStrEx() const;
	QColor GetTypeColor() const;
	EEventStatus GetStatus() const		{ return m_Status; }
	QString GetStatusStr() const;
	quint64 GetMiscID() const			{ return m_MiscID; }
	QFlexGuid GetOtherEnclave() const	{ return m_OtherEnclave; }
	quint32 GetAccessMask() const		{ return m_AccessMask; }

	static EProcAccessClass ClassifyProcessAccess(quint32 uAccessMask);
	static EProcAccessClass ClassifyThreadAccess(quint32 uAccessMask);

	static QStringList GetProcessPermissions(quint32 uAccessMask);
	static QStringList GetThreadPermissions(quint32 uAccessMask);

	static QString GetRoleStr(EExecLogRole Role);
	static QString GetTypeStr(EExecLogType Type);
	static QColor GetAccessColor(quint32 uProcessAccessMask, quint32 uThreadAccessMask);
	static QColor CExecLogEntry::GetAccessColor(EProcAccessClass eAccess);
	static QString GetAccessStr(quint32 uProcessAccessMask, quint32 uThreadAccessMask);
	static QString GetAccessStrEx(quint32 uProcessAccessMask, quint32 uThreadAccessMask);

	bool Match(const CAbstractLogEntry* pEntry) const override;

protected:

	virtual void ReadValue(uint32 Index, const QtVariant& Data);

	EExecLogRole		m_Role = EExecLogRole::eUndefined;
	EExecLogType		m_Type = EExecLogType::eUnknown;
	EEventStatus		m_Status = EEventStatus::eUndefined;
	quint64				m_MiscID = 0; // Image UID or program file UID
	QFlexGuid			m_OtherEnclave;
	quint32				m_AccessMask = 0;
	uint32				m_NtStatus = 0;

};

typedef std::shared_ptr<CExecLogEntry> CExecLogEntryPtr;
