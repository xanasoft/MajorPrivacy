#include "pch.h"
#include "TweakModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Core/PrivacyCore.h"

CTweakModel::CTweakModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CTweakModel::~CTweakModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex>	CTweakModel::Sync(const CTweakPtr& pRoot)
{
	QList<QVariant> Added;
#pragma warning(push)
#pragma warning(disable : 4996)
	QMap<QList<QVariant>, QList<STreeNode*> > New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	auto pList = pRoot.objectCast<CTweakList>();
	foreach(const CTweakPtr& pTweak, pList->GetList())
		Sync(pTweak, QList<QVariant>(), New, Old, Added);

	QList<QModelIndex>	NewBranches;
	CTreeItemModel::Sync(New, Old, &NewBranches);
	return NewBranches;
}

void CTweakModel::Sync(const CTweakPtr& pTweak, const QList<QVariant>& Path, QMap<QList<QVariant>, QList<STreeNode*> >& New, QHash<QVariant, STreeNode*>& Old, QList<QVariant>& Added)
{
	QVariant ID = pTweak->GetName();

	QModelIndex Index;

	QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
	STweakNode* pNode = I != Old.end() ? static_cast<STweakNode*>(I.value()) : NULL;
	if (!pNode)
	{
		pNode = static_cast<STweakNode*>(MkNode(ID));
		pNode->Values.resize(columnCount());
		pNode->Path = Path;
		pNode->pTweak = pTweak;
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
	/*if (pNode->Icon.isNull() && )
	{
	pNode->Icon = 
	Changed = 1;
	}*/
	/*if (pNode->IsGray != ...) {
	pNode->IsGray = ...;
	Changed = 2; // set change for all columns
	}*/

	if (pNode->Status != pTweak->GetStatus()) {
		pNode->Status = pTweak->GetStatus();
		Changed = 1; // set the first column with the checkbox as changed
	}

	for (int section = 0; section < columnCount(); section++)
	{
		if (!IsColumnEnabled(section))
			continue; // ignore columns which are hidden

		QVariant Value;
		switch (section)
		{
		case eName: Value = pTweak->GetName(); break;
		case eStatus: Value = pTweak->GetStatusStr(); break;
		case eType: Value = pTweak->GetTypeStr(); break;
		//case eInfo: Value = pTweak->GetInfo(); break;
		}

		STweakNode::SValue& ColValue = pNode->Values[section];

		if (ColValue.Raw != Value)
		{
			if (Changed == 0)
				Changed = 1;
			ColValue.Raw = Value;

			/*switch (section)
			{
			case eName:			break;
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

	auto pList = pTweak.objectCast<CTweakList>();
	if (pList) {
		foreach(const CTweakPtr & pTweak, pList->GetList())
			Sync(pTweak, QList<QVariant>(Path) << ID, New, Old, Added);
	}
}

CTweakModel::STreeNode* CTweakModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

CTweakPtr CTweakModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
		return CTweakPtr();

	STweakNode* pNode = static_cast<STweakNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pTweak;
}

QVariant CTweakModel::Data(const QModelIndex& index, int role, int section) const
{
	if (!index.isValid())
		return QVariant();

	STweakNode* pNode = static_cast<STweakNode*>(index.internalPointer());
	ASSERT(pNode);

	if (role == Qt::ToolTipRole)
	{
		return pNode->pTweak->GetInfo();
	}
	else if (role == Qt::CheckStateRole)
	{
		if (section == eName)
		{
			ETweakStatus Status = pNode->pTweak->GetStatus();
			if (Status == ETweakStatus::eGroup)
				return QVariant();
			else if (Status == ETweakStatus::eSet || Status == ETweakStatus::eApplied)
				return Qt::Checked;
			else
				return Qt::Unchecked;
		}
	}
	else
		return CTreeItemModel::Data(index, role, section);

	return QVariant();
}

int CTweakModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CTweakModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eStatus:				return tr("Status");
		case eType:					return tr("Type");
		//case eInfo:					return tr("Info");
		}
	}
	return QVariant();
}