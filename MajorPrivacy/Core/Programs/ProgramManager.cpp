#include "pch.h"
#include "ProgramManager.h"
#include "../PrivacyCore.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Common/XVariant.h"

CProgramManager::CProgramManager(QObject* parent)
	: QObject(parent)
{
	m_Root = CProgramSetPtr(new CProgramList());
	m_Root->m_UID = 1;
	m_Items.insert(m_Root->GetUID(), m_Root);
}

//void CProgramManager::UpdateGroup(const CProgramGroupPtr& Group, const class XVariant& Root)
//{
//	QMap<quint64, CProgramItemPtr> OldMap = Group->m_Nodes;
//
//	const XVariant& Items = Root[SVC_API_PROG_ITEMS];
//	for (int i = 0; i < Items.Count(); i++)
//	{
//		const XVariant& Item = Items[i];
//
//		quint64 uId = Item[SVC_API_PROG_UID];
//		CProgramItemPtr pItem = OldMap.take(uId);
//		if (!pItem) 
//		{
//			QString Type = Item[SVC_API_ID_TYPE].AsQStr();
//			if (Type == SVC_API_ID_TYPE_FILE)			pItem = CProgramItemPtr(new CProgramFile());
//			else if (Type == SVC_API_ID_TYPE_PATTERN)	pItem = CProgramItemPtr(new CProgramPattern());
//			else if (Type == SVC_API_ID_TYPE_INSTALL)	pItem = CProgramItemPtr(new CAppInstallation());
//			else if (Type == SVC_API_ID_TYPE_SERVICE)	pItem = CProgramItemPtr(new CWindowsService());
//			else if (Type == SVC_API_ID_TYPE_PACKAGE)	pItem = CProgramItemPtr(new CAppPackage());
//			else if (Type == SVC_API_ID_TYPE_All)		pItem = CProgramItemPtr(new CAllPrograms());
//			else 
//				continue; // unknown type
//			pItem->FromVariant(Item);
//			Group->m_Nodes.insert(uId, pItem);
//		}
//		else // update
//			pItem->FromVariant(Item, true);
//
//		if (CProgramGroupPtr SubGroup = pItem.objectCast<CProgramGroup>())
//			UpdateGroup(SubGroup, Item);
//	}
//
//	foreach(quint64 uId, OldMap.keys())
//		Group->m_Nodes.remove(uId);
//}

STATUS CProgramManager::Update()
{
	auto Ret = theCore->GetPrograms();
	if (Ret.IsError())
		return Ret.GetStatus();

	int iAdded = 0;

	//UpdateGroup(m_Root, Ret.GetValue());

	const XVariant& Items = Ret.GetValue();

	QMap<quint64, QSet<quint64>> Tree;

	QMap<quint64, CProgramItemPtr> OldMap = m_Items;

	Items.ReadRawList([&](const CVariant& vData) {
		const XVariant& Item = *(XVariant*)&vData;

		quint64 uId = Item.Find(API_V_PROG_UID);

		CProgramItemPtr pItem = OldMap.take(uId);
		if (!pItem) 
		{
			CProgramID ID;
			ID.FromVariant(Item.Find(API_V_PROG_ID));

			EProgramType Type = (EProgramType)Item.Find(API_V_PROG_TYPE).To<uint32>();
			if (Type == EProgramType::eProgramFile)				pItem = CProgramItemPtr(new CProgramFile());
			else if (Type == EProgramType::eFilePattern)		pItem = CProgramItemPtr(new CProgramPattern());
			else if (Type == EProgramType::eAppInstallation)	pItem = CProgramItemPtr(new CAppInstallation());
			else if (Type == EProgramType::eWindowsService)		pItem = CProgramItemPtr(new CWindowsService());
			else if (Type == EProgramType::eAppPackage)			pItem = CProgramItemPtr(new CAppPackage());
			else if (Type == EProgramType::eProgramGroup)		pItem = CProgramItemPtr(new CProgramGroup());
			else if (Type == EProgramType::eAllPrograms)		pItem = m_pAll = CProgramSetPtr(new CAllPrograms());
			else if (Type == EProgramType::eProgramRoot)		pItem = CProgramItemPtr(new CProgramRoot()); // used only for root item
			else 
				return; // unknown type
			pItem->SetUID(uId);
			pItem->SetID(ID);
			m_Items.insert(uId, pItem);
			if (ID.GetType() != EProgramType::eUnknown) {
				AddProgram(pItem);
				iAdded++;
			}
		}

		pItem->FromVariant(Item);

		if (pItem->inherits("CProgramSet")) 
		{
			auto Items = Item.Find(API_V_PROG_ITEMS);
			QSet<quint64> ItemIDs;
			Items.ReadRawList([&](const CVariant& vData) {
				//Q_ASSERT(!ItemIDs.contains(vData.Get(SVC_API_PROG_UID))); // todo: xxx
				ItemIDs.insert(vData.Get(API_V_PROG_UID));
			});
			if(!ItemIDs.isEmpty()) Tree[uId] = ItemIDs;
		}
	});

	// sync the tree structure once all items are known to exist
	for (auto I = Tree.begin(); I != Tree.end(); ++I) 
	{
		CProgramSetPtr pItem = m_Items[I.key()].objectCast<CProgramSet>();
		if (!pItem) continue; // should not happen but in case
		QMap<quint64, CProgramItemPtr> OldItems = pItem->m_Nodes;

		foreach(quint64 uId, I.value())
		{
			if (OldItems.take(uId))
				continue;
			CProgramItemPtr pNode = m_Items.value(uId);
			if (pNode) {
				pItem->m_Nodes.insert(uId, pNode);
				pNode->m_Groups.append(pItem);
			}
		}

		foreach(quint64 uId, OldItems.keys()) {
			CProgramItemPtr pNode = pItem->m_Nodes.take(uId);
			if (pNode) pNode->m_Groups.removeOne(pItem);
		}
	}

	foreach(const CProgramItemPtr & pItem, OldMap) {
		for (auto Group : pItem->m_Groups) {
			auto pGroup = Group.lock().objectCast<CProgramSet>();
			if(pGroup) pGroup->m_Nodes.remove(pItem->GetUID());
		}
		m_Items.remove(pItem->GetUID());
		RemoveProgram(pItem);
	}

	m_Root->CountStats();

	if(iAdded > 0)
		emit ProgramsAdded();

	return UpdateLibs();
}

