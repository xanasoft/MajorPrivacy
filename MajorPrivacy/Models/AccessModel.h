#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Access/AccessEntry.h"

#define USE_ACCESS_TREE

struct SAccessItem
{
	QString Name;
#ifndef USE_ACCESS_TREE
	QString Path;
#else
	quint64 LastAccess = 0;
#endif
	uint32 AccessMask = 0;
	QString NtPath;
	enum EType {
		eDevice = 0,
		eObjects,
		eComputer,
		eDrive,
		eFile,
		eFolder,
		eNetwork,
		eHost,
		eRegistry,
	} Type = eDevice;
	QMap<CProgramItemPtr, SAccessStatsPtr> Stats;
#ifdef USE_ACCESS_TREE
	QMap<QString, QSharedPointer<SAccessItem>> Branches;
#endif
};

typedef QSharedPointer<SAccessItem> SAccessItemPtr;

class CAccessModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CAccessModel(QObject* parent = 0);
	~CAccessModel();
	
	typedef SAccessItemPtr ItemType;

#ifndef USE_ACCESS_TREE
	QList<QModelIndex>	Sync(const QHash<QString, SAccessItemPtr>& List);
#else
	QList<QModelIndex>	Sync(const SAccessItemPtr& pRoot);
#endif
	
	SAccessItemPtr	GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eLastAccess,
		eCount
	};

protected:
	struct SAccessNode : STreeNode
	{
		SAccessNode(CTreeItemModel* pModel, const QVariant& Id) : STreeNode(pModel, Id) { }

		SAccessItemPtr pItem;
#ifdef USE_ACCESS_TREE
		quint64 LastAccess = -1;
#endif
	};

#ifdef USE_ACCESS_TREE
	void					Sync(SAccessNode* pParent, QMap<QList<QVariant>, QList<STreeNode*> >& New, QHash<QVariant, STreeNode*>& Old);
#endif

	QVariant NodeData(STreeNode* pNode, int role, int section) const;

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SAccessNode(this, Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
