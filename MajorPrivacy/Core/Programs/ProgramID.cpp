#include "pch.h"
#include "ProgramId.h"
#include "../Service/ServiceAPI.h"


CProgramID::CProgramID()
{

}

CProgramID::~CProgramID()
{

}

XVariant CProgramID::ToVariant() const
{
	XVariant ID;
	ID.BeginIMap();
	switch (m_Type)
	{
	case EType::eFile:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_FILE);
		ID.Write(SVC_API_ID_FILE_PATH, m_FilePath.toStdWString());
		break;
	case EType::eFilePattern:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_PATTERN);
		ID.Write(SVC_API_ID_FILE_PATH, m_FilePath.toStdWString());
		break;
	case EType::eInstall:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_INSTALL);
		ID.Write(SVC_API_ID_REG_KEY, m_RegKey.toStdWString());
		ID.Write(SVC_API_ID_FILE_PATH, m_FilePath.toStdWString());
		break;
	case EType::eService:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_SERVICE);
		ID.Write(SVC_API_ID_FILE_PATH, m_FilePath.toStdWString());
		ID.Write(SVC_API_ID_SVC_TAG, m_ServiceTag.toStdWString());
		break;
	case EType::eApp:
		ID.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_PACKAGE);
		ID.Write(SVC_API_ID_APP_SID, m_AppContainerSid.toStdWString());
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

bool CProgramID::FromVariant(const XVariant& ID)
{
	std::string Type = ID.Find(SVC_API_ID_TYPE);
	if (Type == SVC_API_ID_TYPE_FILE) {
		m_Type = EType::eFile;
		m_FilePath = ID.Find(SVC_API_ID_FILE_PATH).AsQStr();
	}
	else if (Type == SVC_API_ID_TYPE_PATTERN) {
		m_Type = EType::eFilePattern;
		m_FilePath = ID.Find(SVC_API_ID_FILE_PATH).AsQStr();
	}
	else if (Type == SVC_API_ID_TYPE_INSTALL) {
		m_Type = EType::eInstall;
		m_RegKey = ID.Find(SVC_API_ID_REG_KEY).AsQStr();
		m_FilePath = ID.Find(SVC_API_ID_FILE_PATH).AsQStr();
	}
	else if (Type == SVC_API_ID_TYPE_SERVICE) {
		m_Type = EType::eService;
		m_FilePath = ID.Find(SVC_API_ID_FILE_PATH).AsQStr();
		m_ServiceTag = ID.Find(SVC_API_ID_SVC_TAG).AsQStr();
	}
	else if (Type == SVC_API_ID_TYPE_PACKAGE) {
		m_Type = EType::eApp;
		m_AppContainerSid = ID.Find(SVC_API_ID_APP_SID).AsQStr();
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

QString CProgramID::ToString() const
{
	QString Str;
	switch (m_Type)
	{
	case EType::eFile:
		Str = "file:///" + m_FilePath;
		break;
	case EType::eFilePattern:
		Str = "file:///" + m_FilePath;
		break;
	case EType::eInstall:
		Str = "key:///" + m_RegKey;
		break;
	case EType::eService:
		Str = "svc:///" + m_ServiceTag;
		break;
	case EType::eApp:
		Str = "app:///" + m_AppContainerSid;
		break;
	case EType::eAll:
		Str = "special:all";
		break;
	default:
		ASSERT(0);
		break;
	}
	return Str;
}

bool operator < (const CProgramID& l, const CProgramID& r)
{
	return l.m_Type < r.m_Type ||
		   l.m_RegKey < r.m_RegKey ||
		   l.m_ServiceTag < r.m_ServiceTag || 
		   l.m_AppContainerSid < r.m_AppContainerSid ||
		   l.m_FilePath < r.m_FilePath;
	return false;
}

bool operator == (const CProgramID& l, const CProgramID& r)
{
    return l.m_Type == r.m_Type &&
           l.m_FilePath == r.m_FilePath &&
           l.m_RegKey == r.m_RegKey &&
           l.m_ServiceTag == r.m_ServiceTag &&
           l.m_AppContainerSid == r.m_AppContainerSid;
}