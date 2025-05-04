#include "pch.h"
#include "TweakManager.h"
#include "..\Library\Helpers\GPOUtil.h"
#include "..\ServiceCore.h"
#include "..\Library\API\PrivacyAPI.h"
#include "..\Library\API\PrivacyAPIs.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/ConfigIni.h"
#include "../Library/Helpers/Scoped.h"

std::shared_ptr<CTweakList> InitKnownTweaks(std::list<CTweakPtr>* pList);

#define API_TWEAK_LIST_FILE_NAME L"TweakList.dat"
#define API_TWEAK_LIST_FILE_VERSION 1

CTweakManager::CTweakManager()
{
}

CTweakManager::~CTweakManager()
{
}

STATUS CTweakManager::Init()
{
	std::unique_lock Lock(m_Mutex);

	m_pRoot = std::make_shared<CTweakGroup>(L"", L"");

	// prepare default tweak list
	std::list<CTweakPtr> List;
#ifndef _DEBUG
	CScoped pTweaksIni = new CConfigIni(GetApplicationDirectory() + L"\\Tweaks.ini");
	auto Tweaks = pTweaksIni->ListSections();
	if (!Tweaks.empty())
	{
		for (auto& Tweak : Tweaks)
		{
			auto pTweak = CTweak::CreateFromIni(Tweak, pTweaksIni);
			if (!pTweak) continue;
			std::wstring Parent = pTweaksIni->GetValue(Tweak, "Parent", L"");
			pTweak->SetParentId(Parent);
			List.push_back(pTweak);
		}
	}
	else
#endif
		InitKnownTweaks(&List);

	DbgPrint("Tweaks loaded: %d\n", List.size());

	// Load Tweaks from file
	Load();

#if 1
	std::shared_ptr<CTweakList> pRoot = m_pRoot;
	std::map<std::wstring, CTweakPtr> Map = m_Map;

	m_pRoot = std::make_shared<CTweakGroup>(L"", L"");
	m_Map.clear();
#endif

	// add tweaks to the list
	for (auto& pTweak : List) 
	{
		std::wstring Id = pTweak->GetId();
#if 1
		auto F = Map.find(Id);
		if (F != Map.end())
		{
			std::shared_ptr<CTweak> pTweak1 = std::dynamic_pointer_cast<CTweak>(pTweak);
			std::shared_ptr<CTweak> pTweak2 = std::dynamic_pointer_cast<CTweak>(F->second);
			if(pTweak1 && pTweak2)
				pTweak1->SetSet(pTweak2->IsSet());
		}
#else
		auto F = m_Map.find(Id);
		if (F != m_Map.end())
			continue; // this one is already known or duplicate
#endif
		m_Map[Id] = pTweak;

		std::wstring Parent = pTweak->GetParentId();
		if (!Parent.empty()) {
			std::shared_ptr<CTweakList> pParent;
			auto F = m_Map.find(Parent);
			if (F != m_Map.end())
				pParent = std::dynamic_pointer_cast<CTweakList>(F->second);
			ASSERT(pParent);
			if (pParent) {
				pParent->AddTweak(pTweak);
				continue;
			}
		}
		m_pRoot->AddTweak(pTweak);
	}

	return OK;
}

STATUS CTweakManager::Load()
{
	CBuffer Buffer;
	if (!ReadFile(theCore->GetDataFolder() + L"\\" API_TWEAK_LIST_FILE_NAME, Buffer)) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, API_TWEAK_LIST_FILE_NAME L" not found");
		return ERR(STATUS_NOT_FOUND);
	}

	StVariant Data;
	//try {
	auto ret = Data.FromPacket(&Buffer, true);
	//} catch (const CException&) {
	//	return ERR(STATUS_UNSUCCESSFUL);
	//}
	if (ret != StVariant::eErrNone) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Failed to parse " API_TWEAK_LIST_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	if (Data[API_S_VERSION].To<uint32>() != API_TWEAK_LIST_FILE_VERSION) {
		theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_SVC_INIT_FAILED, L"Encountered unsupported " API_TWEAK_LIST_FILE_NAME);
		return ERR(STATUS_UNSUCCESSFUL);
	}

	StVariant Tweaks = Data[API_S_TWEAKS];

	for (uint32 i = 0; i < Tweaks.Count(); i++)
	{
		StVariant Tweak = Tweaks[i];

		SVarWriteOpt::EFormat Format;
		ETweakType eType = CAbstractTweak::ReadType(Tweak, Format);

		StVariantReader Reader(Tweak);
		std::wstring Id = (Format == SVarWriteOpt::eMap) ? Reader.Find(API_S_TWEAK_ID) : Reader.Find(API_V_TWEAK_ID);
		std::wstring Parent = (Format == SVarWriteOpt::eMap) ? Reader.Find(API_S_PARENT) : Reader.Find(API_V_PARENT);

		CTweakPtr pTweak;
		switch (eType)
		{
		case ETweakType::eGroup: pTweak = std::make_shared<CTweakGroup>(Id); break;
		case ETweakType::eSet:	pTweak = std::make_shared<CTweakSet>(Id); break;
		case ETweakType::eReg:	pTweak = std::make_shared<CRegTweak>(Id); break;
		case ETweakType::eGpo:	pTweak = std::make_shared<CGpoTweak>(Id); break;
		case ETweakType::eSvc:	pTweak = std::make_shared<CSvcTweak>(Id); break;
		case ETweakType::eTask: pTweak = std::make_shared<CTaskTweak>(Id); break;
		case ETweakType::eRes:	pTweak = std::make_shared<CResTweak>(Id); break;
		case ETweakType::eExec: pTweak = std::make_shared<CExecTweak>(Id); break;
		case ETweakType::eFw:	pTweak = std::make_shared<CFwTweak>(Id); break;
		default:
			continue;
		}
		pTweak->FromVariant(Tweak);

		m_Map[Id] = pTweak;

		if (!Parent.empty()) {
			std::shared_ptr<CTweakList> pParent;
			auto F = m_Map.find(Parent);
			if (F != m_Map.end())
				pParent = std::dynamic_pointer_cast<CTweakList>(F->second);
			ASSERT(pParent);
			if (pParent) {
				pParent->AddTweak(pTweak);
				continue;
			}
		}

		m_pRoot->AddTweak(pTweak);
	}

	return OK;
}

