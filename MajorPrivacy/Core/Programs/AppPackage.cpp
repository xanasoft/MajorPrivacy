#include "pch.h"
#include "AppPackage.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyAPI.h"

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