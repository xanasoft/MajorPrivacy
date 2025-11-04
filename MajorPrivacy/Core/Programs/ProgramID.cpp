#include "pch.h"
#include "ProgramId.h"
#include "../Library/API/PrivacyAPI.h"

CProgramID::CProgramID()
{
}

CProgramID::CProgramID(EProgramType Type)
{
	m_Type = Type;
}

CProgramID::~CProgramID()
{
}

CProgramID CProgramID::FromPath(const QString& FilePath)
{
	CProgramID ID;
	ID.m_Type = EProgramType::eProgramFile;
	ID.m_FilePath = FilePath;
	return ID;
}

CProgramID CProgramID::FromService(const QString& FilePath, const QString& ServiceTag)
{
	CProgramID ID;
	ID.m_Type = EProgramType::eWindowsService;
	ID.m_FilePath = FilePath;
	ID.m_ServiceTag = ServiceTag;
	return ID;
}

void CProgramID::SetPath(const QString& FilePath)
{
	if(FilePath.isEmpty() || FilePath == "*")
		m_Type = EProgramType::eAllPrograms;
	else {
		m_Type = FilePath.contains("*") ? EProgramType::eFilePattern : EProgramType::eProgramFile;
		m_FilePath = FilePath;
	}
}

QString CProgramID::ToString() const
{
	QString Str;
	switch (m_Type)
	{
	case EProgramType::eProgramFile:
		Str = "file:///" + m_FilePath;
		break;
	case EProgramType::eFilePattern:
		Str = "file:///" + m_FilePath;
		break;
	case EProgramType::eAppInstallation:
		Str = "key:///" + m_AuxValue;
		break;
	case EProgramType::eWindowsService:
		Str = "svc://" + m_ServiceTag;
		break;
	case EProgramType::eAppPackage:
		Str = "app://" + m_AuxValue;
		break;
	case EProgramType::eProgramGroup:
		Str = "group://" + m_AuxValue;
		break;
	case EProgramType::eAllPrograms:
		Str = "all";
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
		   l.m_ServiceTag < r.m_ServiceTag || 
		   l.m_AuxValue < r.m_AuxValue ||
		   l.m_FilePath < r.m_FilePath;
	return false;
}

bool operator == (const CProgramID& l, const CProgramID& r)
{
    return l.m_Type == r.m_Type &&
           l.m_FilePath == r.m_FilePath &&
           l.m_ServiceTag == r.m_ServiceTag &&
           l.m_AuxValue == r.m_AuxValue;
}
