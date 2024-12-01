#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Network/Socket.h"


class CSocketModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CSocketModel(QObject* parent = 0);
	~CSocketModel();
	
	typedef CSocketPtr ItemType;

	QList<QModelIndex>	Sync(const QMap<quint64, CSocketPtr>& SocketList);
	
	CSocketPtr		GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eProtocol,
		eRAddress,
		eRPort,
		eLAddress,
		eLPort,
		eAccess,
		eState,
		eLastActive,
		eUpload,
		eDownload,
		eUploaded,
		eDownloaded,
		eTimeStamp,
		eProgram,
		eCount
	};

protected:
	struct SSocketNode : STreeNode
	{
		SSocketNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		CSocketPtr pSocket;
		CProcessPtr pProcess;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SSocketNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
