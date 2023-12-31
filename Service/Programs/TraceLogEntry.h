#pragma once
#include "../Library/Common/Variant.h"

enum class ETraceLogs
{
	eExecLog = 0,
	eNetLog,
	eFSLog,
	eRegLog,
	eLogMax
};

class CTraceLogEntry
{
public:
	CTraceLogEntry() {}
	virtual ~CTraceLogEntry() {}

	virtual CVariant ToVariant() const;

protected:

	virtual void WriteVariant(CVariant& Entry) const;

	std::wstring		m_ServiceTag;
	std::wstring		m_AppSid;

	uint64              m_TimeStamp = 0;
};

typedef std::shared_ptr<CTraceLogEntry> CTraceLogEntryPtr;
