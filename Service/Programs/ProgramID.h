#pragma once
#include "../Library/Common/Variant.h"

//typedef std::wstring	TAppHash;		// Application Binary Hash SHA256
typedef std::wstring		TPatternId;	// Pattern as string
typedef std::wstring		TAppId;		// Appcontainer SID as string
typedef std::wstring		TFilePath;	// normalized Application Binary file apth
typedef std::wstring		TServiceId;	// Service name as string
typedef std::wstring		TInstallId;	// registry key under: HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\

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

	void Set(EType Type, const std::wstring& Value);
	void Set(const std::wstring& FilePath, const std::wstring& ServiceTag, const std::wstring& AppContainerSid);

	inline EType GetType() const								{ return m_Type; }

	inline const std::wstring& GetFilePath() const				{ return m_FilePath; }
	inline const std::wstring& GetServiceTag() const			{ return m_ServiceTag; }
	inline const std::wstring& GetAppContainerSid() const		{ return m_AppContainerSid; }

	CVariant ToVariant() const;
	bool FromVariant(const CVariant& FwRule);

protected:

	EType m_Type = EType::eUnknown;

	std::wstring m_FilePath; // or pattern
	std::wstring m_RegKey;
	std::wstring m_ServiceTag;
	std::wstring m_AppContainerSid;

};

//typedef std::shared_ptr<CProgramID> CProgramIDPtr;