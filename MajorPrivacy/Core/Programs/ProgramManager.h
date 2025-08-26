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

	void Clear();

	CProgramSetPtr			GetRoot() const { QReadLocker Lock(&m_Mutex); return m_Root; }
	CProgramSetPtr			GetAll() const { QReadLocker Lock(&m_Mutex); return m_pAll; }
	QHash<QString, CProgramGroupPtr>	GetGroups() const { QReadLocker Lock(&m_Mutex); return m_Groups; }
	CProgramItemPtr			GetProgramByID(const CProgramID& ID);
	CProgramItemPtr			GetProgramByUID(quint64 UID, bool bCanUpdate = false);
	CProgramLibraryPtr		GetLibraryByUID(quint64 UID, bool bCanUpdate = false);

	CProgramFilePtr			GetProgramFile(const QString& Path);
	CWindowsServicePtr		GetService(const QString& Id);
	CAppPackagePtr			GetAppPackage(const QString& Id);
	CProgramPatternPtr		GetPattern(const QString& Pattern);

	QSet<CProgramFilePtr>	GetPrograms() const { QReadLocker Lock(&m_Mutex); return ListToSet(m_PathMap.values()); }
	QSet<CWindowsServicePtr> GetServices() const { QReadLocker Lock(&m_Mutex); return ListToSet(m_ServiceMap.values()); }

	RESULT(quint64) SetProgram(const CProgramItemPtr& pItem);
	STATUS AddProgramTo(const CProgramItemPtr& pItem, const CProgramItemPtr& pParent);
	STATUS RemoveProgramFrom(const CProgramItemPtr& pItem, const CProgramItemPtr& pParent = CProgramItemPtr(), bool bDelRules = false, bool bKeepOne = false);

	bool UpdateAllProgramRules();
	bool UpdateProgramRule(const QFlexGuid& Guid);
	void RemoveProgramRule(const QFlexGuid& Guid);

	QSet<QFlexGuid> GetProgramRuleIDs() const;
	QList<CProgramRulePtr> GetProgramRules() const { QReadLocker Lock(&m_Mutex); return m_ProgramRules.values(); }
	//QList<CProgramRulePtr> GetProgramRulesFor(const QList<const class CProgramItem*>& Nodes);
	QList<CProgramRulePtr> GetProgramRules(const QSet<QFlexGuid> &ProgramRuleIDs);

	STATUS SetProgramRule(const CProgramRulePtr& pRule);
	RESULT(CProgramRulePtr) GetProgramRule(const QFlexGuid& Guid);
	STATUS DelProgramRule(const CProgramRulePtr& pRule);

	//QSet<CProgramItemPtr> GetItems() const { QReadLocker Lock(&m_Mutex); return ListToSet(m_Items.values()); }
	QHash<quint64, CProgramItemPtr> GetItems() { QReadLocker Lock(&m_Mutex); return m_Items; }

signals:
	void					ProgramsAdded();

protected:

	static CProgramItemPtr	MakeProgram(EProgramType Type);
	CProgramItemPtr			UpdateProgramByID(const CProgramID& ID);
	CProgramItemPtr			UpdateProgramByUID(quint64 UID);
	CProgramItemPtr			UpdateProgramImpl(const QtVariant& Data);
	STATUS					UpdateLibs();

	void					AddProgramRule(const CProgramRulePtr& pRule);
	void					RemoveProgramRule(const CProgramRulePtr& pRule);

	//void UpdateGroup(const CProgramGroupPtr& Group, const class QtVariant& Root);

	void					AddProgramUnsafe(const CProgramItemPtr& pItem);
	void					RemoveProgramUnsafe(const CProgramItemPtr& pItem);

	mutable QReadWriteLock m_Mutex;

	CProgramSetPtr							m_Root;
	CProgramSetPtr							m_pAll;
	QHash<quint64, CProgramItemPtr>			m_Items;

	QHash<QString, CProgramGroupPtr>		m_Groups;
	QHash<QString, CProgramFilePtr>			m_PathMap;
	QHash<QString, CProgramPatternPtr>		m_PatternMap;
	QHash<QString, CWindowsServicePtr>		m_ServiceMap;
	QHash<QString, CAppPackagePtr>			m_PackageMap;

	uint64									m_LibrariesCacheToken = 0;
	QHash<quint64, CProgramLibraryPtr>		m_Libraries;

	QHash<QFlexGuid, CProgramRulePtr>		m_ProgramRules;
};

