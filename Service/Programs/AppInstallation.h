#pragma once
#include "ProgramPattern.h"

typedef std::wstring		TInstallId;	// registry key under: HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\

class CAppInstallation: public CProgramPattern
{
	TRACK_OBJECT(CAppInstallation)
public:
	CAppInstallation(const TInstallId& Id);

	virtual EProgramType GetType() const					{ return EProgramType::eAppInstallation; }

	virtual void SetInstallPath(const std::wstring Path);
	virtual std::wstring GetInstallPath() const				{ std::unique_lock lock(m_Mutex);return m_Path; }

	virtual int									GetSpecificity() const { return (int)m_Path.length(); }

	virtual NTSTATUS FromVariant(const CVariant& Data);

protected:
	friend class CProgramManager;

	void UpdatePattern();

	void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const CVariant& Data) override;
	void ReadMValue(const SVarName& Name, const CVariant& Data) override;

	std::wstring m_RegKey;
	std::wstring m_Path;
};

typedef std::shared_ptr<CAppInstallation> CAppInstallationPtr;
typedef std::weak_ptr<CAppInstallation> CAppInstallationRef;