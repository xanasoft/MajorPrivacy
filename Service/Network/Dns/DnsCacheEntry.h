#pragma once
#include "../../Common/AbstractInfo.h"
#include "..\Library\Common\Address.h"
#include "..\Library\Common\Variant.h"

class CDnsCacheEntry: public CAbstractInfoEx
{

public:
	CDnsCacheEntry(const std::wstring& HostName, uint16 Type, const CAddress& Address, const std::wstring& ResolvedString = std::wstring());

	std::wstring GetHostName() const		{ std::shared_lock Lock(m_Mutex); return m_HostName; }
	std::wstring GetResolvedString() const	{ std::shared_lock Lock(m_Mutex); return m_ResolvedString; }
	CAddress GetAddress() const				{ std::shared_lock Lock(m_Mutex); return m_Address; }
	uint16 GetType() const					{ std::shared_lock Lock(m_Mutex); return m_Type; }
	std::wstring GetTypeString() const;
	static std::wstring GetTypeString(uint16 Type);

	uint64 GetTTL() const					{ std::shared_lock Lock(m_Mutex); return m_TTL > 0 ? m_TTL : 0; }
	uint64 GetDeadTime() const				{ std::shared_lock Lock(m_Mutex); return m_TTL < 0 ? -m_TTL : 0; }
	void SetTTL(uint64 TTL);
	void SubtractTTL(uint64 Delta);

	uint64 GetQueryCounter() const			{ std::shared_lock Lock(m_Mutex); return m_QueryCounter; }

	CVariant ToVariant() const;

protected:

	std::wstring	m_HostName;
	std::wstring	m_ResolvedString;
	CAddress		m_Address;
	uint16			m_Type = 0;
	sint64			m_TTL = 0;

	uint32			m_QueryCounter = 0;
};

typedef std::shared_ptr<CDnsCacheEntry> CDnsCacheEntryPtr;
typedef std::weak_ptr<CDnsCacheEntry> CDnsCacheEntryRef;