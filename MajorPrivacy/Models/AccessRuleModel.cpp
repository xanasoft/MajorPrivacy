#include "pch.h"
#include "AccessRuleModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"

CAccessRuleModel::CAccessRuleModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CAccessRuleModel::~CAccessRuleModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex>	CAccessRuleModel::Sync(const QList<CAccessRulePtr>& RuleList)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach(const CAccessRulePtr& pRule, RuleList)
	{
		QVariant Guid = pRule->GetGuid();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(Guid);
		SRuleNode* pNode = I != Old.end() ? static_cast<SRuleNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SRuleNode*>(MkNode(Guid));
			pNode->Values.resize(columnCount());
			//pNode->Path = Path;
			pNode->pRule = pRule;
			New[pNode->Path].append(pNode);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		if (pNode->pProg.isNull())
			pNode->pProg = theCore->ProgramManager()->GetProgramByID(pNode->pRule->GetProgramID());

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		int Col = 0;
		bool State = false;
		int Changed = 0;
		if (pNode->Icon.isNull() && pNode->pProg && !pNode->pProg->GetIcon().isNull())
		{
			pNode->Icon = pNode->pProg->GetIcon();
			Changed = 1;
		}
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
			case eName:				Value = pRule->GetName(); break;
			case eEnabled:			Value = pRule->IsEnabled(); break;
			case eAction:			Value = pRule->GetTypeStr(); break;
			case ePath:				Value = pRule->GetPath(); break;
			case eProgram:			Value = pRule->GetProgramPath(); break;
			}

			SRuleNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eName:				ColValue.Formatted = QString::fromStdWString(GetResourceStr(Value.toString().toStdWString())); break;
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

CAccessRuleModel::STreeNode* CAccessRuleModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

CAccessRulePtr CAccessRuleModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
		return CAccessRulePtr();

	SRuleNode* pNode = static_cast<SRuleNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pRule;
}

int CAccessRuleModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CAccessRuleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eEnabled:				return tr("Enabled");
		case eAction:				return tr("Action");
		case ePath:					return tr("Path");
		case eProgram:				return tr("Program");
		}
	}
	return QVariant();
}