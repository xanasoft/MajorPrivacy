#include "pch.h"
#include "AppPackage.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../Library/Common/XVariant.h"
#include "../Service/ServiceAPI.h"

CAppPackage::CAppPackage(QObject* parent)
	: CProgramList(parent)
{
}

QIcon CAppPackage::DefaultIcon() const
{
	return QIcon(":/Icons/Software.png");
}

QString CAppPackage::GetNameEx() const
{
	if(m_Name.isEmpty())
		return m_AppContainerName;
	if(m_AppContainerName.isEmpty())
		return m_Name;
	return QString("%1 (%2)").arg(m_Name).arg(m_AppContainerName); // todo advanced view only
}

void CAppPackage::ReadValue(const SVarName& Name, const XVariant& Data)
{
		 if (VAR_TEST_NAME(Name, SVC_API_PROG_APP_SID)) {
		m_AppContainerSid = Data.AsQStr();
		//SSid AppContainerSid = SSid(m_AppContainerSid.toStdString());
		//m_AppContainerName = QString::fromStdWString(::GetAppContainerNameBySid(AppContainerSid));
	}
	else if (VAR_TEST_NAME(Name, SVC_API_PROG_APP_NAME))	m_AppContainerName = Data.AsQStr();
	else if (VAR_TEST_NAME(Name, SVC_API_PROG_APP_PATH))	m_InstallPath = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, SVC_API_PROG_APP_PATH))	m_InstallPath = Data.AsQStr();

	else CProgramList::ReadValue(Name, Data);
}
