#include "pch.h"
#include "AppInstallation.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "ServiceAPI.h"

CAppInstallation::CAppInstallation(const TInstallId& Id)
{
	m_ID.Set(CProgramID::eInstall, Id);

	m_RegKey = Id;
}


void CAppInstallation::WriteVariant(CVariant& Data) const
{
	Data.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_INSTALL);

	CProgramList::WriteVariant(Data);

	Data.Write(SVC_API_PROG_REG_KEY, m_RegKey);
	Data.Write(SVC_API_PROG_APP_PATH, m_InstallPath);
}