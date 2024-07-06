#pragma once
#include "../Library/Common/Variant.h"
#include "../Library/API/PrivacyDefs.h"

class CProgramID
{
public:
	CProgramID();
	~CProgramID();

	bool operator ==(const CProgramID& ID) const;

	void Set(EProgramType Type, const std::wstring& Value = L"");
	void Set(const std::wstring& FilePath, const std::wstring& ServiceTag, const std::wstring& AppContainerSid);
	void SetPath(const std::wstring& FilePath);

	inline EProgramType GetType() const							{ return m_Type; }

	inline const std::wstring& GetFilePath() const				{ return m_FilePath; }
	inline const std::wstring& GetServiceTag() const			{ return m_ServiceTag; }
	inline const std::wstring& GetAppContainerSid() const		{ return m_AppContainerSid; }
	inline const std::wstring& GetRegKey() const				{ return m_RegKey; }

	static EProgramType ReadType(const CVariant& Data, SVarWriteOpt::EFormat& Format);
	static std::string TypeToStr(EProgramType Type);

	CVariant ToVariant(const SVarWriteOpt& Opts) const;
	bool FromVariant(const CVariant& ID);

protected:

	EProgramType m_Type = EProgramType::eUnknown;

	std::wstring m_FilePath; // or pattern
	std::wstring m_RegKey;
	std::wstring m_ServiceTag;
	std::wstring m_AppContainerSid;
};

//typedef std::shared_ptr<CProgramID> CProgramIDPtr;