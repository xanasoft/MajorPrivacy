#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Processes/ProcessList.h"
#include "AccessModel.h"

class CAccessListModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CAccessListModel(QObject* parent = 0);
	~CAccessListModel();
	
	typedef SAccessStatsPtr ItemType;

	QList<QModelIndex>	Sync(const QMap<CProgramItemPtr, QPair<quint64,QList<QPair<SAccessStatsPtr,SAccessItem::EType>>>>& Map);
	
	SAccessStatsPtr	GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eLastAccess,
		eAccess,
		eStatus,
		eCount
	};

protected:

	void Sync(const CProgramItemPtr& pItem, const QList<QPair<SAccessStatsPtr,SAccessItem::EType>>& List, TNewNodesMap& New, QHash<QVariant, STreeNode*>& Old);

	struct SAccessNode : STreeNode
	{
		SAccessNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		SAccessStatsPtr pItem;
		CProgramItemPtr pProgram;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SAccessNode(/*this,*/ Id); }
	//virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);

	QIcon m_DeviceIcon;
	QIcon m_MonitorIcon;
	QIcon m_DiskIcon;
	QIcon m_FileIcon;
	QIcon m_DirectoryIcon;
	QIcon m_NetworkIcon;
	QIcon m_ObjectsIcon;
	QIcon m_RegEditIcon;
};
