#include "pch.h"
#include "TweakManager.h"
#include "../PrivacyCore.h"
#include "../../Library/API/PrivacyAPI.h"

CTweakManager::CTweakManager(QObject* parent)
	: QObject(parent)
{
	m_pRoot = CTweakPtr(new CTweakList());
}

CTweakPtr CTweakManager::GetRoot() 
{
	Update();

	return m_pRoot; 
}

void CTweakManager::Update()
{
	auto Ret = theCore->GetTweaks();
	if (Ret.IsError())
		return;

	XVariant& Tweaks = Ret.GetValue();

	m_pRoot->FromVariant(Tweaks);
}

STATUS CTweakManager::ApplyTweak(const CTweakPtr& pTweak)
{
	return theCore->ApplyTweak(pTweak->GetName());
}

STATUS CTweakManager::UndoTweak(const CTweakPtr& pTweak)
{
	return theCore->UndoTweak(pTweak->GetName());
}