#include "pch.h"
#include "HashDB.h"
#include "../ServiceCore.h"
#include "../Processes/ProcessList.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/NtIo.h"
#include "../Library/Helpers/NtObj.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Network/Firewall/Firewall.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Common/Exception.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/EvtUtil.h"

CHashDB::CHashDB()
{
}

CHashDB::~CHashDB()
{
}

STATUS CHashDB::Init()
{
	theCore->Driver()->RegisterConfigEventHandler(EConfigGroup::eHashDB, &CHashDB::OnHashChanged, this);
	theCore->Driver()->RegisterForConfigEvents(EConfigGroup::eHashDB);
	LoadHashs();

	return OK;
}

void CHashDB::LoadHashs()
{
	/*StVariant Request;
	auto Res = theCore->Driver()->Call(API_GET_HashS, Request);
	if (Res.IsSuccess()) {
		StVariant Hashs = Res.GetValue();
		for (uint32 i = 0; i < Hashs.Count(); i++)
		{
			StVariant Hash = Hashs[i];
			std::wstring Guid = Hash[API_V_GUID].AsStr();
			OnHashChanged(Guid, EConfigEvent::eAdded, EConfigGroup::eHashs, 0);
		}
	}*/
}

void CHashDB::OnHashChanged(const std::wstring& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID)
{
	ASSERT(Type == EConfigGroup::eHashDB);

	if(Event == EConfigEvent::eAllChanged)
		LoadHashs();
	else
	{
		
	}

	StVariant vEvent;
	vEvent[API_V_GUID] = Guid;
	//vEvent[API_V_NAME] = ;
	vEvent[API_V_EVENT_TYPE] = (uint32)Event;

	theCore->BroadcastMessage(SVC_API_EVENT_HASHDB_CHANGED, vEvent);
}