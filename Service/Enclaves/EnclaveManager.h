#pragma once
#include "../Library/Status.h"
#include "Enclave.h"

class CEnclaveManager
{
	TRACK_OBJECT(CEnclaveManager)
public:
	CEnclaveManager();
	~CEnclaveManager();

	STATUS Init();

	void LoadEnclaves();

protected:

	mutable std::mutex m_Mutex;
	
	void OnEnclaveChanged(const std::wstring& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID);
};