#include "pch.h"
#include "AppInstallation.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../../Library/API/PrivacyAPI.h"

CAppInstallation::CAppInstallation(const TInstallId& Id)
{
	m_ID.Set(EProgramType::eAppInstallation, Id);

	m_RegKey = Id;
}

void CAppInstallation::SetInstallPath(const std::wstring Path)	
{ 
	std::unique_lock lock(m_Mutex); 
	m_Path = Path; 
	UpdatePattern();
}

void CAppInstallation::UpdatePattern()
{
	if(m_Path.length() > 0 && m_Path.at(m_Path.length() - 1) == L'\\')
		m_Path.erase(m_Path.length() - 1);
	SetPathPattern(m_Path + L"\\*");
}

NTSTATUS CAppInstallation::FromVariant(const CVariant& Data)
{
	NTSTATUS status = CProgramPattern::FromVariant(Data);

	std::unique_lock lock(m_Mutex); 
	UpdatePattern();

	return status;
}