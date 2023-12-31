#pragma once
#include "ProgramGroup.h"

class CAppPackage: public CProgramList
{
public:
	CAppPackage(const TAppId& Id, const std::wstring& Name = L"");

	virtual TAppId GetAppSid() const						{ std::shared_lock lock(m_Mutex);return m_AppContainerSid; }
	virtual std::wstring GetContainerName() const			{ std::shared_lock lock(m_Mutex);return m_AppContainerName; }
	virtual void SetInstallPath(const std::wstring& Path)	{ std::unique_lock lock(m_Mutex); m_InstallPath = Path; }
	virtual std::wstring GetInstallPath() const				{ std::shared_lock lock(m_Mutex);return m_InstallPath; }

protected:
	friend class CProgramManager;

	virtual void WriteVariant(CVariant& Data) const;

	TAppId m_AppContainerSid;
	std::wstring m_AppContainerName;

	std::wstring m_InstallPath;
};

typedef std::shared_ptr<CAppPackage> CAppPackagePtr;
typedef std::weak_ptr<CAppPackage> CAppPackageRef;