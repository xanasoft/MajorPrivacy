#include "pch.h"
#include "ProgramRuleModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MajorPrivacy.h"

CProgramRuleModel::CProgramRuleModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CProgramRuleModel::~CProgramRuleModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex>	CProgramRuleModel::Sync(const QList<CProgramRulePtr>& RuleList)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach(const CProgramRulePtr& pRule, RuleList)
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
			pNode->pRule = pRule;
			Index = Find(m_Root, pNode);
		}

		if (pNode->pProg.isNull() || pNode->pRule->GetProgramID() != pNode->pProg->GetID()) {
			pNode->pProg = theCore->ProgramManager()->GetProgramByID(pNode->pRule->GetProgramID());
			pNode->Icon.clear();
		}

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
			case eSignature:		Value = pRule->GetSignatureLevel(); break;
			case eOnSpawn:			Value = ((uint32)pRule->GetOnTrustedSpawn()) << 16 | ((uint32)pRule->GetOnSpawn()); break;
			case eProgram:			Value = pRule->GetProgramNtPath(); break;
			}

			SRuleNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eName:				ColValue.Formatted = CMajorPrivacy::GetResourceStr(Value.toString()); break;
				case eSignature:		ColValue.Formatted = CProgramRule::GetSignatureLevelStr((KPH_VERIFY_AUTHORITY)Value.toUInt()); break;
				case eOnSpawn:			ColValue.Formatted = CProgramRule::GetOnSpawnStr((EProgramOnSpawn)(Value.toUInt() >> 16)) + "/" + CProgramRule::GetOnSpawnStr((EProgramOnSpawn)(Value.toUInt() & 0xFFFF)); break;
				//case eProgram:			ColValue.Formatted = QString("%1 (%2)").arg(pRule->GetProgramPath()).arg(pRule->GetProgramNtPath()); break;
				case eProgram:			ColValue.Formatted = pRule->GetProgramPath(); break;
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

CProgramRuleModel::STreeNode* CProgramRuleModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

CProgramRulePtr CProgramRuleModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
		return CProgramRulePtr();

	SRuleNode* pNode = static_cast<SRuleNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pRule;
}

int CProgramRuleModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CProgramRuleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eEnabled:				return tr("Enabled");
		case eAction:				return tr("Action");
		case eSignature:			return tr("Signature");
		case eOnSpawn:				return tr("Trusted Spawn/UnTrusted");
		case eProgram:				return tr("Program");
		}
	}
	return QVariant();
}