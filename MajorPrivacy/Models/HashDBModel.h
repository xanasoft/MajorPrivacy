#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/HashDB/HashDB.h"
#include "../Core/Programs/ProgramItem.h"


struct SHashItem
{
	QVariant ID;
	QList<QVariant> Parents;
	QString Name;
	EHashType Type = EHashType::eUnknown;
	CHashPtr pEntry;
};

typedef QSharedPointer<SHashItem> SHashItemPtr;

typedef QPair<quint64, quint64> SHashItemKey;

class CHashDBModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CHashDBModel(QObject* parent = 0);
	~CHashDBModel();

	typedef SHashItemPtr ItemType;

	QList<QModelIndex>	Sync(const QMap<SHashItemKey, SHashItemPtr>& List);

	SHashItemPtr	GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eValue,
		eCount
	};

protected:
	struct SHashNode : STreeNode
	{
		SHashNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		SHashItemPtr pItem;
		//CProgramItemPtr pProg;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SHashNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
