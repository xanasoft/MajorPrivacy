#include "pch.h"
#include "DnsRuleModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MajorPrivacy.h"
#include "../Windows/FirewallRuleWnd.h"

CDnsRuleModel::CDnsRuleModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CDnsRuleModel::~CDnsRuleModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex>	CDnsRuleModel::Sync(const QList<CDnsRulePtr>& RuleList)
{
#pragma warning(push)
#pragma warning(disable : 4996)
	TNewNodesMap New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach(const CDnsRulePtr& pRule, RuleList)
	{
		QVariant Guid = pRule->GetGuid().ToQV();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(Guid);
		SRuleNode* pNode = I != Old.end() ? static_cast<SRuleNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SRuleNode*>(MkNode(Guid));
			pNode->Values.resize(columnCount());
			//pNode->Path = Path;
			pNode->pRule = pRule;
			New[pNode->Path.count()][pNode->Path].append(pNode);
		}
		else
		{
			I.value() = NULL;
			pNode->pRule = pRule;
			Index = Find(m_Root, pNode);
		}

		//if (pNode->pProg.isNull() || pNode->pRule->GetProgramID() != pNode->pProg->GetID()) {
		//	pNode->pProg = theCore->ProgramManager()->GetProgramByID(pNode->pRule->GetProgramID());
		//	pNode->Icon.clear();
		//}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		int Col = 0;
		bool State = false;
		int Changed = 0;
		//if (pNode->Icon.isNull() && pNode->pProg && !pNode->pProg->GetIcon().isNull())
		//{
		//	pNode->Icon = pNode->pProg->GetIcon();
		//	Changed = 1;
		//}
		if (pNode->IsGray != !pRule->IsEnabled()) {
			pNode->IsGray = !pRule->IsEnabled();
			Changed = 2; // set change for all columns
		}
		if (pNode->IsItalic != pRule->IsTemporary()) {
			pNode->IsItalic = pRule->IsTemporary();
			Changed = 2; // set change for all columns
		}

		for (int section = 0; section < columnCount(); section++)
		{
			if (!IsColumnEnabled(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch (section)
			{
			case eHostName:			Value = pRule->GetHostName(); break;
			case eEnabled:			Value = pRule->IsEnabled(); break;
			case eHitCount:			Value = pRule->GetHitCount(); break;
			case eAction:			Value = (uint32)pRule->GetAction(); break;
			}

			SRuleNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eEnabled:			ColValue.Formatted = (pRule->IsEnabled() ? tr("Yes") : tr("No")) + (pRule->IsTemporary() ? tr(" (Temporary)") : ""); break;
					case eAction:			ColValue.Formatted = pRule->GetActionStr(); { QColor Color = pRule->GetAction() == CDnsRule::eBlock ? QColor(255, 182, 193) : QColor(144, 238, 144); if(Color.isValid()) ColValue.Color = Color; } break;
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

CDnsRuleModel::STreeNode* CDnsRuleModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

CDnsRulePtr CDnsRuleModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return CDnsRulePtr();

	SRuleNode* pNode = static_cast<SRuleNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pRule;
}

int CDnsRuleModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CDnsRuleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eHostName:				return tr("Host Name");
		case eEnabled:				return tr("Enabled");
		case eHitCount:				return tr("Hit Count");
		case eAction:				return tr("Action");
		case eEmpty:				return "";
		}
	}
	return QVariant();
}