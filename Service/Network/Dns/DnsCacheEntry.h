#pragma once
#include "../../Common/AbstractInfo.h"
#include "..\Library\Common\Address.h"
#include "..\Library\Common\StVariant.h"

class CDnsCacheEntry: public CAbstractInfoEx
{
	TRACK_OBJECT(CDnsCacheEntry)
public:
	enum EStatus
	{
		eNone = 0,
		eCached = 1,
		eAllowed = 2,
		eBlocked = 4,
	};

	CDnsCacheEntry(const std::wstring& HostName, uint16 Type, const CAddress& Address, const std::wstring& ResolvedString = std::wstring(), EStatus Status = eCached);

	std::wstring GetHostName() const		{ std::shared_lock Lock(m_Mutex); return m_HostName; }
	std::wstring GetResolvedString() const	{ std::shared_lock Lock(m_Mutex); return m_ResolvedString; }
	CAddress GetAddress() const				{ std::shared_lock Lock(m_Mutex); return m_Address; }
	uint16 GetType() const					{ std::shared_lock Lock(m_Mutex); return m_Type; }

	uint64 GetTTL() const					{ std::shared_lock Lock(m_Mutex); return m_TTL > 0 ? m_TTL : 0; }
	uint64 GetDeadTime() const				{ std::shared_lock Lock(m_Mutex); return m_TTL < 0 ? -m_TTL : 0; }
	void SetTTL(uint64 TTL);
	void SubtractTTL(uint64 Delta, bool bKill);

	EStatus GetStatus() const				{ std::shared_lock Lock(m_Mutex); return m_Status; }

	uint64 GetQueryCounter() const			{ std::shared_lock Lock(m_Mutex); return m_QueryCounter; }

	StVariant ToVariant(FW::AbstractMemPool* pMemPool = nullptr) const;

protected:

	std::wstring	m_HostName;
	std::wstring	m_ResolvedString;
	CAddress		m_Address;
	uint16			m_Type = 0;
	sint64			m_TTL = 0;

	EStatus			m_Status = eNone;

	uint32			m_QueryCounter = 0;
};

typedef std::shared_ptr<CDnsCacheEntry> CDnsCacheEntryPtr;
typedef std::weak_ptr<CDnsCacheEntry> CDnsCacheEntryRef;