#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Access/AccessManager.h"
#include "../Core/Programs/ProgramItem.h"


class CAccessRuleModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CAccessRuleModel(QObject* parent = 0);
	~CAccessRuleModel();

	typedef CAccessRulePtr ItemType;

	QList<QModelIndex>	Sync(const QList<CAccessRulePtr>& RuleList);

	CAccessRulePtr		GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eEnabled,
		eAction,
		ePath,
		eEnclave,
		eProgram,
		eCount
	};

protected:
	struct SRuleNode : STreeNode
	{
		SRuleNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		CAccessRulePtr pRule;
		CProgramItemPtr pProg;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SRuleNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
