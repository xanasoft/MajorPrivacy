#pragma once
#include "ProgramGroup.h"

class CAppInstallation: public CProgramList
{
public:
	CAppInstallation(const TInstallId& Id);

	virtual void SetInstallPath(const std::wstring& Path)	{ std::unique_lock lock(m_Mutex); m_InstallPath = Path; }
	virtual std::wstring GetInstallPath() const				{ std::shared_lock lock(m_Mutex);return m_InstallPath; }

	virtual int									GetSpecificity() const { return (int)m_InstallPath.length(); }

protected:
	friend class CProgramManager;

	virtual void WriteVariant(CVariant& Data) const;

	std::wstring m_RegKey;

	std::wstring m_InstallPath;
};

typedef std::shared_ptr<CAppInstallation> CAppInstallationPtr;
typedef std::weak_ptr<CAppInstallation> CAppInstallationRef;