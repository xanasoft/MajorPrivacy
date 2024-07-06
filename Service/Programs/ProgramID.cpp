#include "pch.h"
#include "ProgramID.h"
#include "ServiceCore.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/PrivacyAPI.h"

CProgramID::CProgramID()
{
}

CProgramID::~CProgramID()
{
}

bool CProgramID::operator ==(const CProgramID& ID) const
{
	if (m_Type != ID.m_Type)
		return false;
	if (m_FilePath != ID.m_FilePath)
		return false;
	if (m_RegKey != ID.m_RegKey)
		return false;
	if (m_ServiceTag != ID.m_ServiceTag)
		return false;
	if (m_AppContainerSid != ID.m_AppContainerSid)
		return false;
	return true;
}

void CProgramID::Set(EProgramType Type, const std::wstring& Value)
{
	m_Type = Type;
	switch (m_Type)
	{
	case EProgramType::eProgramFile:		m_FilePath = NormalizeFilePath(Value); break;
	case EProgramType::eFilePattern:		m_FilePath = NormalizeFilePath(Value); break;
	case EProgramType::eAppInstallation:	m_RegKey = MkLower(Value); break;
	case EProgramType::eWindowsService:		m_ServiceTag = MkLower(Value); break;
	case EProgramType::eAppPackage:			m_AppContainerSid = MkLower(Value); break;
	}
}

void CProgramID::Set(const std::wstring& FilePath, const std::wstring& ServiceTag, const std::wstring& AppContainerSid)
{
	if (!AppContainerSid.empty())
		m_Type = EProgramType::eAppPackage;
	else if (!ServiceTag.empty())
		m_Type = EProgramType::eWindowsService;
	else if (!FilePath.empty())
		m_Type = EProgramType::eProgramFile;
	else
		m_Type = EProgramType::eAllPrograms;

	m_FilePath = NormalizeFilePath(FilePath);
	m_ServiceTag = MkLower(ServiceTag);
	m_AppContainerSid = MkLower(AppContainerSid);
}

void CProgramID::SetPath(const std::wstring& FilePath)
{
	if(FilePath.empty() || FilePath == L"*")
		m_Type = EProgramType::eAllPrograms;
	else {
		m_Type = FilePath.find(L"*") != -1 ? EProgramType::eFilePattern : EProgramType::eProgramFile;
		m_FilePath = NormalizeFilePath(FilePath);
	}
}
