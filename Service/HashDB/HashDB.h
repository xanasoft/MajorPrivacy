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

	void LoadHashs();

protected:

	mutable std::mutex m_Mutex;
	
	void OnHashChanged(const std::wstring& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID);
};