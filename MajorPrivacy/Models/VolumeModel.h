#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Volumes/VolumeManager.h"
#include "../Core/Volumes/Volume.h"


class CVolumeModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CVolumeModel(QObject* parent = 0);
	~CVolumeModel();
	
	typedef CVolumePtr ItemType;

	QList<QModelIndex>	Sync(const QList<CVolumePtr>& VolumeList);
	
	CVolumePtr		GetItem(const QModelIndex& index);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eStatus,
		eMountPoint,
		eTotalSize,
		eFullPath,
		eCount
	};

protected:
	struct SVolumeNode : STreeNode
	{
		SVolumeNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		CVolumePtr pVolume;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SVolumeNode(/*this,*/ Id); }
	virtual STreeNode*	MkVirtualNode(const QVariant& Id, STreeNode* pParent);
};
