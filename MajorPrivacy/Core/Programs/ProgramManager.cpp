#include "pch.h"
#include "ProgramManager.h"
#include "../PrivacyCore.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "./Common/QtVariant.h"

CProgramManager::CProgramManager(QObject* parent)
	: QObject(parent), m_Mutex(QReadWriteLock::Recursive)
{
	m_Root = CProgramSetPtr(new CProgramList());
	m_Root->m_UID = 1;
	m_Items.insert(m_Root->GetUID(), m_Root);
}

//void CProgramManager::UpdateGroup(const CProgramGroupPtr& Group, const class QtVariant& Root)
//{
//	QMap<quint64, CProgramItemPtr> OldMap = Group->m_Nodes;
//
//	const QtVariant& Items = Root[SVC_API_PROG_ITEMS];
//	for (int i = 0; i < Items.Count(); i++)
//	{
//		const QtVariant& Item = Items[i];
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

CProgramItemPtr CProgramManager::MakeProgram(EProgramType Type)
{
	CProgramItemPtr pItem;
	if (Type == EProgramType::eProgramFile)				pItem = CProgramItemPtr(new CProgramFile());
	else if (Type == EProgramType::eFilePattern)		pItem = CProgramItemPtr(new CProgramPattern());
	else if (Type == EProgramType::eAppInstallation)	pItem = CProgramItemPtr(new CAppInstallation());
	else if (Type == EProgramType::eWindowsService)		pItem = CProgramItemPtr(new CWindowsService());
	else if (Type == EProgramType::eAppPackage)			pItem = CProgramItemPtr(new CAppPackage());
	else if (Type == EProgramType::eProgramGroup)		pItem = CProgramItemPtr(new CProgramGroup());
	else if (Type == EProgramType::eAllPrograms)		pItem = CProgramSetPtr(new CAllPrograms());
	else if (Type == EProgramType::eProgramRoot)		pItem = CProgramItemPtr(new CProgramRoot()); // used only for root item
	return pItem;
}

STATUS CProgramManager::Update()
{
	//////////////////////////////////////////////////////////
	// WARING This is called from a differnt thread
	//////////////////////////////////////////////////////////

	auto Ret = theCore->GetPrograms();
	if (Ret.IsError())
		return Ret.GetStatus();

	QWriteLocker Lock(&m_Mutex); 

	int iAdded = 0;

	//UpdateGroup(m_Root, Ret.GetValue());

	const QtVariant& Items = Ret.GetValue();

	QHash<quint64, QSet<quint64>> Tree;

	QHash<quint64, CProgramItemPtr> OldMap = m_Items;

	QtVariantReader(Items).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Item = *(QtVariant*)&vData;

		QtVariantReader Reader(Item);

		quint64 uId = Reader.Find(API_V_PROG_UID);

		CProgramItemPtr pItem = OldMap.take(uId);
		if (!pItem) 
		{
			CProgramID ID;
			ID.FromVariant(Reader.Find(API_V_ID));

			EProgramType Type = (EProgramType)Reader.Find(API_V_PROG_TYPE).To<uint32>();
			pItem = MakeProgram(Type);
			if(!pItem)
				return;

			if(Type == EProgramType::eAllPrograms)
				m_pAll = pItem.objectCast<CProgramSet>();
			pItem->SetID(ID);
			pItem->SetUID(uId);
			m_Items.insert(uId, pItem);
			if (ID.GetType() != EProgramType::eUnknown) {
				AddProgramUnsafe(pItem);
				iAdded++;
			}
		}

		pItem->FromVariant(Item);

		if (pItem->inherits("CProgramSet")) 
		{
			auto Items = Reader.Find(API_V_PROG_ITEMS);
			QSet<quint64> ItemIDs;
			QtVariantReader(Items).ReadRawList([&](const FW::CVariant& vData) {
				//Q_ASSERT(!ItemIDs.contains(vData.Get(SVC_API_PROG_UID))); // todo: xxx
				ItemIDs.insert(vData.Get(API_V_PROG_UID));
			});
			Tree[uId] = ItemIDs;
		}
	});

	// sync the tree structure once all items are known to exist
	for (auto I = Tree.begin(); I != Tree.end(); ++I) 
	{
		CProgramSetPtr pItem = m_Items[I.key()].objectCast<CProgramSet>();
		if (!pItem) continue; // should not happen but in case
		QHash<quint64, CProgramItemPtr> OldItems = pItem->m_Nodes;

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
		RemoveProgramUnsafe(pItem);
	}

	m_Root->CountStats();

	Lock.unlock();

	if(iAdded > 0)
		emit ProgramsAdded();

	return UpdateLibs();
}

