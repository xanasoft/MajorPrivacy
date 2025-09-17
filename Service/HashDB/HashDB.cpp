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
#include "../Volumes/VolumeManager.h"

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
	LoadEntries();

	return OK;
}

void CHashDB::Update()
{
	if (m_UpdateAllEntries)
		LoadEntries();
}

STATUS CHashDB::LoadEntries()
{
	StVariant Request;
	auto Res = theCore->Driver()->Call(API_GET_HASHES, Request);
	if(Res.IsError())
		return Res;

	std::unique_lock lock(m_Mutex);

	std::map<CBuffer, CHashPtr> OldEntries = m_Entries;

	const StVariant Entries = Res.GetValue().At(API_V_ENTRIES);
	for(uint32 i=0; i < Entries.Count(); i++)
	{
		StVariant Entry = Entries[i];

		CBuffer HashValue = Entry[API_V_HASH];

		CHashPtr pEntry;
		auto I = OldEntries.find(HashValue);
		if (I != OldEntries.end())
		{
			pEntry = I->second;
			OldEntries.erase(I);
		}

		if (!pEntry) 
		{
			pEntry = std::make_shared<CHash>();
			pEntry->FromVariant(Entry);
			m_Entries.insert(std::make_pair(pEntry->GetHash(), pEntry));
		}
		else
			pEntry->FromVariant(Entry);
	}

	for (auto I : OldEntries)
		m_Entries.erase(I.second->GetHash());

	m_UpdateAllEntries = false;
	return OK;
}

std::map<CBuffer, CHashPtr> CHashDB::GetAllEntries() const 
{ 
	std::unique_lock lock(m_Mutex); 
	return m_Entries; 
}

CHashPtr CHashDB::GetEntry(const CBuffer& HashValue)
{
	std::unique_lock Lock(m_Mutex);
	auto F = m_Entries.find(HashValue);
	if (F != m_Entries.end())
		return F->second;
	return nullptr;
}

STATUS CHashDB::SetEntry(const CHashPtr& pEntry, uint64 LockdownToken)
{
	if (!pEntry || pEntry->GetHash().GetSize() == 0)
		return ERR(STATUS_INVALID_PARAMETER);

	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

	StVariant Request;
	Request[API_V_ENTRY] = pEntry->ToVariant(Opts);
	if (LockdownToken)
		Request[API_V_TOKEN] = LockdownToken;
	auto Res = theCore->Driver()->Call(API_SET_HASH, Request);
	//if (Res.IsSuccess()) // will be done by the notification event
	//	UpdateRule(pRule, pRule->GetGuid());
	if(Res.IsError())
		return ERR(Res.GetStatus());
	return OK;
}

STATUS CHashDB::RemoveEntry(const CHashPtr& pEntry, uint64 LockdownToken)
{
	if (!pEntry || pEntry->GetHash().GetSize() == 0)
		return ERR(STATUS_INVALID_PARAMETER);

	StVariant Request;
	Request[API_V_HASH] = pEntry->GetHash();
	if (LockdownToken)
		Request[API_V_TOKEN] = LockdownToken;
	auto Res = theCore->Driver()->Call(API_DEL_HASH, Request);
	//if (Res.IsSuccess()) // will be done by the notification event
	//	UpdateRule(nullptr, Guid);
	if(Res.IsError())
		return ERR(Res.GetStatus());
	return OK;
}

void CHashDB::OnHashChanged(const std::wstring& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID)
{
	ASSERT(Type == EConfigGroup::eHashDB);

	if(Event == EConfigEvent::eAllChanged)
		LoadEntries();
	else
	{
		CBuffer HashValue = FromHex(FW::StringW(nullptr, Guid.c_str(), Guid.length()));

		CHashPtr pEntry;
		if (Event != EConfigEvent::eRemoved) {
			StVariant Request;
			Request[API_V_HASH] = HashValue;
			auto Res = theCore->Driver()->Call(API_GET_HASH, Request);
			if (Res.IsSuccess()) {
				const StVariant Entry = Res.GetValue().At(API_V_ENTRY);
				pEntry = std::make_shared<CHash>();
				pEntry->FromVariant(Entry);
			}
		}

		CHashPtr pEntry2 = pEntry;
		if (Event == EConfigEvent::eRemoved)
			pEntry2 = GetEntry(HashValue);
		if (pEntry2)
			theCore->VolumeManager()->UpdateHashEntry(pEntry2, Event, PID);

		std::unique_lock Lock(m_Mutex);

		CHashPtr pOldEntry;
		if (HashValue.GetSize() != 0) {
			auto F = m_Entries.find(HashValue);
			if (F != m_Entries.end())
				pOldEntry = F->second;
		}

		if (pEntry && pOldEntry)
			pOldEntry->Update(pEntry);
		else {
			if (pOldEntry) m_Entries.erase(pOldEntry->GetHash());
			if (pEntry) m_Entries.insert(std::make_pair(pEntry->GetHash(), pEntry));
		}
	}

	StVariant vEvent;
	vEvent[API_V_GUID] = Guid;
	//vEvent[API_V_NAME] = ;
	vEvent[API_V_EVENT_TYPE] = (uint32)Event;

	theCore->BroadcastMessage(SVC_API_EVENT_HASHDB_CHANGED, vEvent);
}