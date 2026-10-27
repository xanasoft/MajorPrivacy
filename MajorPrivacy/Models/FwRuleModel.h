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

	// rules made from a template shown under it rather than next to it
	void			SetTemplateTree(bool bTree)	{ m_bTemplateTree = bTree; }
	bool			IsTemplateTree() const		{ return m_bTemplateTree; }

	CFwRulePtr		GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eGrouping,
		eIndex,
		eState,
		eSource,
		eEnabled,
		eAction,
		eDirection,
		eProfiles,
		eInterfaces,
		eProtocol,
		eRemoteAddress,
		eLocalAddress,
		eRemotePorts,
		eLocalPorts,
		eICMPOptions,
		eEdgeTraversal,
		eUsers,
		eHitCount,
		eProgram,
		eCount
	};


private slots:
	void OnSidResolved(const QByteArray& Sid, const QString& FullName);

protected:
	struct SRuleNode : STreeNode
	{
		SRuleNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		CFwRulePtr pRule;
		CProgramItemPtr pProg;

		bool IsMissing = false;
	};

	bool				m_bTemplateTree = false;

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SRuleNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
