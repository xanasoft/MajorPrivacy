#pragma once

#include "AppPackage.h"
//#include "ProgramHash.h"
#include "ProgramPattern.h"
#include "AppInstallation.h"
#include "WindowsService.h"
//#include "../Process.h"
#include "ProgramID.h"

class CProgramManager: public QObject
{
	Q_OBJECT
public:
	CProgramManager(QObject* parent = nullptr);

	void Update();

	CProgramSetPtr			GetRoot() const { return m_Root; }
	CProgramItemPtr			GetProgramByID(const CProgramID& ID);

	CProgramFilePtr			GetProgramFile(const QString& Path);
	CWindowsServicePtr		GetService(const QString& Id);
	CAppPackagePtr			GetAppPackage(const QString& Id);

protected:

	//void UpdateGroup(const CProgramGroupPtr& Group, const class XVariant& Root);

	void					AddProgram(const CProgramItemPtr& pItem);
	void					RemoveProgram(const CProgramItemPtr& pItem);

	CProgramSetPtr							m_Root;
	CProgramSetPtr							m_pAll;
	QMap<quint64, CProgramItemPtr>			m_Items;

	QMap<QString, CProgramFilePtr>			m_PathMap;
	QMap<QString, CWindowsServicePtr>		m_ServiceMap;
	QMap<QString, CAppPackagePtr>			m_PackageMap;
};

