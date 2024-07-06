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

	inline EProgramType GetType() const							{ return m_Type; }

	inline const QString& GetFilePath() const					{ return m_FilePath; }
	inline const QString& GetServiceTag() const					{ return m_ServiceTag; }
	inline const QString& GetAppContainerSid() const			{ return m_AppContainerSid; }
	inline const QString& GetRegKey() const						{ return m_RegKey; }

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
	QString m_RegKey;
	QString m_ServiceTag;
	QString m_AppContainerSid;
};

bool operator < (const CProgramID& l, const CProgramID& r);
bool operator == (const CProgramID& l, const CProgramID& r);
__forceinline bool operator != (const CProgramID& l, const CProgramID& r) { return !operator == (l, r); }