#pragma once
#include "ProgramGroup.h"

class CAppPackage: public CProgramListEx
{
	TRACK_OBJECT(CAppPackage)
public:
	CAppPackage(const std::wstring& AppContainerSid, const std::wstring& AppPackageName, const std::wstring& Name = L"");

	virtual EProgramType GetType() const					{ return EProgramType::eAppPackage; }

	virtual std::wstring GetAppSid() const					{ std::unique_lock lock(m_Mutex);return m_AppContainerSid; }
	virtual std::wstring GetAppPackage() const				{ std::unique_lock lock(m_Mutex);return m_AppPackageName; }
	virtual std::wstring GetContainerName() const			{ std::unique_lock lock(m_Mutex);return m_AppContainerName; }
	virtual void SetPath(const std::wstring& Path)			{ std::unique_lock lock(m_Mutex); m_Path = Path; }
	virtual std::wstring GetPath() const					{ std::unique_lock lock(m_Mutex);return m_Path; }

	virtual bool MatchFileName(const std::wstring& FileName) const;

protected:
	friend class CProgramManager;

	void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const StVariant& Data) override;
	void ReadMValue(const SVarName& Name, const StVariant& Data) override;

	std::wstring m_AppContainerSid;
	std::wstring m_AppPackageName;
	std::wstring m_AppContainerName;
	std::wstring m_Path;
};

typedef std::shared_ptr<CAppPackage> CAppPackagePtr;
typedef std::weak_ptr<CAppPackage> CAppPackageRef;