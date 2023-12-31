#include "pch.h"
#include "AppPackage.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "ServiceAPI.h"

CAppPackage::CAppPackage(const TAppId& Id, const std::wstring& Name)
{
	m_ID.Set(CProgramID::eApp, Id);

	SSid AppContainerSid = SSid(std::string(Id.begin(), Id.end()));
	m_AppContainerSid = Id;
	m_AppContainerName = Name.empty() ? ::GetAppContainerNameBySid(AppContainerSid) : Name;
}

void CAppPackage::WriteVariant(CVariant& Data) const
{
	Data.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_PACKAGE);

	CProgramList::WriteVariant(Data);

	Data.Write(SVC_API_PROG_APP_SID, m_AppContainerSid);
	Data.Write(SVC_API_PROG_APP_NAME, m_AppContainerName);
	Data.Write(SVC_API_PROG_APP_PATH, m_InstallPath);
}