#include "pch.h"
#include "AppInstallation.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyAPI.h"

CAppInstallation::CAppInstallation(QObject* parent)
	: CProgramList(parent)
{
}
