#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/ProcessList.h"
#include "../Core/Network/TrafficEntry.h"
#include "../Core/Programs/ProgramItem.h"

struct STrafficItem
{
	CTrafficEntryPtr pEntry;
	CProgramItemPtr pProg;
	QVariant Parent;
};

typedef QSharedPointer<STrafficItem> STrafficItemPtr;

class CTrafficModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CTrafficModel(QObject* parent = 0);
	~CTrafficModel();
	
	typedef STrafficItemPtr ItemType;

	QList<QModelIndex>	Sync(const QMap<quint64, STrafficItemPtr>& List);
	
	STrafficItemPtr	GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eLastActive,
		eUploaded,
		eDownloaded,
		eCount
	};

protected:
	struct STrafficNode : STreeNode
	{
		STrafficNode(CTreeItemModel* pModel, const QVariant& Id) : STreeNode(pModel, Id) { }

		STrafficItemPtr pItem;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new STrafficNode(this, Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