STATUS  CProgramManager::UpdateLibs()
{
	auto Ret = theCore->GetLibraries(m_LibrariesCacheToken);
	if (Ret.IsError())
		return Ret.GetStatus();

	const XVariant& Libraries = Ret.GetValue().Get(API_V_LIBRARIES);

	QMap<quint64, CProgramLibraryPtr> OldLibraries = m_Libraries;

	Libraries.ReadRawList([&](const CVariant& vData) {
		const XVariant& Library = *(XVariant*)&vData;

		quint64 UID = Library.Find(API_V_PROG_UID);

		CProgramLibraryPtr pLibrary = OldLibraries.take(UID);
		if (!pLibrary) {
			pLibrary = CProgramLibraryPtr(new CProgramLibrary());
			m_Libraries.insert(UID, pLibrary);
		} 

		pLibrary->FromVariant(Library);
	});

	//m_LibrariesCacheToken = Ret.GetValue().Get(SVC_API_CACHE_TOKEN).To<uint64>(0);

	//if (m_LibrariesCacheToken)
	//{
	//	const XVariant& OldLibraries = Ret.GetValue().Get(SVC_API_OLD_LIBRARIES);

	//	OldLibraries.ReadRawList([&](const CVariant& vData) {
	//		quint64 UID = vData;
	//		m_Libraries.remove(UID);
	//	});
	//}
	//else
	{
		foreach(quint64 UID, OldLibraries.keys())
			m_Libraries.remove(UID);
	}

	return OK;
}

void CProgramManager::Clear()
{
	m_Root->m_Nodes.clear();
	m_Items.clear();
	m_Items.insert(m_Root->GetUID(), m_Root);
	
	m_PathMap.clear();
	m_PatternMap.clear();
	m_ServiceMap.clear();
	m_PackageMap.clear();
	m_Groups.clear();

	//m_ProgramRules.clear();

	m_LibrariesCacheToken = 0;
	m_Libraries.clear();
}

void CProgramManager::AddProgram(const CProgramItemPtr& pItem)
{
	const CProgramID& ID = pItem->GetID();
	switch (ID.GetType())
	{
	case EProgramType::eProgramFile:			m_PathMap.insert(ID.GetFilePath(), pItem.objectCast<CProgramFile>()); break;
	case EProgramType::eFilePattern:			m_PatternMap.insert(ID.GetFilePath(), pItem.objectCast<CProgramPattern>()); break;
	case EProgramType::eWindowsService:			m_ServiceMap.insert(ID.GetServiceTag(), pItem.objectCast<CWindowsService>()); break;
	case EProgramType::eAppPackage:				m_PackageMap.insert(ID.GetAppContainerSid(), pItem.objectCast<CAppPackage>()); break;
	case EProgramType::eAppInstallation:		break;
	case EProgramType::eProgramGroup:			m_Groups.insert(ID.GetAppContainerSid(), pItem.objectCast<CProgramGroup>()); break;
	case EProgramType::eAllPrograms:			m_pAll = pItem.objectCast<CProgramSet>(); break;
	default: Q_ASSERT(0);
	}
}

