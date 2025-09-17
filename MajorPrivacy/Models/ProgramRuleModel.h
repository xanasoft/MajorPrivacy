#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Enclaves/EnclaveManager.h"


class CProgramRuleModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CProgramRuleModel(QObject* parent = 0);
	~CProgramRuleModel();

	typedef CProgramRulePtr ItemType;

	QList<QModelIndex>	Sync(const QList<CProgramRulePtr>& RuleList);

	CProgramRulePtr		GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eEnabled,
		eAction,
		eSigners,
		eEnclave,
		eProgram,
		eCount
	};

protected:
	struct SRuleNode : STreeNode
	{
		SRuleNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		CProgramRulePtr pRule;
		CProgramItemPtr pProg;
		CEnclavePtr pEnclave;

		bool IsMissing = false;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SRuleNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
