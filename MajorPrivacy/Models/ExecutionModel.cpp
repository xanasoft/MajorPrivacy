#include "pch.h"
#include "ExecutionModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Library/Helpers/NtUtil.h"

CExecutionModel::CExecutionModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CExecutionModel::~CExecutionModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex> CExecutionModel::Sync(const QMap<SExecutionKey, SExecutionItemPtr>& List)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	for(auto X = List.begin(); X != List.end(); ++X)
	{
		QVariant ID = QString("%1_%2").arg(X.key().first).arg(X.key().second);

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SExecutionNode* pNode = I != Old.end() ? static_cast<SExecutionNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SExecutionNode*>(MkNode(ID));
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
		if (pNode->Icon.isNull())
		{
			pNode->Icon = pNode->pItem->pProg2 ? pNode->pItem->pProg2->GetIcon() : pNode->pItem->pProg1->GetIcon();
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
			case eName:				Value = pNode->pItem->pProg2 ? pNode->pItem->pProg2->GetNameEx() : pNode->pItem->pProg1->GetNameEx(); break;
			case eRole:				Value = pNode->pItem->pProg2 ? (pNode->pItem->Info.eRole == CProgramFile::SExecutionInfo::eActor ? tr("Actor") : tr("Target")) : ""; break;
			case eTimeStamp:		Value = pNode->pItem->Info.LastExecTime; break;
			case eStatus:			Value = pNode->pItem->Info.bBlocked; break;
			case eProgram:			Value = pNode->pItem->pProg2 ? pNode->pItem->pProg2->GetPath(EPathType::eDisplay) : pNode->pItem->pProg1->GetPath(EPathType::eDisplay); break;
			}

			SExecutionNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eTimeStamp:	ColValue.Formatted = Value.toULongLong() ? QDateTime::fromMSecsSinceEpoch(FILETIME2ms(Value.toULongLong())).toString("dd.MM.yyyy hh:mm:ss") : ""; break;
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

CExecutionModel::STreeNode* CExecutionModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

SExecutionItemPtr CExecutionModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return SExecutionItemPtr();

	SExecutionNode* pNode = static_cast<SExecutionNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pItem;
}

int CExecutionModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CExecutionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eRole:					return tr("Role");
		case eStatus:				return tr("Last Status");
		case eTimeStamp:			return tr("Time Stamp");
		case eProgram:				return tr("Program");
		}
	}
	return QVariant();
}