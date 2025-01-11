#pragma once
#include "../TraceLogEntry.h"
#include "../Library/API/PrivacyDefs.h"

class CResLogEntry: public CAbstractLogEntry
{
public:

	CResLogEntry();

	QString GetNtPath() const				{ return m_NtPath; }
	quint32 GetAccess() const				{ return m_AccessMask; }
	QString GetAccessStr() const;
	QString GetAccessStrEx() const;
	EEventStatus GetStatus() const			{ return m_Status; }
	QString GetStatusStr() const;

	static QColor GetAccessColor(quint32 uAccessMask, bool bStrong = false);
	static QString GetAccessStr(quint32 uAccessMask);
	static QString GetAccessStrEx(quint32 uAccessMask);

protected:

	virtual void ReadValue(uint32 Index, const XVariant& Data);

	QString				m_NtPath;
	quint32				m_AccessMask = 0;	
	uint32				m_NtStatus = 0;
	EEventStatus		m_Status = EEventStatus::eUndefined;
};

typedef std::shared_ptr<CResLogEntry> CResLogEntryPtr;
