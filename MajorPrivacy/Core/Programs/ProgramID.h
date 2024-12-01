#pragma once
#include "../Library/Common/XVariant.h"
#include "../Library/API/PrivacyDefs.h"

class CProgramID
{
public:
	CProgramID();
	CProgramID(EProgramType Type);
	~CProgramID();

	void SetPath(const QString& FilePath);
	void SetRegKey(const QString& RegKey)						{ Q_ASSERT(m_Type == EProgramType::eAppInstallation); m_AuxValue = RegKey; }
	void SetServiceTag(const QString& ServiceTag)				{ m_ServiceTag = ServiceTag; }
	void SetAppContainerSid(const QString& AppContainerSid)		{ Q_ASSERT(m_Type == EProgramType::eAppPackage); m_AuxValue = AppContainerSid; }
	void SetGuid(const QString& Guid)							{ Q_ASSERT(m_Type == EProgramType::eProgramGroup); m_AuxValue = Guid; }

	EProgramType GetType() const								{ return m_Type; }

	QString GetFilePath() const									{ return m_FilePath; }
	QString GetServiceTag() const								{ return m_ServiceTag; }
	QString GetAppContainerSid() const							{ if(m_Type == EProgramType::eAppPackage) return m_AuxValue; return QString(); }
	QString GetGuid() const										{ if(m_Type == EProgramType::eProgramGroup) return m_AuxValue; return QString();  }
	QString GetRegKey() const									{ if(m_Type == EProgramType::eAppInstallation) return m_AuxValue; return QString(); }

	QString ToString() const;

	static EProgramType ReadType(const XVariant& Data, SVarWriteOpt::EFormat& Format);
	static std::string TypeToStr(EProgramType Type);

	XVariant ToVariant(const SVarWriteOpt& Opts) const;
	bool FromVariant(const XVariant& ID);

protected:

	friend bool operator < (const CProgramID& l, const CProgramID& r);
	friend bool operator == (const CProgramID& l, const CProgramID& r);

	EProgramType m_Type = EProgramType::eUnknown;

	QString m_FilePath; // or pattern
	QString m_ServiceTag;
	QString m_AuxValue;
};

bool operator < (const CProgramID& l, const CProgramID& r);
bool operator == (const CProgramID& l, const CProgramID& r);
__forceinline bool operator != (const CProgramID& l, const CProgramID& r) { return !operator == (l, r); }