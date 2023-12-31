#pragma once
#include "ProgramGroup.h"

class CAppInstallation: public CProgramList
{
	Q_OBJECT
public:
	CAppInstallation(QObject* parent = nullptr);
	
	virtual QString GetRegKey() const			{ return m_RegKey; }
	virtual QString GetPath() const				{ return m_InstallPath; }

	virtual QString GetInfo() const				{ return m_RegKey; } // todo

protected:

	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	QString m_RegKey;
	QString m_InstallPath;
};

typedef QSharedPointer<CAppInstallation> CAppInstallationPtr;