STATUS CTweakManager::Store()
{
	SVarWriteOpt Opts;
	Opts.Format = SVarWriteOpt::eMap;
	Opts.Flags = SVarWriteOpt::eSaveToFile;

	StVariant Data;
	Data[API_S_VERSION] = API_TWEAK_LIST_FILE_VERSION;
	Data[API_S_TWEAKS] = GetTweaks(Opts, 0);

	CBuffer Buffer;
	Data.ToPacket(&Buffer);
	WriteFile(theCore->GetDataFolder() + L"\\" API_TWEAK_LIST_FILE_NAME, Buffer);

	m_bTweaksDirty = false;
	return OK;
}

void CTweakManager::CheckTweaks()
{
	// todo: check and notify on undone tweaks
}

StVariant CTweakManager::GetTweaks(const SVarWriteOpt& Opts, uint32 CallerPID) const
{
	std::unique_lock Lock(m_Mutex);

	//
	// Store Tweaks in a flat list but in order such that the parent tweaks are first
	//

	StVariantWriter Tweaks;
	Tweaks.BeginList();

	std::list<CTweakPtr> List;
	for (auto I : m_pRoot->GetList())
		List.push_back(I);

	while (!List.empty()) 
	{
		auto pTweak = List.front();
		List.pop_front();

		std::unique_lock Lock(pTweak->m_Mutex);

		StVariantWriter Tweak;
		if (Opts.Format == SVarWriteOpt::eIndex) 
		{
			Tweak.BeginIndex();
			pTweak->WriteIVariant(Tweak, Opts);

			if (Opts.Flags & SVarWriteOpt::eSaveAll) 
			{
				ETweakStatus Status = pTweak->GetStatus(CallerPID);
				Tweak.Write(API_V_TWEAK_STATUS, (uint32)Status);
			}
		} 
		else 
		{  
			Tweak.BeginMap();
			pTweak->WriteMVariant(Tweak, Opts);
		}
		Tweaks.WriteVariant(Tweak.Finish());

		auto pList = std::dynamic_pointer_cast<CTweakList>(pTweak);
		if (pList) {
			for (auto I : pList->GetList())
				List.push_back(I);
		}
	}

	return Tweaks.Finish();
}

STATUS CTweakManager::ApplyTweak(const std::wstring& Id, uint32 CallerPID, ETweakMode Mode)
{
	std::unique_lock Lock(m_Mutex);

	auto F = m_Map.find(Id);
	if (F == m_Map.end())
		return ERR(STATUS_NOT_FOUND);
	m_bTweaksDirty = true;
	return F->second->Apply(CallerPID, Mode);
}

STATUS CTweakManager::UndoTweak(const std::wstring& Id, uint32 CallerPID, ETweakMode Mode)
{
	std::unique_lock Lock(m_Mutex);

	auto F = m_Map.find(Id);
	if (F == m_Map.end())
		return ERR(STATUS_NOT_FOUND);
	m_bTweaksDirty = true;
	return F->second->Undo(CallerPID, Mode);
}

STATUS CTweakManager::ApproveTweak(const std::wstring& Id, uint32 CallerPID)
{
	std::unique_lock Lock(m_Mutex);

	auto F = m_Map.find(Id);
	if (F == m_Map.end())
		return ERR(STATUS_NOT_FOUND);

	auto pTweak = std::dynamic_pointer_cast<CTweak>(F->second);
	if (!pTweak)
		return ERR(STATUS_OBJECT_TYPE_MISMATCH);

	auto Status = pTweak->GetStatus(CallerPID);
	bool bApplied = Status == ETweakStatus::eSet || Status == ETweakStatus::eApplied;
	if (bApplied != pTweak->IsSet()) {
		pTweak->SetSet(bApplied);
		m_bTweaksDirty = true;
	}
	return OK;
}

CGPO* CTweakManager::GetLockedGPO()
{
	m_GPOMutex.lock();
	if (!m_pGPO) {
		m_pGPO = new CGPO();
		HRESULT hr = m_pGPO->OpenGPO(true);
		if (FAILED(hr)) {
			delete m_pGPO;
			m_pGPO = NULL;
			m_GPOMutex.unlock();
			theCore->Log()->LogEventLine(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_GPO_FAILED, L"Failed to open GPO: 0x%08X", hr);
		}
	}
	return m_pGPO;
}

void CTweakManager::ReleaseGPO(class CGPO* pGPO)
{
	ASSERT(pGPO == m_pGPO);
	m_GPOMutex.unlock();
}