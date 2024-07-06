#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Access/AccessEntry.h"

struct SAccessItem
{
	QMap<CProgramItemPtr, SAccessStatsPtr> Stats;
};

typedef QSharedPointer<SAccessItem> SAccessItemPtr;

class CAccessModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CAccessModel(QObject* parent = 0);
	~CAccessModel();
	
	typedef SAccessItemPtr ItemType;

	QList<QModelIndex>	Sync(const QMap<QString, SAccessItemPtr>& List);
	
	SAccessItemPtr	GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eCount
	};

protected:
	struct STrafficNode : STreeNode
	{
		STrafficNode(CTreeItemModel* pModel, const QVariant& Id) : STreeNode(pModel, Id), bRoot(false), bDevice(false) { }

		QString Name;
		bool bRoot;
		bool bDevice;
		SAccessItemPtr pItem;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new STrafficNode(this, Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
