#include "pch.h"
#include "AccessListModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"
#include "..\Core\Access\ResLogEntry.h"
#include "../Helpers/IconCache.h"

CAccessListModel::CAccessListModel(QObject* parent)
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

CAccessListModel::~CAccessListModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex>	CAccessListModel::Sync(const QMap<CProgramItemPtr, QPair<quint64,QList<QPair<SAccessStatsPtr,SAccessItem::EType>>>>& Map)
{
#pragma warning(push)
#pragma warning(disable : 4996)
	QMap<QList<QVariant>, QList<STreeNode*> > New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	for(auto X = Map.begin(); X != Map.end(); ++X)
	{
		QVariant ID = (quint64)X.key().data();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SAccessNode* pNode = I != Old.end() ? static_cast<SAccessNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SAccessNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			//pNode->Path = Path;
			pNode->pProgram = X.key();
			New[pNode->Path].append(pNode);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		int Col = 0;
		bool State = false;
		int Changed = 0;
		if (pNode->Icon.isNull() && pNode->pProgram && !pNode->pProgram->GetIcon().isNull())
		{
			pNode->Icon = pNode->pProgram->GetIcon();
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
			case eName:				Value = QString(""); break; // no name update
			case eLastAccess:		Value = X.value().first; break;
			case eAccess:			Value = X.value().second.count(); break;
			}

			SAccessNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eName:				ColValue.Formatted = pNode->pProgram->GetNameEx(); break;
				case eLastAccess:		if(X.value().first) ColValue.Formatted = QDateTime::fromMSecsSinceEpoch(FILETIME2ms(X.value().first)).toString("dd.MM.yyyy hh:mm:ss.zzz"); break;
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

		Sync(X.key(), X.value().second, New, Old);
	}

	QList<QModelIndex>	NewBranches;
	CTreeItemModel::Sync(New, Old, &NewBranches);
	return NewBranches;
}

void CAccessListModel::Sync(const CProgramItemPtr& pItem, const QList<QPair<SAccessStatsPtr,SAccessItem::EType>>& List, QMap<QList<QVariant>, QList<STreeNode*> >& New, QHash<QVariant, STreeNode*>& Old)
{
	for (auto X = List.begin(); X != List.end(); ++X)
	{
		QVariant ID = (quint64)X->first.data();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SAccessNode* pNode = I != Old.end() ? static_cast<SAccessNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SAccessNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->Path = QVariantList() << (quint64)pItem.data();
			pNode->pItem = X->first;
			New[pNode->Path].append(pNode);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		int Col = 0;
		bool State = false;
		int Changed = 0;
		if (pNode->Icon.isNull())
		{
			switch (X->second)
			{
			case SAccessItem::eDevice:		pNode->Icon = m_DeviceIcon; break;
			case SAccessItem::eComputer:	pNode->Icon = m_MonitorIcon; break;
			case SAccessItem::eDrive:		pNode->Icon = m_DiskIcon; break;
			case SAccessItem::eFile:		pNode->Icon = m_FileIcon; //GetFileIcon(pNode->pItem->Path.Get(EPathType::eDisplay), 16); 
											break;
			case SAccessItem::eFolder:		pNode->Icon = m_DirectoryIcon; break;
			case SAccessItem::eNetwork:		pNode->Icon = m_NetworkIcon; break;
			case SAccessItem::eHost:		pNode->Icon = m_MonitorIcon; break;
			case SAccessItem::eObjects:		pNode->Icon = m_ObjectsIcon; break;
			case SAccessItem::eRegistry:	pNode->Icon = m_RegEditIcon; break;
			}
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
			case eName:				Value = pNode->pItem->Path; break;
			case eLastAccess:		Value = pNode->pItem->LastAccessTime; break;
			case eAccess:			Value = pNode->pItem->AccessMask; break;
			case eStatus:			Value = pNode->pItem->NtStatus; break;
			}

			SAccessNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eName:				ColValue.Formatted = theCore->NormalizePath(pNode->pItem->Path); break;
				case eLastAccess:		if(pNode->pItem->LastAccessTime) ColValue.Formatted = QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pNode->pItem->LastAccessTime)).toString("dd.MM.yyyy hh:mm:ss.zzz"); break;
				case eAccess:			ColValue.Formatted = CResLogEntry::GetAccessStr(pNode->pItem->AccessMask); break;
				case eStatus:			ColValue.Formatted = QString("0x%1 (%2)").arg(pNode->pItem->NtStatus, 8, 16, QChar('0')).arg(pNode->pItem->bBlocked ? tr("Blocked") : tr("Allowed")); break;
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
}

//CAccessListModel::STreeNode* CAccessListModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
//{ 
//	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);
//
//	if (!pNode->Values[0].Raw.isValid()) {
//		QStringList Paths = Id.toString().split("\\");
//		pNode->Values[0].Raw = Paths.last();
//	}
//
//	return pNode;
//}

SAccessStatsPtr CAccessListModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return SAccessStatsPtr();

	SAccessNode* pNode = static_cast<SAccessNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pItem;
}

int CAccessListModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CAccessListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName: return tr("Name");
		case eLastAccess: return tr("Last Access");
		case eAccess: return tr("Access");
		case eStatus: return tr("Status");
		}
	}
	return QVariant();
}