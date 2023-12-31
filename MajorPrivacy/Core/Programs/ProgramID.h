#pragma once
#include "../Library/Common/XVariant.h"

class CProgramID
{
public:
	CProgramID();
	~CProgramID();

	enum EType
	{
		eUnknown = 0,
		eFile,
		eFilePattern,
		eInstall,
		eService,
		eApp,
		eAll
	};

	inline EType GetType() const							{ return m_Type; }

	inline const QString& GetFilePath() const				{ return m_FilePath; }
	inline const QString& GetServiceTag() const				{ return m_ServiceTag; }
	inline const QString& GetAppContainerSid() const		{ return m_AppContainerSid; }

	QString ToString() const;

	XVariant ToVariant() const;
	bool FromVariant(const XVariant& FwRule);

protected:
	friend bool operator < (const CProgramID& l, const CProgramID& r);
	friend bool operator == (const CProgramID& l, const CProgramID& r);

	EType m_Type = EType::eUnknown;

	QString m_FilePath; // or pattern
	QString m_RegKey;
	QString m_ServiceTag;
	QString m_AppContainerSid;

};

bool operator < (const CProgramID& l, const CProgramID& r);
bool operator == (const CProgramID& l, const CProgramID& r);
__forceinline bool operator != (const CProgramID& l, const CProgramID& r) { return !operator == (l, r); }