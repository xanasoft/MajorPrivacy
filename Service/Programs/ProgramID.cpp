#include "pch.h"
#include "ProgramID.h"
#include "ServiceCore.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/PrivacyAPI.h"

const std::wstring CProgramID::m_empty;

CProgramID::CProgramID(EProgramType Type)
{
	m_Type = Type;
}

CProgramID::CProgramID(const std::wstring& Value, EProgramType Type)
{
	if (Type == EProgramType::eUnknown) {
		if(Value.empty() || Value == L"*")
			m_Type = EProgramType::eAllPrograms;
		else if (Value.find(L'*') != std::wstring::npos)
			m_Type = EProgramType::eFilePattern;
		else
			m_Type = EProgramType::eProgramFile;
	} else
		m_Type = Type;

	switch (m_Type)
	{
	case EProgramType::eProgramFile:		m_FilePath = Value; break;
	case EProgramType::eFilePattern:		m_FilePath = Value; break;
	case EProgramType::eWindowsService:		m_ServiceTag = Value; break;
	case EProgramType::eAppInstallation:
	case EProgramType::eAppPackage:
	case EProgramType::eProgramGroup:		m_AuxValue = Value; break;
	}
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
	if (m_ServiceTag != ID.m_ServiceTag)
		return false;
	if (m_AuxValue != ID.m_AuxValue)
		return false;
	return true;
}

CProgramID CProgramID::FromFw(const std::wstring& FilePath, const std::wstring& ServiceTag, const std::wstring& AppContainerSid)
{
	CProgramID ID;
	if (!AppContainerSid.empty())
		ID.m_Type = EProgramType::eAppPackage;
	else if (!ServiceTag.empty())
		ID.m_Type = EProgramType::eWindowsService;
	else if (!FilePath.empty())
		ID.m_Type = EProgramType::eProgramFile;
	else
		ID.m_Type = EProgramType::eAllPrograms;

	ID.m_FilePath = theCore->NormalizePath(FilePath);
	ID.m_ServiceTag = MkLower(ServiceTag);
	ID.m_AuxValue = MkLower(AppContainerSid);

	return ID;
}

const std::wstring& CProgramID::GetAppContainerSid() const 
{
	if (m_Type == EProgramType::eAppPackage) 
		return m_AuxValue;
	ASSERT(0);
	return m_empty;
}

const std::wstring& CProgramID::GetGuid() const 
{
	if (m_Type == EProgramType::eProgramGroup) 
		return m_AuxValue;
	ASSERT(0);
	return m_empty;
}

const std::wstring& CProgramID::GetRegKey() const 
{
	if (m_Type == EProgramType::eAppInstallation) 
		return m_AuxValue;
	ASSERT(0);
	return m_empty;
}