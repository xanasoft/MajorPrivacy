#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Network/NetworkManager.h"
#include "../Core/Programs/ProgramItem.h"


class CFwRuleModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CFwRuleModel(QObject* parent = 0);
	~CFwRuleModel();

	typedef CFwRulePtr ItemType;

	QList<QModelIndex>	Sync(const QList<CFwRulePtr>& RuleList);

	CFwRulePtr		GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eGrouping,
		eIndex,
		eStatus,
		eHitCount,
		eProfiles,
		eAction,
		eDirection,
		eProtocol,
		eRemoteAddress,
		eLocalAddress,
		eRemotePorts,
		eLocalPorts,
		eICMPOptions,
		eInterfaces,
		eEdgeTraversal,
		eProgram,
		eCount
	};

protected:
	struct SRuleNode : STreeNode
	{
		SRuleNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		CFwRulePtr pRule;
		CProgramItemPtr pProg;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SRuleNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
