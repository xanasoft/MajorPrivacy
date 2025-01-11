#include "pch.h"
#include "VolumeModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"

CVolumeModel::CVolumeModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CVolumeModel::~CVolumeModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex>	CVolumeModel::Sync(const QList<CVolumePtr>& VolumeList)
{
#pragma warning(push)
#pragma warning(disable : 4996)
	QMap<QList<QVariant>, QList<STreeNode*> > New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach(const CVolumePtr& pVolume, VolumeList)
	{
		QVariant ID = pVolume->GetImagePath();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SVolumeNode* pNode = I != Old.end() ? static_cast<SVolumeNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SVolumeNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			//pNode->Path = Path;
			pNode->pVolume = pVolume;
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
			pNode->Icon = pNode->pVolume->IsFolder() ? pNode->Icon = QIcon(":/Icons/Folder.png") :QIcon(":/Icons/SecureDisk.png");
			Changed = 1;
		}
		if (pNode->IsGray != (pNode->pVolume->GetStatus() == CVolume::eFolder)) {
			pNode->IsGray = (pNode->pVolume->GetStatus() == CVolume::eFolder);
			Changed = 2; // set change for all columns
		}

		for (int section = 0; section < columnCount(); section++)
		{
			if (!IsColumnEnabled(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch (section)
			{
			case eName:				Value = pVolume->GetName(); break;
			case eStatus:			Value = pVolume->GetStatus(); break;
			case eMountPoint:		Value = pVolume->GetMountPoint(); break;
			case eTotalSize:		Value = pVolume->GetVolumeSize(); break;
			case eFullPath:			Value = pVolume->GetImagePath(); break;
			}

			SVolumeNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eStatus:		ColValue.Formatted = pVolume->GetStatusStr(); break;
				case eTotalSize:	ColValue.Formatted = FormatSize(Value.toULongLong()); break;
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

CVolumeModel::STreeNode* CVolumeModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

CVolumePtr CVolumeModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return CVolumePtr();

	SVolumeNode* pNode = static_cast<SVolumeNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pVolume;
}

int CVolumeModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CVolumeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eStatus:				return tr("Status");
		case eMountPoint:			return tr("Mount Point");
		case eTotalSize:			return tr("Total Size");
		case eFullPath:				return tr("Full Path");
		}
	}
	return QVariant();
}