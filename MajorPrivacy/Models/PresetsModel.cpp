#include "pch.h"
#include "PresetsModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"

CPresetsModel::CPresetsModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CPresetsModel::~CPresetsModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex> CPresetsModel::Sync(const QList<CPresetPtr>& PresetList)
{
#pragma warning(push)
#pragma warning(disable : 4996)
	TNewNodesMap New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach(const CPresetPtr& pPreset, PresetList)
	{
		QVariant Guid = pPreset->GetGuid().ToQV();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(Guid);
		SPresetNode* pNode = I != Old.end() ? static_cast<SPresetNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SPresetNode*>(MkNode(Guid));
			pNode->Values.resize(columnCount());
			pNode->pPreset = pPreset;
			New[pNode->Path.count()][pNode->Path].append(pNode);
		}
		else
		{
			I.value() = NULL;
			pNode->pPreset = pPreset;
			Index = Find(m_Root, pNode);
		}

		// Update icon
		if (pNode->Icon.isNull() || pNode->Icon.value<QIcon>().isNull()) {
			QIcon icon = pPreset->GetIcon();
			if (!icon.isNull())
				pNode->Icon = icon;
		}

		// Update bold state for active presets
		bool bActive = pPreset->IsActive();
		if (pNode->IsBold != bActive) {
			pNode->IsBold = bActive;
		}

		int Col = 0;
		bool State = false;
		int Changed = 0;

		for (int section = 0; section < columnCount(); section++)
		{
			if (!IsColumnEnabled(section))
				continue;

			QVariant Value;
			switch (section)
			{
			case eName:			Value = pPreset->GetName(); break;
			case eDescription:	Value = pPreset->GetDescription(); break;
			}

			SPresetNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eName:
					ColValue.Formatted = CMajorPrivacy::GetResourceStr(pPreset->GetName());
					ColValue.ToolTip = pPreset->GetGuid().ToQS();
					break;
				case eDescription:
					ColValue.Formatted = CMajorPrivacy::GetResourceStr(pPreset->GetDescription());
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

	QList<QModelIndex> NewBranches;
	CTreeItemModel::Sync(New, Old, &NewBranches);
	return NewBranches;
}

CPresetPtr CPresetsModel::GetItem(const QModelIndex& index) const
{
	if (!index.isValid())
		return CPresetPtr();

	SPresetNode* pNode = static_cast<SPresetNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pPreset;
}

QString CPresetsModel::GetGuid(const QModelIndex& index) const
{
	if (!index.isValid())
		return QString();

	SPresetNode* pNode = static_cast<SPresetNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pPreset ? pNode->pPreset->GetGuid().ToQS() : QString();
}

int CPresetsModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CPresetsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:			return tr("Name");
		case eDescription:	return tr("Description");
		}
	}
	return QVariant();
}
