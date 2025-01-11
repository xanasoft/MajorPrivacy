#include "pch.h"
#include "IngressModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Windows/ProgramRuleWnd.h"
#include "../Core/Processes/ExecLogEntry.h"


CIngressModel::CIngressModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CIngressModel::~CIngressModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex> CIngressModel::Sync(const QMap<SIngressKey, SIngressItemPtr>& List)
{
#pragma warning(push)
#pragma warning(disable : 4996)
	QMap<QList<QVariant>, QList<STreeNode*> > New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	for(auto X = List.begin(); X != List.end(); ++X)
	{
		QVariant ID = QString("%1_%2").arg(X.key().first).arg(X.key().second);

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SIngressNode* pNode = I != Old.end() ? static_cast<SIngressNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SIngressNode*>(MkNode(ID));
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
			case eRole:				Value = pNode->pItem->pProg2 ? (CExecLogEntry::GetRoleStr(pNode->pItem->Info.Role)) : ""; break;
			case eTimeStamp:		Value = pNode->pItem->Info.LastAccessTime; break;
			case eStatus:			Value = pNode->pItem->Info.bBlocked; break;
			case eOperation:		Value = (((quint64)pNode->pItem->Info.ThreadAccessMask) << 32) | ((quint64)pNode->pItem->Info.ProcessAccessMask); break;
			case eProgram:			Value = pNode->pItem->pProg2 ? pNode->pItem->pProg2->GetPath() : pNode->pItem->pProg1->GetPath(); break;
			}

			SIngressNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eRole:			if (pNode->pItem->pProg2) { QColor Color = CProgramRuleWnd::GetRoleColor(pNode->pItem->Info.Role); if (Color.isValid()) ColValue.Color = Color; } break;
				case eTimeStamp:	ColValue.Formatted = Value.toULongLong() ? QDateTime::fromMSecsSinceEpoch(FILETIME2ms(Value.toULongLong())).toString("dd.MM.yyyy hh:mm:ss") : ""; break;
				case eStatus:		ColValue.Formatted = Value.toBool() ? tr("Blocked") : "";//tr("Allowed"); 
									ColValue.Color = Value.toBool() ? QColor(255, 182, 193) : QVariant(); //QColor(144, 238, 144);
									break;
				case eOperation:	ColValue.Formatted = CExecLogEntry::GetAccessStr(pNode->pItem->Info.ProcessAccessMask, pNode->pItem->Info.ThreadAccessMask); 
									ColValue.ToolTip = CExecLogEntry::GetAccessStrEx(pNode->pItem->Info.ProcessAccessMask, pNode->pItem->Info.ThreadAccessMask);
									{ QColor Color = CExecLogEntry::GetAccessColor(pNode->pItem->Info.ProcessAccessMask, pNode->pItem->Info.ThreadAccessMask); if (Color.isValid()) ColValue.Color = Color; }
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

	QList<QModelIndex>	NewBranches;
	CTreeItemModel::Sync(New, Old, &NewBranches);
	return NewBranches;
}

CIngressModel::STreeNode* CIngressModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

SIngressItemPtr CIngressModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return SIngressItemPtr();

	SIngressNode* pNode = static_cast<SIngressNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pItem;
}

int CIngressModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CIngressModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eRole:					return tr("Role");
		case eStatus:				return tr("Status");
		case eOperation:			return tr("Operation");
		case eTimeStamp:			return tr("Time Stamp");
		case eProgram:				return tr("Program");
		}
	}
	return QVariant();
}