STATUS CProgramManager::UpdateLibs()
{
	auto Ret = theCore->GetLibraries(m_LibrariesCacheToken);
	if (Ret.IsError())
		return Ret.GetStatus();

	QWriteLocker Lock(&m_Mutex); 

	const QtVariant& Libraries = Ret.GetValue().Get(API_V_LIBRARIES);

	QHash<quint64, CProgramLibraryPtr> OldLibraries = m_Libraries;

	QtVariantReader(Libraries).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Library = *(QtVariant*)&vData;

		QtVariant vUID(Library.Allocator());
		FW::CVariantReader::Find(Library, API_V_PROG_UID, vUID);
		quint64 UID = vUID;

		CProgramLibraryPtr pLibrary = OldLibraries.take(UID);
		bool bUpdate = false; // we dont have live library  data currently hence we dont update it once its enlisted
		if (!pLibrary) {
			pLibrary = CProgramLibraryPtr(new CProgramLibrary());
			m_Libraries.insert(UID, pLibrary);
			bUpdate = true;
		} 

		if(bUpdate)
			pLibrary->FromVariant(Library);
	});

	//m_LibrariesCacheToken = Ret.GetValue().Get(SVC_API_CACHE_TOKEN).To<uint64>(0);

	//if (m_LibrariesCacheToken)
	//{
	//	const QtVariant& OldLibraries = Ret.GetValue().Get(SVC_API_OLD_LIBRARIES);

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
	QWriteLocker Lock(&m_Mutex); 

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

void CProgramManager::AddProgramUnsafe(const CProgramItemPtr& pItem)
{
	const CProgramID& ID = pItem->GetID();
	switch (ID.GetType())
	{
	case EProgramType::eProgramFile:			m_PathMap.insert(ID.GetFilePath(), pItem.objectCast<CProgramFile>()); break;
	case EProgramType::eFilePattern:			m_PatternMap.insert(ID.GetFilePath(), pItem.objectCast<CProgramPattern>()); break;
	case EProgramType::eWindowsService:			m_ServiceMap.insert(ID.GetServiceTag(), pItem.objectCast<CWindowsService>()); break;
	case EProgramType::eAppPackage:				m_PackageMap.insert(ID.GetAppContainerSid(), pItem.objectCast<CAppPackage>()); break;
	case EProgramType::eAppInstallation:		break;
	case EProgramType::eProgramGroup:			m_Groups.insert(ID.GetGuid(), pItem.objectCast<CProgramGroup>()); break;
	case EProgramType::eAllPrograms:			m_pAll = pItem.objectCast<CProgramSet>(); break;
	default: Q_ASSERT(0);
	}
}

void CProgramManager::RemoveProgramUnsafe(const CProgramItemPtr& pItem)
{
	const CProgramID& ID = pItem->GetID();
	switch (ID.GetType())
	{
	case EProgramType::eProgramFile:			m_PathMap.remove(ID.GetFilePath()); break;
	case EProgramType::eFilePattern:			m_PatternMap.remove(ID.GetFilePath()); break;
	case EProgramType::eWindowsService:			m_ServiceMap.remove(ID.GetServiceTag()); break;
	case EProgramType::eAppPackage:				m_PackageMap.remove(ID.GetAppContainerSid()); break;
	case EProgramType::eProgramGroup:			m_Groups.remove(ID.GetGuid()); break;
	//default: Q_ASSERT(0); // ignore
	}
}

