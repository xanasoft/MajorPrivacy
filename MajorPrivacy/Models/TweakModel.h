#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Tweaks/Tweak.h"


class CTweakModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CTweakModel(QObject* parent = 0);
	~CTweakModel();

	typedef CTweakPtr ItemType;

	QList<QModelIndex> Sync(const CTweakPtr& pRoot, bool bShowNotAvailable = false);

	CTweakPtr		GetItem(const QModelIndex& index);

	QVariant		Data(const QModelIndex &index, int role, int section) const;

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eStatus,
		eType,
		eHint,
		eDescription,
		eCount
	};

protected:
	
	void Sync(const CTweakPtr& pTweak, bool bShowNotAvailable, const QList<QVariant>& Path, QMap<QList<QVariant>, QList<STreeNode*> >& New, QHash<QVariant, STreeNode*>& Old, QList<QVariant>& Added);

	struct STweakNode : STreeNode
	{
		STweakNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		ETweakStatus Status;
		CTweakPtr pTweak;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new STweakNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
