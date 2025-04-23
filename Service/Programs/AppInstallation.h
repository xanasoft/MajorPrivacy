#pragma once
#include "ProgramPattern.h"

class CAppInstallation: public CProgramListEx
{
	TRACK_OBJECT(CAppInstallation)
public:
	CAppInstallation(const std::wstring& RegKey); // registry key under: HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\

	virtual EProgramType GetType() const					{ return EProgramType::eAppInstallation; }

	virtual void SetPath(const std::wstring Path);
	virtual std::wstring GetPath() const					{ std::unique_lock lock(m_Mutex);return m_Path; }

	virtual bool MatchFileName(const std::wstring& FileName) const;

protected:
	friend class CProgramManager;

	void WriteIVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const StVariant& Data) override;
	void ReadMValue(const SVarName& Name, const StVariant& Data) override;

	std::wstring m_RegKey;
	std::wstring m_Path;
};

typedef std::shared_ptr<CAppInstallation> CAppInstallationPtr;
typedef std::weak_ptr<CAppInstallation> CAppInstallationRef;