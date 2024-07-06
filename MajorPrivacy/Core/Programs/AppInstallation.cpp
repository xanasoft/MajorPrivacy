#include "pch.h"
#include "AppInstallation.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../Library/Common/XVariant.h"
#include "../Library/API/PrivacyAPI.h"

CAppInstallation::CAppInstallation(QObject* parent)
	: CProgramPattern(parent)
{
}
