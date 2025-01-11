#include "pch.h"
#include "AppInstallation.h"
#include "../../Library/Helpers/SID.h"
#include "../../Library/Helpers/AppUtil.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/Common/Strings.h"

CAppInstallation::CAppInstallation(const std::wstring& RegKey)
{
	m_ID = CProgramID(MkLower(RegKey), EProgramType::eAppInstallation);
	m_RegKey = RegKey;
}

void CAppInstallation::SetPath(const std::wstring Path)	
{ 
	std::unique_lock lock(m_Mutex); 
	m_Path = Path; 
}

bool CAppInstallation::MatchFileName(const std::wstring& FileName) const
{
	std::unique_lock lock(m_Mutex);

	//
	// Note: we may have multiple identical patterns we must not add them to each other
	// this happens for example with installations when moluple once point to the same directory
	// Example: Cyberpunk 2077 installs itself, redmod, and phantom liberty to the same folder
	// 
	// therfor if(FileName.length() <= m_Path.length) return false;
	//

	if (m_Path.empty() || FileName.length() <= m_Path.length())
		return false;
	return _wcsnicmp(FileName.c_str(), m_Path.c_str(), m_Path.length()) == 0;
}