#include "pch.h"
#include "AccessModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Helpers/IconCache.h"
#include "../../Library/Helpers/NtUtil.h"
#include "..\Core\Access\ResLogEntry.h"

CAccessModel::CAccessModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;

	m_DeviceIcon = QIcon(":/Icons/CPU.png");
	m_MonitorIcon = QIcon(":/Icons/Monitor.png");
	m_DiskIcon = QIcon(":/Icons/Disk.png");
	m_FileIcon = QIcon(":/Icons/File.png");
	m_DirectoryIcon = QIcon(":/Icons/Directory.png");
	m_NetworkIcon = QIcon(":/Icons/Network.png");
	m_ObjectsIcon = QIcon(":/Icons/Objects.png");
	m_RegEditIcon = QIcon(":/Icons/RegEdit.png");
}

CAccessModel::~CAccessModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

#ifndef USE_ACCESS_TREE

QList<QModelIndex> CAccessModel::Sync(const QHash<QString, SAccessItemPtr>& List)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old;
	//QHash<QVariant, STreeNode*> Old = m_Map;

	for(auto X = List.constBegin(); X != List.constEnd(); ++X)
	{
		QVariant ID = X.value()->Path;

		QModelIndex Index;

		//QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		//SAccessNode* pNode = I != Old.end() ? static_cast<SAccessNode*>(I.value()) : NULL;
		QHash<QVariant, STreeNode*>::iterator I = m_Map.find(ID);
		SAccessNode* pNode = I != m_Map.end() ? static_cast<SAccessNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SAccessNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pItem = X.value();

			QList<QVariant> Path;
			QString sPath;
			QStringList lPath = X.value()->Path.split("\\");
			for(int i = 0; i < lPath.count() - 1; i++) {
				if(!sPath.isEmpty())
					sPath += "\\";
				sPath += lPath[i];
				Path.append(sPath);
			}
			pNode->Path = Path;
			New[pNode->Path].append(pNode);
		}
		else
		{
			//Old.erase(I);
			//I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));
#else

QList<QModelIndex> CAccessModel::Sync(const SAccessItemPtr& pRoot)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old;// = m_Map;

	SAccessNode* pRootNode = static_cast<SAccessNode*>(m_Root);
	pRootNode->pItem = pRoot;
	//Sync(pRootNode, New, Old);
	Sync(pRootNode, New, m_Map);

	QList<QModelIndex>	NewBranches;
	//qDebug() << "Sync new" << New.size() << ", old" << Old.size();
	CTreeItemModel::Sync(New, Old, &NewBranches);
	return NewBranches;
}

void CAccessModel::Sync(SAccessNode* pParent, QMap<QList<QVariant>, QList<STreeNode*> >& New, QHash<QVariant, STreeNode*>& Old)
{
	foreach(const SAccessItemPtr& pItem, pParent->pItem->Branches)
	{
		QVariant ID = (quint64)pItem.data();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SAccessNode* pNode = I != Old.end() ? static_cast<SAccessNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SAccessNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			if(pParent->ID.isValid())
				pNode->Path = QList<QVariant>(pParent->Path) << pParent->ID;
			pNode->pItem = pItem;
			New[pNode->Path].append(pNode);
		}
		else
		{
			//I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		// dont update if nothign changed
		if(pNode->LastAccess == pNode->pItem->LastAccess)
			continue;
		pNode->LastAccess = pNode->pItem->LastAccess;

		Sync(pNode, New, Old);

#endif
		int Col = 0;
		bool State = false;
		int Changed = 0;
#ifndef USE_ACCESS_TREE
		if (pNode->Icon.isNull()/* && pNode->pItem->pProg && !pNode->pItem->pProg->GetIcon().isNull()*/ || pNode->ChildrenChanged)
#else
		if (pNode->Icon.isNull()/* && pNode->pItem->pProg && !pNode->pItem->pProg->GetIcon().isNull()*/)
#endif
		{
			pNode->ChildrenChanged = false;

			switch (pNode->pItem->Type)
			{
			case SAccessItem::eDevice:		pNode->Icon = m_DeviceIcon; break;
			case SAccessItem::eComputer:	pNode->Icon = m_MonitorIcon; break;
			case SAccessItem::eDrive:		pNode->Icon = m_DiskIcon; break;
			case SAccessItem::eFile: // or folder
#ifndef USE_ACCESS_TREE
				if(pNode->Children.isEmpty()) {
#else
				if(pNode->pItem->Branches.isEmpty()) {
#endif
					if(pNode->pItem->Name.contains("."))
						pNode->Icon = GetFileIcon(Split2(pNode->pItem->Name, ".", true).second, 16);
					else
						pNode->Icon = pNode->Icon = m_FileIcon;
					break;
				}
			case SAccessItem::eFolder:
				pNode->Icon = pNode->Icon = m_DirectoryIcon;
				break;
			case SAccessItem::eNetwork:		pNode->Icon = m_NetworkIcon; break;
			case SAccessItem::eHost:		pNode->Icon = m_MonitorIcon; break;
			case SAccessItem::eObjects:		pNode->Icon = m_ObjectsIcon; break;
			case SAccessItem::eRegistry:	pNode->Icon = m_RegEditIcon; break;
			}

			//pNode->Icon = pNode->pItem->pProg->GetIcon();
			Changed = 1;
		}
		/*if (pNode->IsGray != ...) {
			pNode->IsGray = ...;
			Changed = 2; // set change for all columns
		}*/

		for (int section = 0; section < columnCount(); section++)
		{
			if (!IsColumnEnabled(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch (section)
			{
			case eName:				Value = pNode->pItem->Name; break;
			case eLastAccess:		if(pNode->pItem->LastAccess) Value = pNode->pItem->LastAccess;
			}

			SAccessNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				//case eName:			ColValue.Formatted = pNode->Name; break;
				case eLastAccess:		if(pNode->pItem->LastAccess)
											ColValue.Formatted = QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pNode->pItem->LastAccess)).toString("dd.MM.yyyy hh:mm:ss.zzz"); 
										break;
				}
			}

			if (State != (Changed != 0))
			{
				if (State && Index.isValid())
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section - 1, pNode));
				State = (Changed != 0);
				Col = section;
			}
			if (Changed == 1)
				Changed = 0;
		}
		if (State && Index.isValid())
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount() - 1, pNode));

	}

