#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/AbstractTreeModel.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Access/AccessEntry.h"

struct SAccessItem
{
	TRACK_OBJECT(SAccessItem)
	SAccessItem() {}
	virtual ~SAccessItem() {}
	QString Name;
	quint64 LastAccess = 0;
	quint64 AccessCount = 0;
	uint32 AccessMask = 0;
	QString Path;
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
	QHash<CProgramItemPtr, SAccessStatsPtr> Stats;
	quint64 OwnCount = 0; // part of AccessCount from Stats, the rest comes from Branches
	QHash<QString, QSharedPointer<SAccessItem>> Branches;

	mutable int FilterMatch = -1; // cached by CAccessModel::FilterAccepts, -1 to evaluate again
	mutable quint32 FilterGen = 0;
};

typedef QSharedPointer<SAccessItem> SAccessItemPtr;

class CAccessModel : public CAbstractTreeModel
{
	Q_OBJECT
public:
	CAccessModel(QObject* parent = nullptr);

	typedef SAccessItemPtr ItemType;

	void			Update(const SAccessItemPtr& pRoot);

	void			SetFilter(const QRegularExpression& Exp);
	bool			FilterAccepts(const SAccessItemPtr& pItem) const;

	SAccessItemPtr	GetItem(const QModelIndex& index);
	
	int				columnCount(const QModelIndex& parent = QModelIndex()) const { return eCount; }
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eLastAccess,
		eAccessCount,
		eCount
	};

protected:

	int				VisibleChildCount(const SAccessItemPtr& pItem) const;

	struct SAccessModelNode : SAbstractTreeNode
	{
		SAccessItemPtr data;
	};

	SAbstractTreeNode* MkNode(const void* data, const QVariant& key = QVariant(), SAbstractTreeNode* parent = nullptr) override;

	void ForEachChild(SAbstractTreeNode* parentNode, const std::function<void(const QVariant& key, const void* data)>& func) override;

	void updateNodeData(SAbstractTreeNode* node, const void* data, const QModelIndex& nodeIndex) override;

	const void* childData(SAbstractTreeNode* node, const QVariant& key) override;

	QVariant data(SAbstractTreeNode* node, int column, int role = Qt::DisplayRole) const override;

private:
	QRegularExpression m_FilterExp;
	quint32 m_FilterGen = 1;

	QIcon m_DeviceIcon;
	QIcon m_MonitorIcon;
	QIcon m_DiskIcon;
	QIcon m_FileIcon;
	QIcon m_DirectoryIcon;
	QIcon m_NetworkIcon;
	QIcon m_ObjectsIcon;
	QIcon m_RegEditIcon;
};

