#pragma once
#include "../Library/Helpers/MiscHelpers.h"
#include "../Library/Common/DbgHelp.h"

enum class EDnsSource
{
	eNone            = 0x00,
	eReverseDns      = 0x01,
	eCachedQuery     = 0x02, // good
	eCapturedQuery   = 0x04  // best
};

class CHostName 
{
	TRACK_OBJECT(CHostName)
public:
	virtual ~CHostName() {}

	virtual std::wstring ToString() const { return m_HostName; }

protected:
	CHostName() {}

	std::wstring m_HostName;
	//std::wstring m_HostNameAlias;
	EDnsSource m_Source = EDnsSource::eNone;
};

typedef std::shared_ptr<CHostName> CHostNamePtr;
typedef std::weak_ptr<CHostName> CHostNameRef;

class CResHostName : public CHostName 
{
	TRACK_OBJECT(CResHostName)
public:
	CResHostName(uint64 TimeStamp) {
		m_TimeStamp = TimeStamp;
	}

	virtual std::wstring ToString() const { std::shared_lock Lock(m_Mutex); return m_HostName; }

	virtual void Resolve(const std::wstring& HostName, EDnsSource Source, uint64 SourceTimeStamp) 
	{
		std::unique_lock Lock(m_Mutex); 
		if (Source > m_Source || (Source == EDnsSource::eCapturedQuery									// we may set an old captured value right away, but shortly therafter get a new etw event
			&& SourceTimeStamp > m_SourceTimeStamp														// if we get a result with a newer time stamp
			&& (SourceTimeStamp < m_TimeStamp || SourceTimeStamp - m_TimeStamp < (1 * 1000 * 10000ULL))	// and its not unplausibly new for this event, we replace the old value with the new one
			)) 
		{
			if (m_HostName != HostName) {
				//if (Source == EDnsSource::eCachedQuery)
				//	m_HostNameAlias = m_HostName;
				m_HostName = HostName;
			}
			m_Source = Source;
			m_SourceTimeStamp = SourceTimeStamp;
#ifdef _DEBUG
			//DbgPrint(L"Resolved Host Name %s (%d)\n", HostName.c_str(), Source);
#endif
		}
	}

protected:
	mutable std::shared_mutex m_Mutex;
	uint64 m_TimeStamp = 0;
	uint64 m_SourceTimeStamp = 0;
};