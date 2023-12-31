#include "pch.h"
#include "ProgramManager.h"
#include "../PrivacyCore.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Service/ServiceAPI.h"
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
//			else if (Type == SVC_API_ID_TYPE_All)		pItem = CProgramItemPtr(new CAllProgram());
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

void CProgramManager::Update()
{
	auto Ret = theCore->GetPrograms();
	if (Ret.IsError())
		return;

	//UpdateGroup(m_Root, Ret.GetValue());

	const XVariant& Items = Ret.GetValue();

	QMap<quint64, QList<quint64>> Tree;

	QMap<quint64, CProgramItemPtr> OldMap = m_Items;

	Items.ReadRawList([&](const CVariant& vData) {
		const XVariant& Item = *(XVariant*)&vData;

		quint64 uId = Item.Find(SVC_API_PROG_UID);

		CProgramItemPtr pItem = OldMap.take(uId);
		if (!pItem) 
		{
			CProgramID ID;
			ID.FromVariant(Item.Find(SVC_API_PROG_ID));

			QString Type = Item.Find(SVC_API_ID_TYPE).AsQStr();
			if (Type == SVC_API_ID_TYPE_FILE)			pItem = CProgramItemPtr(new CProgramFile());
			else if (Type == SVC_API_ID_TYPE_PATTERN)	pItem = CProgramItemPtr(new CProgramPattern());
			else if (Type == SVC_API_ID_TYPE_INSTALL)	pItem = CProgramItemPtr(new CAppInstallation());
			else if (Type == SVC_API_ID_TYPE_SERVICE)	pItem = CProgramItemPtr(new CWindowsService());
			else if (Type == SVC_API_ID_TYPE_PACKAGE)	pItem = CProgramItemPtr(new CAppPackage());
			else if (Type == SVC_API_ID_TYPE_All)		pItem = CProgramItemPtr(new CAllProgram());
			else if (Type == SVC_API_ID_TYPE_GROUP)		pItem = CProgramItemPtr(new CProgramGroup());
			else 
				return; // unknown type
			pItem->SetUID(uId);
			pItem->SetID(ID);
			m_Items.insert(uId, pItem);
			if(ID.GetType() != CProgramID::eUnknown)
				AddProgram(pItem);
		}

		pItem->FromVariant(Item);

		if(pItem->inherits("CProgramSet"))
			Tree.insert(uId, Item.Find(SVC_API_PROG_ITEMS).AsQList<quint64>());
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
		m_Items.remove(pItem->GetUID());
		RemoveProgram(pItem);
	}

	m_Root->CountStats();
}

void CProgramManager::AddProgram(const CProgramItemPtr& pItem)
{
	const CProgramID& ID = pItem->GetID();
	switch (ID.GetType())
	{
	case CProgramID::eAll:		m_pAll = pItem.objectCast<CProgramSet>();
	case CProgramID::eFile:		m_PathMap.insert(ID.GetFilePath(), pItem.objectCast<CProgramFile>()); break;
	case CProgramID::eService:	m_ServiceMap.insert(ID.GetServiceTag(), pItem.objectCast<CWindowsService>()); break;
	case CProgramID::eApp:		m_PackageMap.insert(ID.GetAppContainerSid(), pItem.objectCast<CAppPackage>()); break;
	//default: // ignore
	}
}

void CProgramManager::RemoveProgram(const CProgramItemPtr& pItem)
{
	const CProgramID& ID = pItem->GetID();
	switch (ID.GetType())
	{
	case CProgramID::eFile:		m_PathMap.remove(ID.GetFilePath()); break;
	case CProgramID::eService:	m_ServiceMap.remove(ID.GetServiceTag()); break;
	case CProgramID::eApp:		m_PackageMap.remove(ID.GetAppContainerSid()); break;
	//default: // ignore
	}
}

CProgramItemPtr CProgramManager::GetProgramByID(const CProgramID& ID)
{
	switch (ID.GetType())
	{
	case CProgramID::eAll:		return m_pAll;
	case CProgramID::eFile:		return GetProgramFile(ID.GetFilePath());
	case CProgramID::eService:	return GetService(ID.GetServiceTag());
	case CProgramID::eApp:		return GetAppPackage(ID.GetAppContainerSid());
	default:					return NULL;
	}
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