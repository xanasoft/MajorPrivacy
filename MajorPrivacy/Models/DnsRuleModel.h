#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Network/NetworkManager.h"
#include "../Core/Programs/ProgramItem.h"


class CDnsRuleModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CDnsRuleModel(QObject* parent = 0);
	~CDnsRuleModel();

	typedef CDnsRulePtr ItemType;

	QList<QModelIndex>	Sync(const QList<CDnsRulePtr>& RuleList);

	CDnsRulePtr		GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eHostName = 0,
		eEnabled,
		eHitCount,
		eAction,
		eEmpty,
		eCount
	};

protected:
	struct SRuleNode : STreeNode
	{
		SRuleNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		CDnsRulePtr pRule;
		//CProgramItemPtr pProg;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SRuleNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
