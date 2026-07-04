#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Presets/Preset.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Presets/PresetManager.h"


class CPresetItemsModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CPresetItemsModel(QObject* parent = 0);
	~CPresetItemsModel();

	void					SetItems(const QMap<QFlexGuid, SItemPreset>& Items);
	void					SetOwnership(const QMap<QFlexGuid, SItemOwnerInfo>& Ownership) { m_Ownership = Ownership; }
	QList<QModelIndex>		Sync();

	QFlexGuid				GetItemGuid(const QModelIndex& index) const;
	SItemPreset				GetItem(const QModelIndex& index) const;

	int						columnCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant				headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eType,
		eAction,
		eStatus,
		eOwner,
		eDescription,
		eCount
	};

	static QColor GetActionColor(SItemPreset::EActivate Action);
	static QColor GetStatusColor(bool bEnabled, bool bUnknown);

protected:
	struct SItemNode : STreeNode
	{
		SItemNode(const QVariant& Id) : STreeNode(Id) { }

		QFlexGuid Guid;
		SItemPreset Item;
		CProgramItemPtr pProg;

		bool IsMissing = false;
	};

	virtual STreeNode*	MkNode(const QVariant& Id) { return new SItemNode(Id); }

	QMap<QFlexGuid, SItemPreset> m_Items;
	QMap<QFlexGuid, SItemOwnerInfo> m_Ownership;
};