void CProgramManager::RemoveProgram(const CProgramItemPtr& pItem)
{
	const CProgramID& ID = pItem->GetID();
	switch (ID.GetType())
	{
	case EProgramType::eProgramFile:			m_PathMap.remove(ID.GetFilePath()); break;
	case EProgramType::eFilePattern:			m_PatternMap.remove(ID.GetFilePath()); break;
	case EProgramType::eWindowsService:			m_ServiceMap.remove(ID.GetServiceTag()); break;
	case EProgramType::eAppPackage:				m_PackageMap.remove(ID.GetAppContainerSid()); break;
	case EProgramType::eProgramGroup:			m_Groups.remove(ID.GetAppContainerSid()); break;
	//default: Q_ASSERT(0); // ignore
	}
}

CProgramItemPtr CProgramManager::GetProgramByID(const CProgramID& ID)
{
	switch (ID.GetType())
	{
	case EProgramType::eAllPrograms:			return m_pAll;
	case EProgramType::eProgramFile:			return GetProgramFile(ID.GetFilePath());
	case EProgramType::eFilePattern:			return GetPattern(ID.GetFilePath());
	case EProgramType::eWindowsService:			return GetService(ID.GetServiceTag());
	case EProgramType::eAppPackage:				return GetAppPackage(ID.GetAppContainerSid());
	default: Q_ASSERT(0); return NULL;
	}
}

CProgramItemPtr CProgramManager::GetProgramByUID(quint64 UID, bool bCanUpdate)
{
	CProgramItemPtr pProgram = m_Items.value(UID);
	if(!pProgram){
		if (bCanUpdate) {
			Update();
			pProgram = m_Items.value(UID);
		}
#ifdef _DEBUG
		else
			qDebug() << "Program not found by UID:" << UID;
#endif
	}
	return pProgram;
}

CProgramLibraryPtr CProgramManager::GetLibraryByUID(quint64 UID, bool bCanUpdate)
{
	CProgramLibraryPtr pLibrary = m_Libraries.value(UID);
	if (!pLibrary) {
		if (bCanUpdate) {
			UpdateLibs();
			pLibrary = m_Libraries.value(UID);
		}
#ifdef _DEBUG
		else
			qDebug() << "Library not found by UID:" << UID;
#endif
	}
	return pLibrary;
}

CProgramFilePtr CProgramManager::GetProgramFile(const QString& Path)
{
	CProgramFilePtr pItem = m_PathMap.value(Path);
	if (!pItem) {
		Update();
		pItem = m_PathMap.value(Path);
	}
	return pItem;
}
	
CWindowsServicePtr CProgramManager::GetService(const QString& Id)
{
	CWindowsServicePtr pItem = m_ServiceMap.value(Id);
	if (!pItem) {
		Update();
		pItem = m_ServiceMap.value(Id);
	}
	return pItem;
}

CAppPackagePtr CProgramManager::GetAppPackage(const QString& Id)
{
	CAppPackagePtr pItem = m_PackageMap.value(Id);
	if (!pItem) {
		Update();
		pItem = m_PackageMap.value(Id);
	}
	return pItem;
}

CProgramPatternPtr CProgramManager::GetPattern(const QString& Pattern)
{
	CProgramPatternPtr pItem = m_PatternMap.value(Pattern);
	if (!pItem) {
		Update();
		pItem = m_PatternMap.value(Pattern);
	}
	return pItem;
}

RESULT(quint64) CProgramManager::SetProgram(const CProgramItemPtr& pItem)
{
	return theCore->SetProgram(pItem);
}

STATUS CProgramManager::AddProgramTo(const CProgramItemPtr& pItem, const CProgramItemPtr& pParent)
{
	return theCore->AddProgramTo(pItem->GetUID(), pParent->GetUID());
}

STATUS CProgramManager::RemoveProgramFrom(const CProgramItemPtr& pItem, const CProgramItemPtr& pParent, bool bDelRules)
{
	return theCore->RemoveProgramFrom(pItem->GetUID(), pParent ? pParent->GetUID() : 0, bDelRules);
}

/////////////////////////////////////////////////////////////////////////////
// Rules

