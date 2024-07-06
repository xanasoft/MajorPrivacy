#include "pch.h"
#include "AccessModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Core/PrivacyCore.h"
#include <QFileIconProvider>

CAccessModel::CAccessModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CAccessModel::~CAccessModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex> CAccessModel::Sync(const QMap<QString, SAccessItemPtr>& List)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	QFileIconProvider IconProvider;

	for(auto X = List.begin(); X != List.end(); ++X)
	{
		QString sID = X.key();
		if(sID.left(1) != "\\") sID.prepend("\\"); 

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(sID);
		STrafficNode* pNode = I != Old.end() ? static_cast<STrafficNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<STrafficNode*>(MkNode(sID));
			pNode->Values.resize(columnCount());
			pNode->pItem = X.value();

			auto PathName = Split2(sID, "\\", true);	
			pNode->Name = PathName.second;
			pNode->bRoot = PathName.first.isEmpty();
			pNode->bDevice = sID.length() < 3 || sID.at(2) != ":"; // mind the leading '\'
			if (pNode->bRoot) {
				if(pNode->bDevice)
					pNode->Name.prepend("\\");
				else
					pNode->Name.append("\\");
			}
			QList<QVariant> Path;
			QString sPath;
			foreach(const QString& Name, SplitStr(PathName.first, "\\")) {
				sPath += "\\" + Name;
				Path.append(sPath);
			}
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
		if (pNode->Icon.isNull()/* && pNode->pItem->pProg && !pNode->pItem->pProg->GetIcon().isNull()*/ || pNode->ChildrenChanged)
		{
			pNode->ChildrenChanged = false;

			if (pNode->bDevice)
				pNode->Icon = IconProvider.icon(QFileIconProvider::Computer);
			else if (pNode->bRoot)
				pNode->Icon = IconProvider.icon(QFileIconProvider::Drive);
			else if(!pNode->Children.isEmpty())
				pNode->Icon = IconProvider.icon(QFileIconProvider::Folder);
			else {
				//pNode->Icon = IconProvider.icon(QFileIconProvider::File);
				QString sPath = pNode->ID.toString().mid(1); // strip leading '\'
				pNode->Icon = IconProvider.icon(QFileInfo(sPath));
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
			case eName:				Value = pNode->Name; break;
			}

			STrafficNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				
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

	STrafficNode* pNode = static_cast<STrafficNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pItem;
}

int CAccessModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CAccessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		}
	}
	return QVariant();
}