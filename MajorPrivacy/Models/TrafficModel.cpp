#include "pch.h"
#include "TrafficModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Service/ServiceAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Core/PrivacyCore.h"

CTrafficModel::CTrafficModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CTrafficModel::~CTrafficModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex> CTrafficModel::Sync(const QMap<quint64, STrafficItemPtr>& List)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	for(auto X = List.begin(); X != List.end(); ++X)
	{
		QVariant ID = X.key();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		STrafficNode* pNode = I != Old.end() ? static_cast<STrafficNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<STrafficNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pItem = X.value();
			QList<QVariant> Path;
			if(pNode->pItem->Parent.isValid())
				Path.append(pNode->pItem->Parent);
			pNode->Path = Path;
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
		if (pNode->Icon.isNull() && pNode->pItem->pProg && !pNode->pItem->pProg->GetIcon().isNull())
		{
			pNode->Icon = pNode->pItem->pProg->GetIcon();
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
			case eName:				Value = pNode->pItem->pProg ? pNode->pItem->pProg->GetNameEx() : pNode->pItem->pEntry->GetHostName(); break;
			case eLastActive:		Value = pNode->pItem->pEntry->GetLastActivity(); break;
			case eUploaded:			Value = pNode->pItem->pEntry->GetUploadTotal(); break;
			case eDownloaded:		Value = pNode->pItem->pEntry->GetDownloadTotal(); break;
			}

			STrafficNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eLastActive:	ColValue.Formatted = Value.toULongLong() ? QDateTime::fromMSecsSinceEpoch(Value.toULongLong()).toString("dd.MM.yyyy hh:mm:ss") : ""; break;
				case eUploaded:		ColValue.Formatted = FormatSize(Value.toULongLong()); break;
				case eDownloaded:	ColValue.Formatted = FormatSize(Value.toULongLong()); break;
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

	QList<QModelIndex>	NewBranches;
	CTreeItemModel::Sync(New, Old, &NewBranches);
	return NewBranches;
}

CTrafficModel::STreeNode* CTrafficModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

STrafficItemPtr CTrafficModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return STrafficItemPtr();

	STrafficNode* pNode = static_cast<STrafficNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pItem;
}

int CTrafficModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CTrafficModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eLastActive:			return tr("Last Activity");
		case eUploaded:				return tr("Uploaded");
		case eDownloaded:			return tr("Downloaded");
		}
	}
	return QVariant();
}