CProgramItemPtr CProgramManager::GetProgramByID(const CProgramID& ID)
{
	switch (ID.GetType())
	{
	case EProgramType::eAllPrograms:			return GetAll();
	case EProgramType::eProgramFile:			return GetProgramFile(ID.GetFilePath());
	case EProgramType::eFilePattern:			return GetPattern(ID.GetFilePath());
	case EProgramType::eWindowsService:			return GetService(ID.GetServiceTag());
	case EProgramType::eAppPackage:				return GetAppPackage(ID.GetAppContainerSid());
	default: Q_ASSERT(0); return NULL;
	}
}

CProgramItemPtr CProgramManager::UpdateProgramByID(const CProgramID& ID)
{
	auto Ret = theCore->GetProgram(ID);
	if (Ret.IsError())
		return CProgramItemPtr();

	return UpdateProgramImpl(Ret.GetValue());
}

CProgramItemPtr CProgramManager::UpdateProgramByUID(quint64 UID)
{
	auto Ret = theCore->GetProgram(UID);
	if (Ret.IsError())
		return CProgramItemPtr();

	return UpdateProgramImpl(Ret.GetValue());
}

CProgramItemPtr CProgramManager::UpdateProgramImpl(const QtVariant& Data)
{
	QWriteLocker Lock(&m_Mutex); 
	bool bAdded = false;

	QtVariantReader Reader(Data);

	quint64 uId = Reader.Find(API_V_PROG_UID);

	CProgramItemPtr pItem = m_Items.value(uId);
	if (!pItem) 
	{
		CProgramID ID;
		ID.FromVariant(Reader.Find(API_V_ID));

		EProgramType Type = (EProgramType)Reader.Find(API_V_PROG_TYPE).To<uint32>();
		pItem = MakeProgram(Type);
		if(!pItem)
			return nullptr;

		pItem->SetID(ID);
		pItem->SetUID(uId);
		m_Items.insert(uId, pItem);
		if (ID.GetType() != EProgramType::eUnknown) {
			AddProgramUnsafe(pItem);
			bAdded = true;
		}
	}

	pItem->FromVariant(Data);

	Lock.unlock();
	if(bAdded)
		emit ProgramsAdded();

	return pItem;
}

CProgramItemPtr CProgramManager::GetProgramByUID(quint64 UID, bool bCanUpdate)
{
	QReadLocker Lock(&m_Mutex);
	CProgramItemPtr pProgram = m_Items.value(UID);
	Lock.unlock();

	if(!pProgram){
		if (bCanUpdate)
			pProgram = UpdateProgramByUID(UID);
#ifdef _DEBUG
		else
			qDebug() << "Program not found by UID:" << UID;
#endif
	}
	return pProgram;
}

