#include "pch.h"
#include "AppPackage.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../../Library/API/PrivacyAPI.h"

CAppPackage::CAppPackage(const TAppId& Id, const std::wstring& Name)
{
	m_ID.Set(EProgramType::eAppPackage, Id);

	SSid AppContainerSid = SSid(std::string(Id.begin(), Id.end()));
	m_AppContainerSid = Id;
	m_AppContainerName = Name.empty() ? ::GetAppContainerNameBySid(AppContainerSid) : Name;
}