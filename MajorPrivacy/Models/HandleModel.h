#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Access/Handle.h"


class CHandleModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CHandleModel(QObject* parent = 0);
	~CHandleModel();
	
	typedef CHandlePtr ItemType;

	QList<QModelIndex>	Sync(const QList<CHandlePtr>& HandleList);
	
	CHandlePtr		GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eProgram,
		eCount
	};

protected:
	struct SHandleNode : STreeNode
	{
		SHandleNode(CTreeItemModel* pModel, const QVariant& Id) : STreeNode(pModel, Id) { }

		CHandlePtr pHandle;
		CProcessPtr pProcess;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SHandleNode(this, Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
