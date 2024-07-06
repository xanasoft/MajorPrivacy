#include "pch.h"
#include "HandleModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"

CHandleModel::CHandleModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CHandleModel::~CHandleModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex>	CHandleModel::Sync(const QList<CHandlePtr>& HandleList)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach(const CHandlePtr& pHandle, HandleList)
	{
		QVariant ID = pHandle->GetHandleRef();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SHandleNode* pNode = I != Old.end() ? static_cast<SHandleNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SHandleNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			//pNode->Path = Path;
			pNode->pHandle = pHandle;
			pNode->pProcess = theCore->ProcessList()->GetProcess(pHandle->GetProcessId(), true);
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
		if (pNode->Icon.isNull() && pNode->pProcess && !pNode->pProcess->GetIcon().isNull())
		{
			pNode->Icon = pNode->pProcess->GetIcon();
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
			case eName:				Value = pNode->pHandle->GetPath(EPathType::eDisplay); break;
			case eProgram:			Value = pNode->pProcess ? pNode->pProcess->GetPath(EPathType::eDisplay) : tr("PROCESS MISSING"); break;
			}

			SHandleNode::SValue& ColValue = pNode->Values[section];

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

CHandleModel::STreeNode* CHandleModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

CHandlePtr CHandleModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return CHandlePtr();

	SHandleNode* pNode = static_cast<SHandleNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pHandle;
}

int CHandleModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CHandleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");

		case eProgram:				return tr("Program");
		}
	}
	return QVariant();
}