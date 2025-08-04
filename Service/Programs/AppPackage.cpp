#include "pch.h"
#include "AppPackage.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Library/Common/Strings.h"

CAppPackage::CAppPackage(const std::wstring& AppContainerSid, const std::wstring& AppPackageName, const std::wstring& Name)
{
	m_ID = CProgramID(MkLower(AppContainerSid), EProgramType::eAppPackage);

	m_AppContainerSid = AppContainerSid;
	m_AppPackageName = AppPackageName;
	if (AppPackageName.empty()) {
		SSid Sid = SSid(std::string(AppContainerSid.begin(), AppContainerSid.end()));
		m_AppPackageName = ::GetAppContainerNameBySid(Sid);
	} else
		m_AppPackageName = AppPackageName;
	m_AppContainerName = Name;
}

bool CAppPackage::MatchFileName(const std::wstring& FileName) const
{
	std::unique_lock lock(m_Mutex);

	if (m_Path.empty() || FileName.length() <= m_Path.length())
		return false;
	return _wcsnicmp(FileName.c_str(), m_Path.c_str(), m_Path.length()) == 0;
}