#pragma once
#include "ProgramGroup.h"

typedef std::wstring		TAppId;		// Appcontainer SID as string

class CAppPackage: public CProgramList
{
	TRACK_OBJECT(CAppPackage)
public:
	CAppPackage(const TAppId& Id, const std::wstring& Name = L"");

	virtual EProgramType GetType() const					{ return EProgramType::eAppPackage; }

	virtual TAppId GetAppSid() const						{ std::unique_lock lock(m_Mutex);return m_AppContainerSid; }
	virtual std::wstring GetContainerName() const			{ std::unique_lock lock(m_Mutex);return m_AppContainerName; }
	virtual void SetInstallPath(const std::wstring& Path)	{ std::unique_lock lock(m_Mutex); m_Path = Path; }
	virtual std::wstring GetInstallPath() const				{ std::unique_lock lock(m_Mutex);return m_Path; }

protected:
	friend class CProgramManager;

	void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const CVariant& Data) override;
	void ReadMValue(const SVarName& Name, const CVariant& Data) override;

	TAppId m_AppContainerSid;
	std::wstring m_AppContainerName;
	std::wstring m_Path;
};

typedef std::shared_ptr<CAppPackage> CAppPackagePtr;
typedef std::weak_ptr<CAppPackage> CAppPackageRef;