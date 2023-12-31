#include "pch.h"
#include "TweakManager.h"
#include "..\Library\Helpers\GPOUtil.h"

CTweakPtr InitKnownTweaks(std::map<std::wstring, CTweakPtr>* pList);

CTweakManager::CTweakManager()
{
}

CTweakManager::~CTweakManager()
{
}

STATUS CTweakManager::Init()
{
	// no lock needed during init

	m_Root = InitKnownTweaks(&m_List);

	return OK;
}

STATUS CTweakManager::ApplyTweak(const std::wstring& Name, ETweakMode Mode)
{
	std::unique_lock Lock(m_Mutex);

	auto F = m_List.find(Name);
	if (F == m_List.end())
		return ERR(STATUS_NOT_FOUND);
	return F->second->Apply(Mode);
}

STATUS CTweakManager::UndoTweak(const std::wstring& Name, ETweakMode Mode)
{
	std::unique_lock Lock(m_Mutex);

	auto F = m_List.find(Name);
	if (F == m_List.end())
		return ERR(STATUS_NOT_FOUND);
	return F->second->Undo(Mode);
}

CGPO* CTweakManager::GetGPOLocked()
{
	if (!m_pGPO) {
		m_pGPO = new CGPO();
		m_pGPO->Open(true);
	}
	return m_pGPO;
}