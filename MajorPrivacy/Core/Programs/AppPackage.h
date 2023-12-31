#pragma once
#include "ProgramGroup.h"

class CAppPackage: public CProgramList
{
	Q_OBJECT
public:
	CAppPackage(QObject* parent = nullptr);

	virtual QIcon DefaultIcon() const;

	virtual QString GetNameEx() const;
	virtual QString GetAppSid() const			{ return m_AppContainerSid; }
	virtual QString GetContainerName() const	{ return m_AppContainerName; }
	virtual QString GetPath() const				{ return m_InstallPath; }

	virtual QString GetInfo() const				{ return m_AppContainerSid; } // todo

protected:
	
	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	QString m_AppContainerSid;
	QString m_AppContainerName;
	QString m_InstallPath;
};

typedef QSharedPointer<CAppPackage> CAppPackagePtr;