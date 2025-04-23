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
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/EvtUtil.h"

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

void CEnclaveManager::LoadEnclaves()
{
	/*StVariant Request;
	auto Res = theCore->Driver()->Call(API_GET_ENCLAVES, Request);
	if (Res.IsSuccess()) {
		StVariant Enclaves = Res.GetValue();
		for (uint32 i = 0; i < Enclaves.Count(); i++)
		{
			StVariant Enclave = Enclaves[i];
			std::wstring Guid = Enclave[API_V_GUID].AsStr();
			OnEnclaveChanged(Guid, EConfigEvent::eAdded, EConfigGroup::eEnclaves, 0);
		}
	}*/
}

void CEnclaveManager::OnEnclaveChanged(const std::wstring& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID)
{
	ASSERT(Type == EConfigGroup::eEnclaves);

	if(Event == EConfigEvent::eAllChanged)
		LoadEnclaves();
	else
	{
		
	}

	StVariant vEvent;
	vEvent[API_V_GUID] = Guid;
	//vEvent[API_V_NAME] = ;
	vEvent[API_V_EVENT_TYPE] = (uint32)Event;

	theCore->BroadcastMessage(SVC_API_EVENT_ENCLAVE_CHANGED, vEvent);
}