#ifndef USE_ACCESS_TREE
	QList<QModelIndex>	NewBranches;
	//qDebug() << "Sync new" << New.size() << ", old" << Old.size();
	CTreeItemModel::Sync(New, Old, &NewBranches);
	return NewBranches;
#endif
}

CAccessModel::STreeNode* CAccessModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

SAccessItemPtr CAccessModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return SAccessItemPtr();

	SAccessNode* pNode = static_cast<SAccessNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pItem;
}

int CAccessModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CAccessModel::NodeData(STreeNode* pNode, int role, int section) const
{
	if (pNode->Values.size() <= section)
		return QVariant();

	SAccessNode* pAccessNode = static_cast<SAccessNode*>(pNode);

	switch (role)
	{
	case Qt::ToolTipRole:
		switch(section)
		{
		case eName:			return pAccessNode->pItem ? pAccessNode->pItem->NtPath : QVariant();
		case eLastAccess:	return pAccessNode->pItem ? CResLogEntry::GetAccessStr(pAccessNode->pItem->AccessMask) : QVariant();
		}
		
	case Qt::ForegroundRole:
		if (section == eLastAccess) 
		{
			uint32 uAccessMask = pAccessNode->pItem->AccessMask;

			if((uAccessMask & GENERIC_WRITE) == GENERIC_WRITE
			|| (uAccessMask & GENERIC_ALL) == GENERIC_ALL
			|| (uAccessMask & DELETE) == DELETE
			|| (uAccessMask & WRITE_DAC) == WRITE_DAC
			|| (uAccessMask & WRITE_OWNER) == WRITE_OWNER
			|| (uAccessMask & FILE_WRITE_DATA) == FILE_WRITE_DATA
			|| (uAccessMask & FILE_APPEND_DATA) == FILE_APPEND_DATA)
				return QColor(Qt::red);

			if((uAccessMask & FILE_WRITE_ATTRIBUTES) == FILE_WRITE_ATTRIBUTES
			|| (uAccessMask & FILE_WRITE_EA) == FILE_WRITE_EA)
				return QColor(255, 165, 0);

			if((uAccessMask & GENERIC_READ) == GENERIC_READ
			|| (uAccessMask & GENERIC_EXECUTE) == GENERIC_EXECUTE
			|| (uAccessMask & READ_CONTROL) == READ_CONTROL
			|| (uAccessMask & FILE_READ_DATA) == FILE_READ_DATA
			|| (uAccessMask & FILE_EXECUTE) == FILE_EXECUTE)
				return QColor(Qt::green);
			
			if((uAccessMask & SYNCHRONIZE) == SYNCHRONIZE
			|| (uAccessMask & FILE_READ_ATTRIBUTES) == FILE_READ_ATTRIBUTES
			|| (uAccessMask & FILE_READ_EA) == FILE_READ_EA)
				return QColor(Qt::blue);
		}

	default:
		return CTreeItemModel::NodeData(pNode, role, section);
	}
}

QVariant CAccessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eLastAccess:			return tr("Last Access");
		}
	}
	return QVariant();
}