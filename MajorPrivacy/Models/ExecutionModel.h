#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramFile.h"

struct SExecutionItem
{
	CProgramFilePtr				pProg1;
	CProgramFilePtr				pProg2;
	CProgramFile::SExecutionInfo Info;
	QVariant					Parent;
};

typedef QSharedPointer<SExecutionItem> SExecutionItemPtr;

typedef QPair<quint64, quint64> SExecutionKey;

class CExecutionModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CExecutionModel(QObject* parent = 0);
	~CExecutionModel();
	
	typedef SExecutionItemPtr ItemType;

	QList<QModelIndex>	Sync(const QMap<SExecutionKey, SExecutionItemPtr>& List);
	
	SExecutionItemPtr	GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eRole,
		eStatus,
		eTimeStamp,
		eProgram,
		eCount
	};

protected:
	struct SExecutionNode : STreeNode
	{
		SExecutionNode(CTreeItemModel* pModel, const QVariant& Id) : STreeNode(pModel, Id) { }

		SExecutionItemPtr pItem;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SExecutionNode(this, Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
