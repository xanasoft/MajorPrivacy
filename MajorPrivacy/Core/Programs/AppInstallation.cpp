#include "pch.h"
#include "AppInstallation.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../Library/Common/XVariant.h"
#include "../Service/ServiceAPI.h"

CAppInstallation::CAppInstallation(QObject* parent)
	: CProgramList(parent)
{
}

void CAppInstallation::ReadValue(const SVarName& Name, const XVariant& Data)
{
		 if (VAR_TEST_NAME(Name, SVC_API_PROG_REG_KEY))		m_RegKey = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, SVC_API_PROG_APP_PATH))	m_InstallPath = Data.AsQStr();

	else CProgramList::ReadValue(Name, Data);
}