CProgramLibraryPtr CProgramManager::GetLibraryByUID(quint64 UID, bool bCanUpdate)
{
	QReadLocker Lock(&m_Mutex);

	CProgramLibraryPtr pLibrary = m_Libraries.value(UID);
	if (!pLibrary) {
		if (bCanUpdate) {
			Lock.unlock();
			UpdateLibs();
			Lock.relock();
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
	QReadLocker Lock(&m_Mutex);
	CProgramFilePtr pItem = m_PathMap.value(Path);
	Lock.unlock();

	if (!pItem) {
		CProgramID ID;
		ID.SetPath(Path);
		pItem = UpdateProgramByID(ID).objectCast<CProgramFile>();
	}
	return pItem;
}
	
CWindowsServicePtr CProgramManager::GetService(const QString& Id)
{
	QReadLocker Lock(&m_Mutex);
	CWindowsServicePtr pItem = m_ServiceMap.value(Id);
	Lock.unlock();

	if (!pItem) {
		CProgramID ID;
		ID.SetServiceTag(Id);
		pItem = UpdateProgramByID(ID).objectCast<CWindowsService>();
	}
	return pItem;
}

CAppPackagePtr CProgramManager::GetAppPackage(const QString& Id)
{
	QReadLocker Lock(&m_Mutex);
	CAppPackagePtr pItem = m_PackageMap.value(Id);
	Lock.unlock();

	if (!pItem) {
		CProgramID ID;
		ID.SetAppContainerSid(Id);
		pItem = UpdateProgramByID(ID).objectCast<CAppPackage>();
	}
	return pItem;
}

CProgramPatternPtr CProgramManager::GetPattern(const QString& Pattern)
{
	QReadLocker Lock(&m_Mutex);
	CProgramPatternPtr pItem = m_PatternMap.value(Pattern);
	Lock.unlock();

	if (!pItem) {
		CProgramID ID;
		ID.SetPath(Pattern);
		pItem = UpdateProgramByID(ID).objectCast<CProgramPattern>();
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

STATUS CProgramManager::RemoveProgramFrom(const CProgramItemPtr& pItem, const CProgramItemPtr& pParent, bool bDelRules, bool bKeepOne)
{
	return theCore->RemoveProgramFrom(pItem->GetUID(), pParent ? pParent->GetUID() : 0, bDelRules, bKeepOne);
}

/////////////////////////////////////////////////////////////////////////////
// Rules

bool CProgramManager::UpdateAllProgramRules()
{
	auto Ret = theCore->GetAllProgramRules();
	if (Ret.IsError())
		return false;

	QtVariant& Rules = Ret.GetValue();

	QHash<QFlexGuid, CProgramRulePtr> OldRules = m_ProgramRules;

	for (int i = 0; i < Rules.Count(); i++)
	{
		const QtVariant& Rule = Rules[i];

		QFlexGuid Guid;
		Guid.FromVariant(Rule[API_V_GUID]);

		QString ProgramPath = Rule[API_V_FILE_PATH].AsQStr();
		CProgramID ID;
		ID.SetPath(theCore->NormalizePath(ProgramPath, true));

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

	foreach(const QFlexGuid& Guid, OldRules.keys())
		RemoveProgramRule(OldRules.take(Guid));

	return true;
}

bool CProgramManager::UpdateProgramRule(const QFlexGuid& Guid)
{
	auto Ret = theCore->GetProgramRule(Guid);
	if (Ret.IsError())
		return false;

	QtVariant& Rule = Ret.GetValue();

	QFlexGuid Guid2;
	Guid2.FromVariant(Rule[API_V_GUID]);
	if (Guid2 != Guid)
		return false;

	QString ProgramPath = Rule[API_V_FILE_PATH].AsQStr();
	CProgramID ID;
	ID.SetPath(theCore->NormalizePath(ProgramPath, true));

	CProgramRulePtr pRule = m_ProgramRules.value(Guid2);
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

void CProgramManager::RemoveProgramRule(const QFlexGuid& Guid)
{
	CProgramRulePtr pRule = m_ProgramRules.value(Guid);
	if (pRule)
		RemoveProgramRule(pRule);
}

QSet<QFlexGuid> CProgramManager::GetProgramRuleIDs() const
{
	return ListToSet(m_ProgramRules.keys()); 
}

QList<CProgramRulePtr> CProgramManager::GetProgramRules(const QSet<QFlexGuid>& ProgramRuleIDs)
{
	QList<CProgramRulePtr> List;
	foreach(const QFlexGuid& Guid, ProgramRuleIDs) {
		CProgramRulePtr pProgramRulePtr = m_ProgramRules.value(Guid);
		if (pProgramRulePtr) List.append(pProgramRulePtr);
	}
	return List;
}

STATUS CProgramManager::SetProgramRule(const CProgramRulePtr& pRule)
{
	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

	return theCore->SetProgramRule(pRule->ToVariant(Opts));
}

RESULT(CProgramRulePtr) CProgramManager::GetProgramRule(const QFlexGuid& Guid)
{
	auto Ret = theCore->GetProgramRule(Guid);
	if (Ret.IsError())
		return Ret;

	QtVariant& Rule = Ret.GetValue();

	QString ProgramPath = Rule[API_V_FILE_PATH].AsQStr();
	CProgramID ID;
	ID.SetPath(theCore->NormalizePath(ProgramPath, true));

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