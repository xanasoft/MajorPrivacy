#include "pch.h"
#include "LibraryModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Library/Helpers/NtUtil.h"

CLibraryModel::CLibraryModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CLibraryModel::~CLibraryModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex> CLibraryModel::Sync(const QMap<SLibraryKey, SLibraryItemPtr>& List)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	for(auto X = List.begin(); X != List.end(); ++X)
	{
		QVariant ID = QString("%1_%2").arg(X.key().first).arg(X.key().second);

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SLibraryNode* pNode = I != Old.end() ? static_cast<SLibraryNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SLibraryNode*>(MkNode(ID));
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
			if (!pNode->pItem->pProg)
			{
				pNode->Icon = CProgramLibrary::DefaultIcon();
				Changed = 1;
			}
			else if (!pNode->pItem->pProg->GetIcon().isNull())
			{
				pNode->Icon = pNode->pItem->pProg->GetIcon();
				Changed = 1;
			}
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
			case eName:				Value = pNode->pItem->pLibrary ? pNode->pItem->pLibrary->GetName() : pNode->pItem->pProg->GetNameEx(); break;
			case eLastLoadTime:		Value = pNode->pItem->Info.LastLoadTime; break;
			case eSignAuthority:	Value = (pNode->pItem->pLibrary ? pNode->pItem->Info.Sign.Data : pNode->pItem->pProg->GetSignInfo().Data); break;
			case eStatus:			Value = (uint32)pNode->pItem->Info.LastStatus; break;
			case eNumber:			Value = pNode->pItem->pLibrary ? pNode->pItem->Info.TotalLoadCount : pNode->pItem->Count; break;
			case eModule:			Value = pNode->pItem->pLibrary ? pNode->pItem->pLibrary->GetPath(EPathType::eDisplay) : pNode->pItem->pProg->GetPath(EPathType::eDisplay); break;
			}

			SLibraryNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eLastLoadTime:		ColValue.Formatted = Value.toULongLong() ? QDateTime::fromMSecsSinceEpoch(FILETIME2ms(Value.toULongLong())).toString("dd.MM.yyyy hh:mm:ss") : ""; break;
				case eSignAuthority:	ColValue.Formatted = CProgramFile::GetSignatureInfoStr(SLibraryInfo::USign{Value.toULongLong()}); break;
				case eStatus:			ColValue.Formatted = CProgramFile::GetLibraryStatusStr((EEventStatus)Value.toUInt()); break;
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

CLibraryModel::STreeNode* CLibraryModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

SLibraryItemPtr CLibraryModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return SLibraryItemPtr();

	SLibraryNode* pNode = static_cast<SLibraryNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pItem;
}

int CLibraryModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CLibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eLastLoadTime:			return tr("Last Load");
		case eSignAuthority:		return tr("Signature");
		case eStatus:				return tr("Last Status");
		case eNumber:				return tr("Count");
		case eModule:				return tr("Module");
		}
	}
	return QVariant();
}