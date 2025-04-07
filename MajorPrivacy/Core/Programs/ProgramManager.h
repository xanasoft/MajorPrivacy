#pragma once

#include "../Library/Status.h"
#include "AppPackage.h"
//#include "ProgramHash.h"
#include "ProgramPattern.h"
#include "AppInstallation.h"
#include "WindowsService.h"
//#include "../Process.h"
#include "ProgramID.h"
#include "ProgramRule.h"
#include "ProgramLibrary.h"
#include "../MiscHelpers/Common/Common.h"


class CProgramManager: public QObject
{
	Q_OBJECT
public:
	CProgramManager(QObject* parent = nullptr);

	STATUS Update();
	STATUS UpdateLibs();

	void Clear();

	CProgramSetPtr			GetRoot() const { return m_Root; }
	CProgramSetPtr			GetAll() const { return m_pAll; }
	QMap<QString, CProgramGroupPtr>	GetGroups() const { return m_Groups; }
	CProgramItemPtr			GetProgramByID(const CProgramID& ID);
	CProgramItemPtr			GetProgramByUID(quint64 UID, bool bCanUpdate = false);
	CProgramLibraryPtr		GetLibraryByUID(quint64 UID, bool bCanUpdate = false);

	CProgramFilePtr			GetProgramFile(const QString& Path);
	CWindowsServicePtr		GetService(const QString& Id);
	CAppPackagePtr			GetAppPackage(const QString& Id);
	CProgramPatternPtr		GetPattern(const QString& Pattern);

	QSet<CProgramFilePtr>	GetPrograms() const { return ListToSet(m_PathMap.values()); }
	QSet<CWindowsServicePtr> GetServices() const { return ListToSet(m_ServiceMap.values()); }

	RESULT(quint64) SetProgram(const CProgramItemPtr& pItem);
	STATUS AddProgramTo(const CProgramItemPtr& pItem, const CProgramItemPtr& pParent);
	STATUS RemoveProgramFrom(const CProgramItemPtr& pItem, const CProgramItemPtr& pParent = CProgramItemPtr(), bool bDelRules = false);

	bool UpdateAllProgramRules();
	bool UpdateProgramRule(const QFlexGuid& Guid);
	void RemoveProgramRule(const QFlexGuid& Guid);

	QSet<QFlexGuid> GetProgramRuleIDs() const;
	QList<CProgramRulePtr> GetProgramRules() const { return m_ProgramRules.values(); }
	//QList<CProgramRulePtr> GetProgramRulesFor(const QList<const class CProgramItem*>& Nodes);
	QList<CProgramRulePtr> GetProgramRules(const QSet<QFlexGuid> &ProgramRuleIDs);

	STATUS SetProgramRule(const CProgramRulePtr& pRule);
	RESULT(CProgramRulePtr) GetProgramRule(const QFlexGuid& Guid);
	STATUS DelProgramRule(const CProgramRulePtr& pRule);

	QSet<CProgramItemPtr> GetItems() const { return ListToSet(m_Items.values()); }

signals:
	void					ProgramsAdded();

protected:

	void					AddProgramRule(const CProgramRulePtr& pRule);
	void					RemoveProgramRule(const CProgramRulePtr& pRule);

	//void UpdateGroup(const CProgramGroupPtr& Group, const class QtVariant& Root);

	void					AddProgram(const CProgramItemPtr& pItem);
	void					RemoveProgram(const CProgramItemPtr& pItem);

	CProgramSetPtr							m_Root;
	CProgramSetPtr							m_pAll;
	QMap<quint64, CProgramItemPtr>			m_Items;

	QMap<QString, CProgramGroupPtr>			m_Groups;
	QMap<QString, CProgramFilePtr>			m_PathMap;
	QMap<QString, CProgramPatternPtr>		m_PatternMap;
	QMap<QString, CWindowsServicePtr>		m_ServiceMap;
	QMap<QString, CAppPackagePtr>			m_PackageMap;

	uint64									m_LibrariesCacheToken = 0;
	QMap<quint64, CProgramLibraryPtr>		m_Libraries;

	QMap<QFlexGuid, CProgramRulePtr>		m_ProgramRules;
};

