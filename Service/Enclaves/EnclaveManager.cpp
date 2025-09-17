#include "pch.h"
#include "EnclaveManager.h"
#include "../ServiceCore.h"
#include "../Processes/ProcessList.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/NtIo.h"
#include "../Library/Helpers/NtObj.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Network/NetworkManager.h"
#include "../Network/Firewall/Firewall.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Common/Exception.h"
#include "../Library/Helpers/EvtUtil.h"
#include "../Volumes/VolumeManager.h"

CEnclaveManager::CEnclaveManager()
{
}

CEnclaveManager::~CEnclaveManager()
{
}

STATUS CEnclaveManager::Init()
{
	theCore->Driver()->RegisterConfigEventHandler(EConfigGroup::eEnclaves, &CEnclaveManager::OnEnclaveChanged, this);
	theCore->Driver()->RegisterForConfigEvents(EConfigGroup::eEnclaves);
	LoadEnclaves();

	return OK;
}

void CEnclaveManager::Update()
{
	if (m_UpdateAllEnclaves)
		LoadEnclaves();
}

STATUS CEnclaveManager::LoadEnclaves()
{
	StVariant Request;

	auto Res = theCore->Driver()->Call(API_GET_ENCLAVES, Request);
	if(Res.IsError())
		return Res;

	std::unique_lock lock(m_Mutex);

	std::map<CFlexGuid, CEnclavePtr> OldEnclaves = m_Enclaves;

	const StVariant Enclaves = Res.GetValue().At(API_V_ENCLAVES);
	for(uint32 i=0; i < Enclaves.Count(); i++)
	{
		StVariant Enclave = Enclaves[i];

		CFlexGuid Guid;
		Guid.FromVariant(Enclave[API_V_GUID]);
		
		CEnclavePtr pEnclave;
		auto I = OldEnclaves.find(Guid);
		if (I != OldEnclaves.end())
		{
			pEnclave = I->second;
			OldEnclaves.erase(I);
		}

		if (!pEnclave) 
		{
			pEnclave = std::make_shared<CEnclave>();
			pEnclave->FromVariant(Enclave);
			m_Enclaves.insert(std::make_pair(pEnclave->GetGuid(), pEnclave));
		}
		else
			pEnclave->FromVariant(Enclave);
	}

	for (auto I : OldEnclaves)
		m_Enclaves.erase(I.second->GetGuid());

	m_UpdateAllEnclaves = false;
	return OK;
}

std::map<CFlexGuid, CEnclavePtr> CEnclaveManager::GetAllEnclaves()
{
	std::unique_lock Lock(m_Mutex);
	return m_Enclaves;
}

CEnclavePtr CEnclaveManager::GetEnclave(const CFlexGuid& Guid)
{
	std::unique_lock Lock(m_Mutex);
	auto F = m_Enclaves.find(Guid);
	if (F != m_Enclaves.end())
		return F->second;
	return nullptr;
}

STATUS CEnclaveManager::AddEnclave(const CEnclavePtr& pEnclave, uint64 LockdownToken)
{
	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

	StVariant Request;
	Request[API_V_ENCLAVE] = pEnclave->ToVariant(Opts);
	if (LockdownToken)
		Request[API_V_TOKEN] = LockdownToken;
	return theCore->Driver()->Call(API_SET_ENCLAVE, Request);
}

STATUS CEnclaveManager::RemoveEnclave(const CEnclavePtr& pEnclave, uint64 LockdownToken)
{
	StVariant Request;
	Request[API_V_GUID] = pEnclave->GetGuid().ToVariant(true);
	if (LockdownToken)
		Request[API_V_TOKEN] = LockdownToken;
	return theCore->Driver()->Call(API_DEL_ENCLAVE, Request);
}

void CEnclaveManager::OnEnclaveChanged(const CFlexGuid& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID)
{
	ASSERT(Type == EConfigGroup::eEnclaves);

	if(Event == EConfigEvent::eAllChanged)
		LoadEnclaves();
	else
	{
		CEnclavePtr pEnclave;
		if (Event != EConfigEvent::eRemoved) {
			StVariant Request;
			Request[API_V_GUID] = Guid.ToVariant(true);
			auto Res = theCore->Driver()->Call(API_GET_ENCLAVE, Request);
			if (Res.IsSuccess()) {
				const StVariant Enclave = Res.GetValue().At(API_V_ENCLAVE);
				pEnclave = std::make_shared<CEnclave>();
				pEnclave->FromVariant(Enclave);
			}
		}
		
		CEnclavePtr pEnclave2 = pEnclave;
		if (Event == EConfigEvent::eRemoved)
			pEnclave2 = GetEnclave(Guid);
		if (pEnclave2)
			theCore->VolumeManager()->UpdateEnclave(pEnclave2, Event, PID);

		std::unique_lock Lock(m_Mutex);

		CEnclavePtr pOldEnclave;
		if (!Guid.IsNull()) {
			auto F = m_Enclaves.find(Guid);
			if (F != m_Enclaves.end())
				pOldEnclave = F->second;
		}

		if (pEnclave && pOldEnclave)
			pOldEnclave->Update(pEnclave);
		else {
			if (pOldEnclave) m_Enclaves.erase(pOldEnclave->GetGuid());
			if (pEnclave) m_Enclaves.insert(std::make_pair(pEnclave->GetGuid(), pEnclave));
		}
	}

	StVariant vEvent;
	vEvent[API_V_GUID] = Guid.ToVariant(false);
	//vEvent[API_V_NAME] = ;
	vEvent[API_V_EVENT_TYPE] = (uint32)Event;

	theCore->BroadcastMessage(SVC_API_EVENT_ENCLAVE_CHANGED, vEvent);
}