#pragma once
#include "../TraceLogEntry.h"
#include "../../Helpers/FilePath.h"
#include "../Library/API/PrivacyDefs.h"

class CResLogEntry: public CAbstractLogEntry
{
public:

	CResLogEntry();

	QString GetPath(EPathType Type) const	{ return m_Path.Get(Type); }
	quint32 GetAccess() const				{ return m_AccessMask; }
	QString GetAccessStr() const;
	EEventStatus GetStatus() const			{ return m_Status; }
	QString GetStatusStr() const;

	static QString GetAccessStr(quint32 uAccessMask);

protected:

	virtual void ReadValue(uint32 Index, const XVariant& Data);

	CFilePath			m_Path;
	quint32				m_AccessMask = 0;	
	uint32				m_NtStatus = 0;
	EEventStatus		m_Status = EEventStatus::eUndefined;
};

typedef std::shared_ptr<CResLogEntry> CResLogEntryPtr;
