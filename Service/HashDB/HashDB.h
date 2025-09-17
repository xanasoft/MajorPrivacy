#pragma once
#include "../Library/Status.h"
#include "HashEntry.h"

class CHashDB
{
	TRACK_OBJECT(CHashDB)
public:
	CHashDB();
	~CHashDB();

	STATUS Init();

	void Update();

	STATUS LoadEntries();

	std::map<CBuffer, CHashPtr> GetAllEntries() const;
	CHashPtr GetEntry(const CBuffer& HashValue);
	STATUS SetEntry(const CHashPtr& pEntry, uint64 LockdownToken = 0);
	STATUS RemoveEntry(const CHashPtr& pEntry, uint64 LockdownToken = 0);

protected:

	void OnHashChanged(const std::wstring& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID);

	mutable std::mutex m_Mutex;
	bool								m_UpdateAllEntries = true;
	std::map<CBuffer, CHashPtr>			m_Entries;
};