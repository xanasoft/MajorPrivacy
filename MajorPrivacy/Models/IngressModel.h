#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramFile.h"

struct SIngressItem
{
	CProgramFilePtr				pProg1;
	CProgramFilePtr				pProg2;
	CProgramFile::SIngressInfo	Info;
	QVariant					Parent;
};

typedef QSharedPointer<SIngressItem> SIngressItemPtr;

typedef QPair<quint64, quint64> SIngressKey;

class CIngressModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CIngressModel(QObject* parent = 0);
	~CIngressModel();
	
	typedef SIngressItemPtr ItemType;

	QList<QModelIndex>	Sync(const QMap<SIngressKey, SIngressItemPtr>& List);
	
	SIngressItemPtr	GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eRole,
		eStatus,
		eAccess,
		eTimeStamp,
		eProgram,
		eCount
	};

protected:
	struct SIngressNode : STreeNode
	{
		SIngressNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		SIngressItemPtr pItem;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SIngressNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