bool CProgramManager::UpdateAllProgramRules()
{
	auto Ret = theCore->GetAllProgramRules();
	if (Ret.IsError())
		return false;

	XVariant& Rules = Ret.GetValue();

	QMap<QString, CProgramRulePtr> OldRules = m_ProgramRules;

	for (int i = 0; i < Rules.Count(); i++)
	{
		const XVariant& Rule = Rules[i];

		QString Guid = Rule[API_V_RULE_GUID].AsQStr();

		QString ProgramPath = QString::fromStdWString(NormalizeFilePath(Rule[API_V_FILE_PATH].AsStr()));

		CProgramID ID;
		ID.SetPath(ProgramPath);

		CProgramRulePtr pRule = OldRules.value(Guid);
		if (pRule) {
			if(ID == pRule->GetProgramID())
				OldRules.remove(Guid);
			else // on ID change compeltely re add the rule as if it would be new
			{
				RemoveProgramRule(pRule);
				pRule.clear();
			}
		}

		bool bAdd = false;
		if (!pRule) {
			pRule = CProgramRulePtr(new CProgramRule(ID));
			bAdd = true;
		} 

		pRule->FromVariant(Rule);

		if(bAdd)
			AddProgramRule(pRule);
	}

	foreach(const QString& Guid, OldRules.keys())
		RemoveProgramRule(OldRules.take(Guid));

	return true;
}

bool CProgramManager::UpdateProgramRule(const QString& RuleId)
{
	auto Ret = theCore->GetProgramRule(RuleId);
	if (Ret.IsError())
		return false;

	XVariant& Rule = Ret.GetValue();

	QString RuleID = Rule[API_V_RULE_GUID].AsQStr();

	QString ProgramPath = QString::fromStdWString(NormalizeFilePath(Rule[API_V_FILE_PATH].AsStr()));

	CProgramID ID;
	ID.SetPath(ProgramPath);

	CProgramRulePtr pRule = m_ProgramRules.value(RuleID);
	if (pRule) {
		if(ID != pRule->GetProgramID()) // on ID change compeltely re add the rule as if it would be new
		{
			RemoveProgramRule(pRule);
			pRule.clear();
		}
	}

	bool bAdd = false;
	if (!pRule) {
		pRule = CProgramRulePtr(new CProgramRule(ID));
		bAdd = true;
	} 

	pRule->FromVariant(Rule);

	if(bAdd)
		AddProgramRule(pRule);

	return true;
}

void CProgramManager::RemoveProgramRule(const QString& RuleId)
{
	CProgramRulePtr pRule = m_ProgramRules.value(RuleId);
	if (pRule)
		RemoveProgramRule(pRule);
}

QSet<QString> CProgramManager::GetProgramRuleIDs() const
{
	return ListToSet(m_ProgramRules.keys()); 
}

QList<CProgramRulePtr> CProgramManager::GetProgramRules(const QSet<QString>& ProgramRuleIDs)
{
	QList<CProgramRulePtr> List;
	foreach(const QString & Guid, ProgramRuleIDs) {
		CProgramRulePtr pProgramRulePtr = m_ProgramRules.value(Guid);
		if (pProgramRulePtr) List.append(pProgramRulePtr);
	}
	return List;
}

STATUS CProgramManager::SetProgramRule(const CProgramRulePtr& pRule)
{
	return theCore->SetProgramRule(pRule->ToVariant(SVarWriteOpt()));
}

RESULT(CProgramRulePtr) CProgramManager::GetProgramRule(QString Guid)
{
	auto Ret = theCore->GetProgramRule(Guid);
	if (Ret.IsError())
		return Ret;

	XVariant& Rule = Ret.GetValue();

	QString ProgramPath = QString::fromStdWString(NormalizeFilePath(Rule[API_V_FILE_PATH].AsStr()));

	CProgramID ID;
	ID.SetPath(ProgramPath);

	CProgramRulePtr pRule = CProgramRulePtr(new CProgramRule(ID));
	pRule->FromVariant(Rule);

	RETURN(pRule);
}

STATUS CProgramManager::DelProgramRule(const CProgramRulePtr& pRule)
{
	STATUS Status = theCore->DelProgramRule(pRule->GetGuid());
	if(Status)
		RemoveProgramRule(pRule);
	return Status;
}

void CProgramManager::AddProgramRule(const CProgramRulePtr& pRule)
{
	m_ProgramRules.insert(pRule->GetGuid(), pRule);
}

void CProgramManager::RemoveProgramRule(const CProgramRulePtr& pRule)
{
	m_ProgramRules.remove(pRule->GetGuid());
}