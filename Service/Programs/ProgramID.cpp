#include "pch.h"
#include "ProgramID.h"
#include "ServiceAPI.h"
#include "ServiceCore.h"
#include "../Library/Helpers/NtUtil.h"


CProgramID::CProgramID()
{

}

CProgramID::~CProgramID()
{

}

void CProgramID::Set(EType Type, const std::wstring& Value)
{
	m_Type = Type;
	switch (m_Type)
	{
	case eFile:			m_FilePath = NormalizeFilePath(Value); break;
	case eFilePattern:	m_FilePath = MkLower(Value); break;
	case eInstall:		m_RegKey = MkLower(Value); break;
	case eService:		m_ServiceTag = MkLower(Value); break;
	case eApp:			m_AppContainerSid = MkLower(Value); break;
	}
}

void CProgramID::Set(const std::wstring& FilePath, const std::wstring& ServiceTag, const std::wstring& AppContainerSid)
{
	if (!AppContainerSid.empty())
		m_Type = eApp;
	else if (!ServiceTag.empty())
		m_Type = eService;
	else if (!FilePath.empty())
		m_Type = eFile;
	else
		m_Type = eAll;

	m_FilePath = NormalizeFilePath(FilePath);
	m_ServiceTag = MkLower(ServiceTag);
	m_AppContainerSid = MkLower(AppContainerSid);
}

CVariant CProgramID::ToVariant() const
{
	CVariant ID;
	ID.BeginIMap();
	switch (m_Type)
	{
	case EType::eFile:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_FILE);
		ID.Write(SVC_API_ID_FILE_PATH, m_FilePath);
		break;
	case EType::eFilePattern:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_PATTERN);
		ID.Write(SVC_API_ID_FILE_PATH, m_FilePath);
		break;
	case EType::eInstall:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_INSTALL);
		ID.Write(SVC_API_ID_REG_KEY, m_RegKey);
		ID.Write(SVC_API_ID_FILE_PATH, m_FilePath);
		break;
	case EType::eService:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_SERVICE);
		ID.Write(SVC_API_ID_FILE_PATH, m_FilePath);
		ID.Write(SVC_API_ID_SVC_TAG, m_ServiceTag);
		break;
	case EType::eApp:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_PACKAGE);
		ID.Write(SVC_API_ID_APP_SID, m_AppContainerSid);
		break;
	case EType::eAll:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_All);
		break;
	default:
		ASSERT(0);
		break;
	}
	ID.Finish();
	return ID;
}

bool CProgramID::FromVariant(const CVariant& ID)
{
	std::string Type = ID.Find(SVC_API_ID_TYPE);
	if (Type == SVC_API_ID_TYPE_FILE) {
		m_Type = EType::eFile;
		m_FilePath = NormalizeFilePath(ID.Find(SVC_API_ID_FILE_PATH));
	}
	else if (Type == SVC_API_ID_TYPE_PATTERN) {
		m_Type = EType::eFilePattern;
		m_FilePath = ID.Find(SVC_API_ID_FILE_PATH);
	}
	else if (Type == SVC_API_ID_TYPE_INSTALL) {
		m_Type = EType::eInstall;
		m_RegKey = MkLower(ID.Find(SVC_API_ID_REG_KEY));
		m_FilePath = NormalizeFilePath(ID.Find(SVC_API_ID_FILE_PATH));
	}
	else if (Type == SVC_API_ID_TYPE_SERVICE) {
		m_Type = EType::eService;
		m_FilePath = MkLower(ID.Find(SVC_API_ID_FILE_PATH));
		m_ServiceTag = NormalizeFilePath(ID.Find(SVC_API_ID_SVC_TAG));
	}
	else if (Type == SVC_API_ID_TYPE_PACKAGE) {
		m_Type = EType::eApp;
		m_AppContainerSid = MkLower(ID.Find(SVC_API_ID_APP_SID));
	}
	else if (Type == SVC_API_ID_TYPE_All) {
		m_Type = EType::eAll;
	}
	else {
		ASSERT(0);
		return false;
	}
	return true;
}

