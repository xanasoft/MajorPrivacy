#include "pch.h"
#include "HashDBModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MajorPrivacy.h"
#include "../Windows/FirewallRuleWnd.h"

CHashDBModel::CHashDBModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CHashDBModel::~CHashDBModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex>	CHashDBModel::Sync(const QMap<SHashItemKey, SHashItemPtr>& List)
{
#pragma warning(push)
#pragma warning(disable : 4996)
	TNewNodesMap New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	for(auto X = List.begin(); X != List.end(); ++X)
	{
		QVariant ID = X.value()->ID;

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SHashNode* pNode = I != Old.end() ? static_cast<SHashNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SHashNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pItem = X.value();
			pNode->Path = pNode->pItem->Parents;
			New[pNode->Path.count()][pNode->Path].append(pNode);
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
			switch (pNode->pItem->Type)
			{
				case EHashType::eFileHash: pNode->Icon = QIcon(":/Icons/SignFile.png"); break;
				case EHashType::eCertHash: pNode->Icon = QIcon(":/Icons/Cert.png"); break;
			}
			Changed = 1;
		}
		if (pNode->pItem->pEntry && pNode->IsGray != !pNode->pItem->pEntry->IsEnabled()) {
			pNode->IsGray = !pNode->pItem->pEntry->IsEnabled();
			Changed = 2; // set change for all columns
		}
		if (pNode->pItem->pEntry && pNode->IsItalic != pNode->pItem->pEntry->IsTemporary()) {
			pNode->IsItalic = pNode->pItem->pEntry->IsTemporary();
			Changed = 2; // set change for all columns
		}

		for (int section = 0; section < columnCount(); section++)
		{
			if (!IsColumnEnabled(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch (section)
			{
			//case eName:				Value = pNode->pItem->pEntry ? pNode->pItem->pEntry->GetHash().toHex().toUpper() : pNode->pItem->Name; break;
			//case eValue:			Value = pNode->pItem->pEntry && pNode->pItem->pEntry->GetType() == EHashType::eFileHash ? pNode->pItem->pEntry->GetName() : ""; break;
			case eName:				Value = pNode->pItem->Name; break;
			case eValue:			Value = pNode->pItem->pEntry ? QString::fromLatin1(pNode->pItem->pEntry->GetHash().toHex().toUpper()) : ""; break;
			}

			SHashNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				/*switch (section)
				{
				case eName:			ColValue.Formatted = ; break;
				}*/
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

CHashDBModel::STreeNode* CHashDBModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

SHashItemPtr CHashDBModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return SHashItemPtr();

	SHashNode* pNode = static_cast<SHashNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pItem;
}

int CHashDBModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CHashDBModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:				return tr("Name");
		case eValue:			return tr("Value");
		}
	}
	return QVariant();
}