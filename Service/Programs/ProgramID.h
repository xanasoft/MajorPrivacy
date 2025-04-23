#pragma once
#include "../Library/Common/StVariant.h"
#include "../Library/API/PrivacyDefs.h"

class CProgramID
{
public:
	CProgramID(EProgramType Type = EProgramType::eUnknown);
	CProgramID(const std::wstring& Value, EProgramType Type = EProgramType::eUnknown);
	~CProgramID();

	bool operator ==(const CProgramID& ID) const;
	
	static CProgramID FromFw(const std::wstring& FilePath, const std::wstring& ServiceTag, const std::wstring& AppContainerSid);

	inline EProgramType GetType() const							{ return m_Type; }

	inline const std::wstring& GetFilePath() const				{ return m_FilePath; }
	inline const std::wstring& GetServiceTag() const			{ return m_ServiceTag; }
	const std::wstring& GetAppContainerSid() const;
	const std::wstring& GetGuid() const;
	const std::wstring& GetRegKey() const;

	static EProgramType ReadType(const StVariant& Data, SVarWriteOpt::EFormat& Format);
	static std::string TypeToStr(EProgramType Type);

	StVariant ToVariant(const SVarWriteOpt& Opts) const;
	bool FromVariant(const StVariant& ID);

protected:

	static const std::wstring m_empty;

	EProgramType m_Type = EProgramType::eUnknown;

	std::wstring m_FilePath; // or pattern
	std::wstring m_ServiceTag;
	std::wstring m_AuxValue;
};

//typedef std::shared_ptr<CProgramID> CProgramIDPtr;