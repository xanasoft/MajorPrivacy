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

	void Update();

	STATUS LoadEnclaves();

	std::map<CFlexGuid, CEnclavePtr>	GetAllEnclaves();
	CEnclavePtr GetEnclave(const CFlexGuid& Guid);
	STATUS AddEnclave(const CEnclavePtr& pEnclave, uint64 LockdownToken = 0);
	STATUS RemoveEnclave(const CEnclavePtr& pEnclave, uint64 LockdownToken = 0);

protected:
	
	void OnEnclaveChanged(const CFlexGuid& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID);

	mutable std::mutex m_Mutex;
	bool								m_UpdateAllEnclaves = true;
	std::map<CFlexGuid, CEnclavePtr>	m_Enclaves;
};