#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Presets/Preset.h"


class CPresetsModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CPresetsModel(QObject* parent = 0);
	~CPresetsModel();

	QList<QModelIndex>	Sync(const QList<CPresetPtr>& PresetList);

	CPresetPtr			GetItem(const QModelIndex& index) const;
	QString				GetGuid(const QModelIndex& index) const;

	int					columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant			headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eDescription,
		eCount
	};

protected:
	struct SPresetNode : STreeNode
	{
		SPresetNode(const QVariant& Id) : STreeNode(Id) { }

		CPresetPtr pPreset;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SPresetNode(Id); }
};
