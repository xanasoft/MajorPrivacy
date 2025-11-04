#pragma once
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyDefs.h"

class CProgramID
{
public:
	CProgramID();
	CProgramID(EProgramType Type);
	~CProgramID();

	static CProgramID FromPath(const QString& FilePath);
	static CProgramID FromService(const QString& FilePath, const QString& ServiceTag);

	void SetPath(const QString& FilePath);
	void SetRegKey(const QString& RegKey)						{ m_Type = EProgramType::eAppInstallation; m_AuxValue = RegKey; }
	void SetServiceTag(const QString& ServiceTag)				{ m_Type = EProgramType::eWindowsService; m_ServiceTag = ServiceTag; }
	void SetAppContainerSid(const QString& AppContainerSid)		{ m_Type = EProgramType::eAppPackage; m_AuxValue = AppContainerSid; }
	void SetGuid(const QString& Guid)							{ m_Type = EProgramType::eProgramGroup; m_AuxValue = Guid; }

	EProgramType GetType() const								{ return m_Type; }

	QString GetFilePath() const									{ return m_FilePath; }
	QString GetServiceTag() const								{ return m_ServiceTag; }
	QString GetAppContainerSid() const							{ if(m_Type == EProgramType::eAppPackage) return m_AuxValue; return QString(); }
	QString GetGuid() const										{ if(m_Type == EProgramType::eProgramGroup) return m_AuxValue; return QString();  }
	QString GetRegKey() const									{ if(m_Type == EProgramType::eAppInstallation) return m_AuxValue; return QString(); }

	QString ToString() const;

	static EProgramType ReadType(const QtVariant& Data, SVarWriteOpt::EFormat& Format);
	static QString TypeToStr(EProgramType Type);

	QtVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	bool FromVariant(const QtVariant& ID);

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